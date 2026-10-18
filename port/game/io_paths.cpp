/*
 * Path translation for Modern Combat 4 (1.1.6, Amazon build GloftM4HM).
 *
 * See portbase/src/io_paths.h for the split between what portbase answers and
 * what only this game knows. The names below are in the library's .rodata
 * (`strings -a -t x libModernCombat4.so`):
 *
 *   a3c1c8  /data/data/com.gameloft.android.ANMP.GloftM4HM
 *   a3c218  /sdcard/gameloft/games/GloftM4HM
 *   aa4a34  resourcePath: %s / homePath: %s / tempPath: %s
 *
 * The three directories are not discovered by the engine: Java hands them over
 * through setPaths(resource, home, temp), and this port is that Java
 * (GL2JNILib.setupPaths: the "SDFolder" preference, getFilesDir, getCacheDir).
 * The rewrite rules here are a safety net for the names the engine builds on
 * its own.
 *
 * There are no expansion files: the game data is a tree of flat files
 * (data/*.gla, textures/*.gla), the "cache" that goes to
 * /sdcard/Android/data/<package>/files on a phone. Here it is <game>/files.
 * What the game writes (saves, preferences) goes to io_writable_dir().
 */

#include <dirent.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include "io_paths.h"
#include "so_util.h"
#include "trace.h"

#include "jni.h"
#include "jni_internals.h"

#include "mc4.h"

/* The package, in the spellings the binary and its Java use. */
static const char kDataData[] = "/data/data/com.gameloft.android.ANMP.GloftM4HM";
static const char kExtFiles[] = "/sdcard/Android/data/com.gameloft.android.ANMP.GloftM4HM/files";
static const char kSdFolder[] = "/sdcard/gameloft/games/GloftM4HM";

static const char kObbDir[]   = "/sdcard/Android/obb/com.gameloft.android.ANMP.GloftM4HM";
static const char kMainObb[]  = "main.1100.com.gameloft.android.ANMP.GloftM4HM.obb";
static const char kPatchObb[] = "patch.12222.com.gameloft.android.ANMP.GloftM4HM.obb";

static const char *obb_dir(void)
{
    static char dir[PATH_MAX];
    if (!dir[0])
        snprintf(dir, sizeof(dir), "%s/obb", io_game_dir());
    return dir;
}

/*
 * The patch file's name carries the game's build number (12222 for 1.2.3e, 11764 for 1.2.1a), so it is
 * whichever patch.*.obb the wrapper linked into the obb folder; kPatchObb is only the answer for a folder
 * that has none.
 */
static const char *patch_obb_name(void)
{
    static char name[NAME_MAX + 1];
    if (!name[0]) {
        snprintf(name, sizeof(name), "%s", kPatchObb);
        if (DIR *d = opendir(obb_dir())) {
            /* readdir64: the 32-bit readdir fails with EOVERFLOW on a filesystem with 64-bit inode numbers. */
            while (struct dirent64 *e = readdir64(d)) {
                size_t n = strlen(e->d_name);
                if (strncmp(e->d_name, "patch.", 6) == 0 && n > 10 && strcmp(e->d_name + n - 4, ".obb") == 0) {
                    snprintf(name, sizeof(name), "%s", e->d_name);
                    break;
                }
            }
            closedir(d);
        }
        trace("obb: patch file %s", name);
    }
    return name;
}

const char *mc4_obb_path(bool patch)
{
    static char path[2][PATH_MAX];
    char *p = path[patch ? 1 : 0];
    if (!p[0])
        snprintf(p, PATH_MAX, "%s/%s", obb_dir(), patch ? patch_obb_name() : kMainObb);
    return p;
}

const char *mc4_resource_dir(void)
{
    static char dir[PATH_MAX];
    if (!dir[0])
        snprintf(dir, sizeof(dir), "%s/files", io_game_dir());
    return dir;
}

/*
 * Re-root `path` at `root`, keeping whatever followed `prefix`.
 *
 * The tail can be empty (the game asks for the bare directory when it builds
 * its own paths by concatenation) and it can start with or without a slash, so
 * the separator is normalised here rather than at each call site.
 */
static const char *reroot(const char *path, const char *prefix, const char *root,
                          char *buf, size_t bufsz)
{
    const char *tail = path + strlen(prefix);
    while (*tail == '/')
        tail++;

    if (*tail)
        snprintf(buf, bufsz, "%s/%s", root, tail);
    else
        snprintf(buf, bufsz, "%s", root);

    return buf;
}

static bool has_prefix(const char *path, const char *prefix)
{
    size_t n = strlen(prefix);
    return strncmp(path, prefix, n) == 0 && (path[n] == '/' || path[n] == '\0');
}

