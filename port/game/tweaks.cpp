/*
 * Engine tweaks that do not depend on offsets inside the library.
 *
 *   <PREFIX>_FAST_CAS=0   turn off the atomics fix below (on by default). A library built for ARMv5
 *                         calls the kernel's cmpxchg helper at 0xffff0fc0 for its __sync_* functions,
 *                         which qemu-user emulates by trapping out of translated code on every call.
 *                         Each literal holding that address is pointed at a small ldrex/strex routine.
 *   <PREFIX>_SLEEP_TRACE=1   who calls glf::Thread::Sleep, see sleep_trace_hook.
 *   <PREFIX>_MONEY_INFINITE=1, <PREFIX>_MONEY_RESTORE=N   the credits balance, see install_money.
 *   <PREFIX>_UCLAMP=0     leave the scheduler's clamp on the main thread alone (see mc4_sched_tick).
 *   <PREFIX>_UCLAMP_MIN=0..1024 also ask for at least this much (0 by default).
 */
#include <errno.h>
#include <pthread.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>

#include "so_util.h"
#include "trace.h"
#include "port_env.h"

#include "mc4.h"

static void install_fast_cas(so_module *mod)
{
    /* kuser cmpxchg: r0 = old, r1 = new, r2 = ptr; returns r0 = 0 when it stored. */
    static const uint32_t code[] = {
        0xf57ff05b, /* dmb ish            */
        0xe1923f9f, /* 1: ldrex r3,[r2]   */
        0xe0533000, /* subs r3,r3,r0      */
        0x01823f91, /* strexeq r3,r1,[r2] */
        0x03330001, /* teqeq r3,#1        */
        0x0afffffa, /* beq 1b             */
        0xf57ff05b, /* dmb ish            */
        0xe1a00003, /* mov r0,r3          */
        0xe12fff1e, /* bx lr              */
    };
    void *page = mmap(NULL, 4096, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (page == MAP_FAILED) {
        trace("tweaks: fast cas: mmap failed");
        return;
    }
    memcpy(page, code, sizeof(code));
    uint32_t *w = (uint32_t *)mod->text_base;
    size_t n = mod->text_size / 4, hits = 0;
    for (size_t i = 0; i < n; i++) {
        if (w[i] == 0xffff0fc0u) {
            w[i] = (uint32_t)(uintptr_t)page;
            hits++;
        }
    }
    trace("tweaks: kuser cmpxchg literal replaced at %zu places (routine %p)", hits, page);
}

/*
 * <PREFIX>_SLEEP_TRACE=1: who calls glf::Thread::Sleep, for how long and from which thread.
 * The function is three instructions (ms * 1000, tail call to usleep), so it is replaced outright.
 * One line per distinct call site, then a count line every 600 calls.
 */
static so_module *g_trace_mod;

static void sleep_trace(uintptr_t lr, unsigned ms)
{
    static struct { uintptr_t lr; unsigned calls; unsigned long total_ms; } sites[32];
    static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
    static unsigned long calls;
    lr -= g_trace_mod->text_base;

    pthread_mutex_lock(&lock);
    int slot = -1;
    for (int i = 0; i < 32; i++) {
        if (sites[i].lr == lr || !sites[i].lr) {
            slot = i;
            break;
        }
    }
    if (slot >= 0) {
        if (!sites[slot].lr) {
            sites[slot].lr = lr;
            trace("sleep trace: new call site +0x%lx: %u ms, thread %ld%s", (unsigned long)lr, ms,
                  (long)syscall(SYS_gettid), syscall(SYS_gettid) == getpid() ? " (main)" : "");
        }
        sites[slot].calls++;
        sites[slot].total_ms += ms;
    }
    if (++calls % 600 == 0) {
        for (int i = 0; i < 32 && sites[i].lr; i++) {
            if (sites[i].calls)
                trace("sleep trace: +0x%lx: %u calls, %lu ms in total", (unsigned long)sites[i].lr,
                      sites[i].calls, sites[i].total_ms);
            sites[i].calls = 0;
            sites[i].total_ms = 0;
        }
    }
    pthread_mutex_unlock(&lock);
    usleep(ms * 1000u);
}

static void sleep_trace_hook(unsigned ms)
{
    sleep_trace((uintptr_t)__builtin_return_address(0), ms);
}

/* glf::Thread::_Sleep(ms): the same three instructions as a member function. */
static void sleep_trace_hook_member(void *self, unsigned ms)
{
    (void)self;
    sleep_trace((uintptr_t)__builtin_return_address(0), ms);
}

/*
 * <PREFIX>_MONEY_INFINITE=1: the player's credits read as kMoneyInfinite and spending takes nothing.
 * <PREFIX>_MONEY_RESTORE=N:  once the player profile is there, the credits are set to N (the infinite
 *                            option is off, or its profile is left alone).
 *
 * Both exports are short and start with a push, so the first words are overwritten outright:
 *
 *     PlayerProfileBase::GetTotalCredits() const   ->  ldr r0, [pc, #0] ; bx lr ; .word kMoneyInfinite
 *     PlayerProfileBase::RemoveCredits(int)        ->  bx lr
 *
 * Nothing else changes the stored balance: AddCredits still adds to the real one (the token at
 * profile + 156), so switching the option off gives back what the player had plus what was earned.
 * Of the 63 callers of the getter and 12 of RemoveCredits none reads the token directly (the one place
 * that writes it is the profile's own set-up).
 *
 * MONEY_RESTORE writes the real balance through the token's own operator= and marks the profile
 * changed (the byte at +200 that AddCredits sets), a little after the profile first answers as valid so
 * that a load in progress does not overwrite it.
 */
static const int kMoneyInfinite = 9999999;

static void *(*g_settings_instance)(void);
static void *(*g_active_profile)(void *);
static void *(*g_token_assign)(void *, const int *);
static int  (*g_token_get)(const void *);
static long g_money_restore = -1;

static void install_money(so_module *mod)
{
    long restore = port_getenv_long("MONEY_RESTORE", -1);
    bool infinite = port_getenv_long("MONEY_INFINITE", 0) != 0;
    if (!infinite && restore < 0)
        return;

    if (infinite) {
        uint32_t *get = (uint32_t *)so_symbol(mod, "_ZNK17PlayerProfileBase15GetTotalCreditsEv");
        uint32_t *rem = (uint32_t *)so_symbol(mod, "_ZN17PlayerProfileBase13RemoveCreditsEi");
        /* RemoveCredits starts with a push in both builds (r4-r7 in 1.2.3e, r4-r6 in 1.2.1a). */
        if (get && rem && get[0] == 0xe92d4010u && (rem[0] & 0xffff0000u) == 0xe92d0000u) {
            get[0] = 0xe59f0000u;           /* ldr r0, [pc, #0]  (the word after the next one) */
            get[1] = 0xe12fff1eu;           /* bx lr */
            get[2] = (uint32_t)kMoneyInfinite;
            rem[0] = 0xe12fff1eu;           /* bx lr */
            __builtin___clear_cache((char *)get, (char *)(get + 3));
            __builtin___clear_cache((char *)rem, (char *)(rem + 1));
            trace("tweaks: money: infinite (%d), spending takes nothing", kMoneyInfinite);
        } else {
            trace("tweaks: money: GetTotalCredits/RemoveCredits not as in 1.2.1a/1.2.3e - infinite money left off");
        }
        return;
    }

    g_settings_instance = (void *(*)(void))so_symbol(mod, "_ZN12GameSettings11GetInstanceEv");
    g_active_profile = (void *(*)(void *))so_symbol(mod, "_ZN12GameSettings16GetPlayerProfileEv");
    g_token_assign = (void *(*)(void *, const int *))so_symbol(mod, "_ZN19ObfuscatedDataTokenaSERKi");
    g_token_get = (int (*)(const void *))so_symbol(mod, "_ZNK19ObfuscatedDataTokencviEv");
    if (!g_settings_instance || !g_active_profile || !g_token_assign || !g_token_get) {
        trace("tweaks: money: restore needs GameSettings/ObfuscatedDataToken exports - not found, left alone");
        return;
    }
    g_money_restore = restore > 99999999 ? 99999999 : restore;
    trace("tweaks: money: credits will be set to %ld once the profile is there", g_money_restore);
}

static void money_tick(long frames)
{
    static long ready_at = -1;
    if (g_money_restore < 0 || frames % 30 != 0)
        return;
    void *settings = g_settings_instance();
    if (!settings)
        return;
    char *profile = (char *)g_active_profile(settings);
    if (!profile)
        return;
    /* The same two virtual calls the getter makes first; the second one's answer says the profile is valid. */
    void **vt = *(void ***)profile;
    ((void (*)(void *))vt[10])(profile);
    if (!((int (*)(void *))vt[10])(profile)) {
        ready_at = -1;
        return;
    }
    if (ready_at < 0)
        ready_at = frames;
    if (frames - ready_at < 300)
        return;
    int before = g_token_get(profile + 156);
    int value = (int)g_money_restore;
    g_token_assign(profile + 156, &value);
    profile[200] = 1;
    trace("tweaks: money: credits %d replaced by %d", before, value);
    g_money_restore = -1;
}

void mc4_tweaks_install(so_module *mod)
{
    if (port_getenv_long("FAST_CAS", 1))
        install_fast_cas(mod);

    install_money(mod);

    if (port_getenv_long("SLEEP_TRACE", 0)) {
        g_trace_mod = mod;
        if (so_symbol(mod, "_ZN3glf6Thread5SleepEj")) {
            hook_symbol(mod, "_ZN3glf6Thread5SleepEj", (uintptr_t)sleep_trace_hook, 1);
            trace("tweaks: glf::Thread::Sleep is traced");
            if (so_symbol(mod, "_ZN3glf6Thread6_SleepEj"))
                hook_symbol(mod, "_ZN3glf6Thread6_SleepEj", (uintptr_t)sleep_trace_hook_member, 1);
        } else {
            trace("tweaks: no glf::Thread::Sleep exported by this library - not traced");
        }
    }
}

/*
 * The phone's scheduler puts a utilisation clamp on the game's main thread a few seconds into a
 * mission: uclamp.max 171-199 of 1024, on that one thread and no other. To the frequency governor the
 * thread then never looks more than a fifth loaded, and the core stays at its lowest clock while the
 * thread is in fact saturated (util 1023, 1.0 GHz of 4.3). The clamp is a per-task attribute its owner
 * may set, so the main thread sets it back - again and again, because whatever lowers it does so
 * repeatedly.
 *
 * sched_setattr is not something Android apps normally call, and a system call the seccomp filter
 * does not know kills the process, so the first call is made in a forked child.
 */
struct mc4_sched_attr {
    uint32_t size, sched_policy;
    uint64_t sched_flags;
    int32_t  sched_nice;
    uint32_t sched_priority;
    uint64_t sched_runtime, sched_deadline, sched_period;
    uint32_t sched_util_min, sched_util_max;
};

static int set_uclamp(uint32_t lo)
{
    mc4_sched_attr a;
    memset(&a, 0, sizeof(a));
    a.size = sizeof(a);
    a.sched_flags = 0x08 /* KEEP_POLICY */ | 0x10 /* KEEP_PARAMS */ | 0x20 /* UTIL_CLAMP_MIN */ | 0x40 /* UTIL_CLAMP_MAX */;
    a.sched_util_min = lo;
    a.sched_util_max = 1024;
    return syscall(SYS_sched_setattr, 0, &a, 0) == 0 ? 0 : errno;
}

void mc4_sched_tick(long frames)
{
    money_tick(frames);

    static int state = -1;      /* -1 not tried, 0 off, 1 working */
    static uint32_t lo;

    if (state == 0 || (state == 1 && frames % 8 != 0))
        return;
    if (state < 0) {
        state = 0;
        if (!port_getenv_long("UCLAMP", 1))
            return;
        long v = port_getenv_long("UCLAMP_MIN", 0);
        lo = v < 0 ? 0 : v > 1024 ? 1024 : (uint32_t)v;
        pid_t child = fork();
        if (child < 0)
            return;
        if (child == 0)
            _exit(set_uclamp(lo) & 0x7f);
        int st = 0;
        if (waitpid(child, &st, 0) != child || !WIFEXITED(st) || WEXITSTATUS(st) != 0) {
            trace("tweaks: uclamp: sched_setattr unusable here (%s %d) - left alone",
                  WIFEXITED(st) ? "errno" : "signal", WIFEXITED(st) ? WEXITSTATUS(st) : WTERMSIG(st));
            return;
        }
        state = 1;
        trace("tweaks: uclamp: main thread kept at min %u, max 1024", lo);
    }
    int e = set_uclamp(lo);
    if (e) {
        trace("tweaks: uclamp: sched_setattr failed (errno %d) - stopped", e);
        state = 0;
    }
}
