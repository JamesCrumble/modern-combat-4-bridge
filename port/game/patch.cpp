/*
 * The licence, and the four pointers it locks.
 *
 * libModernCombat4.so exports lockPointer1..4 (0x1812f38..0x1812f44), the same
 * GDRM scheme as N.O.V.A. 3 and Wild Blood, where the engine's init stores
 * through them without checking. The text below is those ports'.
 *
 * On Android they are allocated by the installer's own native init - the
 * GameInstaller.initNative / GDRMPolicy.initNativeAP entry points, which the
 * Java installer activity calls before the game starts. That activity is a
 * licence check against a server that has been gone for years, so this port
 * does not run it. What it leaves behind is four null pointers that the engine
 * writes to without checking.
 *
 * Giving them storage is the whole fix, and it is deliberately the *smallest*
 * one: nothing here is patched, no branch is rewritten, no verdict is forged.
 * The engine writes its lock words into four words of ours and reads back what
 * it wrote.
 */

#include <stdint.h>
#include <stdio.h>

#include "so_util.h"
#include "trace.h"

#include "mc4.h"

/*
 * One word per pointer, and they are separate rather than an array because the
 * engine compares the addresses in at least one place ("lockPointer" appears
 * four times in the symbol table, not once with an index) - four words of one
 * array would still be four distinct addresses, but keeping them named makes a
 * later fault in one of them attributable to that one.
 */
static int g_lock_word[4];

void mc4_unlock_drm(so_module *mod)
{
    static const char *const kNames[4] = {
        "lockPointer1", "lockPointer2", "lockPointer3", "lockPointer4",
    };

    for (int i = 0; i < 4; i++) {
        int **slot = (int **)so_symbol(mod, kNames[i]);
        if (!slot) {
            trace("DRM: %s is not exported by this build - if init() faults on "
                  "a null store, this is why", kNames[i]);
            continue;
        }
        g_lock_word[i] = 0;
        *slot = &g_lock_word[i];
    }

    trace("DRM: the four GDRM lock words point at loader storage");
}