const char *port_fix_path(const char *orig, char *buf, size_t bufsz)
{
    if (!orig || !*orig)
        return NULL;

    /*
     * glf's file helpers build some names as sprintf(".%s", path), so a path
     * this port handed the engine through setPaths can come back with a "."
     * glued to the front, which makes it a relative name. A leading "./" in
     * front of one of the roots this port knows can only have come from that
     * concatenation, so the dot is dropped - and only then: "./data/foo" is a
     * relative name the engine means literally.
     */
    bool undotted_here = false;

    /*
     * And the same for a whole directory glued in front: the engine builds
     * "<resource>/" + "<home>/LogFilters.dat" and "./" + "/<obb>/main...obb",
     * which leaves a "//" where the second, absolute name begins. The last
     * place where an absolute root this port knows starts is the name meant.
     */
    {
        const char *roots[] = {
            io_game_dir(), io_writable_dir(), mc4_resource_dir(), obb_dir(),
            kObbDir, kExtFiles, kSdFolder, kDataData,
        };
        const char *best = NULL;
        for (const char *p = orig; *p; p++) {
            if (p[0] != '/' || (p != orig && p[-1] == '/' && p[1] == '/'))
                continue;
            if (p == orig)
                continue;
            if (p[-1] != '/' && !(p == orig + 1 && orig[0] == '.'))
                continue;
            for (unsigned i = 0; i < sizeof(roots) / sizeof(roots[0]); i++) {
                if (roots[i] && *roots[i] && has_prefix(p, roots[i])) {
                    best = p;
                    break;
                }
            }
        }
        if (best) {
            orig = best;
            undotted_here = true;
        }
    }

    if (has_prefix(orig, kObbDir))
        return reroot(orig, kObbDir, obb_dir(), buf, bufsz);

    if (has_prefix(orig, kExtFiles))
        return reroot(orig, kExtFiles, mc4_resource_dir(), buf, bufsz);

    if (has_prefix(orig, kSdFolder))
        return reroot(orig, kSdFolder, mc4_resource_dir(), buf, bufsz);

    if (has_prefix(orig, kDataData))
        return reroot(orig, kDataData, io_writable_dir(), buf, bufsz);

    /* An undotted host path is already the answer; returning NULL would give
     * portbase the dotted original and it would resolve it against the game
     * directory again. */
    if (undotted_here) {
        snprintf(buf, bufsz, "%s", orig);
        return buf;
    }

    /* No rule. NULL, not `orig`: portbase's generic fallbacks resolve the bare
     * relative names against the game tree. */
    return NULL;
}

/* The fonts the game uses are its own .ttf files under data/. */
const char *port_system_font(void)
{
    return NULL;
}

/*
 * The directories the engine writes into without ever creating them. On
 * Android GL2JNILib.setupPaths() calls ensurePathExists() on all three.
 */
void mc4_prepare_writable_storage(void)
{
    static const char *const kSubdirs[] = { "home", "cache" };
    char path[PATH_MAX];

    for (unsigned i = 0; i < sizeof(kSubdirs) / sizeof(kSubdirs[0]); i++) {
        snprintf(path, sizeof(path), "%s/%s", io_writable_dir(), kSubdirs[i]);
        if (mkdir(path, 0777) == 0)
            trace("writable storage: created %s", path);
    }
}

/*
 * setPaths, called once.
 *
 * The engine reaches back into Java for setupPaths() from inside
 * GL2JNILib.init(), and the fake class answers by coming here. main() does not
 * call it; the guard keeps a second callback from repeating it.
 */
void mc4_set_paths(_JNIEnv *env)
{
    static bool done = false;
    if (done)
        return;
    done = true;

    so_module *mod = mc4_module();
    if (!mod) {
        trace("setPaths skipped: the module is not loaded yet");
        done = false;
        return;
    }

    auto setPaths = (void (*)(JNIEnv *, jclass, jstring, jstring, jstring))
        so_symbol(mod, MC4_GL2JNILIB("setPaths"));
    if (!setPaths) {
        trace("setPaths: the module exports no GL2JNILib.setPaths");
        return;
    }

    char home[PATH_MAX], temp[PATH_MAX];
    snprintf(home, sizeof(home), "%s/home",  io_writable_dir());
    snprintf(temp, sizeof(temp), "%s/cache", io_writable_dir());

    JNIEnv *e = (JNIEnv *)env;
    jstring res = e->NewStringUTF(mc4_resource_dir());
    jstring hom = e->NewStringUTF(home);
    jstring tmp = e->NewStringUTF(temp);

    trace("setPaths resource='%s' home='%s' temp='%s'", mc4_resource_dir(), home, temp);

    setPaths(e, NULL, res, hom, tmp);
    trace("GL2JNILib.setPaths returned");
}
