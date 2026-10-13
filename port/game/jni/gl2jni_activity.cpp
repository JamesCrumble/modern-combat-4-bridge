/*
 * com/gameloft/glf/GL2JNILib and <package>/MC4 - the Java half of this game -
 * plus the small classes under the game's own package.
 *
 * The engine looks its callbacks up by name on two classes: the glf ones on
 * the class GL2JNILib.init() is handed, the game's own on <package>/MC4 (the
 * activity; MC4.nativeMC4Init() is given the instance). The names are in the
 * library's .rodata, the descriptors are the ones classes.dex declares.
 *
 * Almost everything answers with the quietest defensible constant and comes
 * from tools/gen_stubs.sh (glf_gen.inc, game_gen.inc). The ones written here
 * do real work:
 *
 *   setupPaths               hands the engine its three directories (mc4_set_paths)
 *   SwapBuffer               the engine presents its own frames (mc4_present)
 *   GetWindowWidth/Height    the surface this port drives
 *   getMainFileLocation,
 *   getPatchFileLocation     the two expansion files, which the engine reads itself
 *   getGeneration            the device class the Java picks; 3 is what the
 *                            "Ultra" APK answers for every device
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <time.h>

#include <map>
#include <string>

#include "platform.h"
#include "jni.h"
#include "jni_internals.h"
#include "app_exit.h"
#include "io_paths.h"
#include "port_env.h"
#include "so_util.h"
#include "trace.h"

#include "mc4.h"
#include "mc4_classes.h"

#define MC4_PACKAGE "com/gameloft/android/ANMP/GloftM4HM"

GloftGL2JNIActivity g_activity;

/* The surface, so GetWindowWidth/Height agree with the resize main.cpp performs. */
static int g_width  = 640;
static int g_height = 480;

void mc4_set_view_size(int w, int h)
{
    g_width  = w;
    g_height = h;
}

static const char *cstr(jstring s)
{
    return s ? ((String *)s)->str : "";
}

/* ------------------------------------------------------------ the paths */

static void Lib_setupPaths(JNIEnv *env, jclass clazz)
{
    (void)clazz;
    mc4_set_paths(env);
}

/*
 * A file out of the APK's assets/ (AssetManager.open on Android). The wrapper
 * unpacks that folder next to the library, so it is <game>/assets here. A
 * leading "." is dropped, as the game's Java does. Empty and not NULL when the
 * file is missing: the caller passes what it gets straight to GetArrayLength.
 */
static jbyteArray read_asset(JNIEnv *env, const char *name)
{
    if (name[0] == '.')
        name++;
    while (name[0] == '/')
        name++;
    char path[PATH_MAX];
    snprintf(path, sizeof(path), "%s/assets/%s", io_game_dir(), name);
    FILE *f = strstr(name, "..") ? NULL : fopen(path, "rb");
    if (!f) {
        trace("asset '%s' not found (%s) - answered empty", name, path);
        return env->NewByteArray(0);
    }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    std::string data((size_t)(size > 0 ? size : 0), '\0');
    size_t got = data.empty() ? 0 : fread(&data[0], 1, data.size(), f);
    fclose(f);
    jbyteArray out = env->NewByteArray((jsize)got);
    if (got)
        env->SetByteArrayRegion(out, 0, (jsize)got, (const jbyte *)data.data());
    trace("asset '%s': %zu bytes", name, got);
    return out;
}

static jbyteArray Lib_getResource(JNIEnv *env, jclass clazz, jstring name)
{
    (void)clazz;
    return read_asset(env, cstr(name));
}

/* ------------------------------------------------------------- the view */

static void Lib_createView(JNIEnv *env, jclass clazz)
{
    (void)env; (void)clazz;
    trace("createView - the loader already owns the window");
}

static void Lib_setViewSettings(JNIEnv *env, jclass clazz,
                                jint pixel, jint depth, jint stencil,
                                jint csaa, jint extra)
{
    (void)env; (void)clazz;
    trace("setViewSettings(pixel=%d depth=%d stencil=%d csaa=%d extra=%d)",
          pixel, depth, stencil, csaa, extra);
}

static jboolean Lib_setCurrentContext(JNIEnv *env, jclass clazz, jint index)
{
    (void)env; (void)clazz;
    /* One context; anything else is refused rather than silently accepted. */
    return index == 0 ? JNI_TRUE : JNI_FALSE;
}

static jint Lib_GetWindowWidth(JNIEnv *e, jclass c)  { (void)e; (void)c; return g_width;  }
static jint Lib_GetWindowHeight(JNIEnv *e, jclass c) { (void)e; (void)c; return g_height; }

/* ----------------------------------------------------------- the device */

static jobject Lib_GetManufacturer(JNIEnv *env, jclass c)   { (void)c; return (jobject)env->NewStringUTF("samsung"); }
static jobject Lib_GetDeviceName(JNIEnv *env, jclass c)     { (void)c; return (jobject)env->NewStringUTF("GT-I9300"); }
static jobject Lib_GetDeviceFirmware(JNIEnv *env, jclass c) { (void)c; return (jobject)env->NewStringUTF("4.1.2"); }

static jobject Lib_getDeviceLanguage(JNIEnv *env, jclass clazz)
{
    (void)clazz;
    /* <PREFIX>_LANG lets the wrapper pass the phone's language; "en" otherwise. */
    const char *lang = getenv("MC4_LANG");
    return (jobject)env->NewStringUTF(lang && *lang ? lang : "en");
}

static void Lib_exitGame(JNIEnv *env, jclass clazz)
{
    (void)env; (void)clazz;
    /* Recorded and consumed by the frame loop; see portbase/android/app_exit.h. */
    android_app_request_exit("the game called GL2JNILib.exitGame()");
}

/* The engine presents its own frames: the game's GLSurfaceView skips its swap
 * (GL2JNILib.g is true) and GL2JNILib.SwapBuffer() does eglSwapBuffers. */
static void Lib_SwapBuffer(JNIEnv *env, jclass clazz)
{
    (void)env; (void)clazz;
    mc4_present();
}

/* Says once that a generated stub was reached: the first thing to look at when
 * the game behaves as if Java had told it something wrong. */
#define GEN_SAID(what)                                  \
    do {                                                \
        static bool said;                               \
        if (!said) {                                    \
            said = true;                                \
            trace("java stub answered: %s", what);      \
        }                                               \
    } while (0)

#define STR "Ljava/lang/String;"
#define M(fn, name, sig) ManagedMethod::RegisterStatic<&fn>(CLS::clazz, name, sig)

#include "glf_gen.inc"

#define MC4_GLF_METHODS                                                         \
    M(Lib_getResource,         "getResource",             "(" STR ")[B"),       \
    M(Lib_setupPaths,          "setupPaths",              "()V"),               \
    M(Lib_createView,          "createView",              "()V"),               \
    M(Lib_setViewSettings,     "setViewSettings",         "(IIIII)V"),          \
    M(Lib_setCurrentContext,   "setCurrentContext",       "(I)Z"),              \
    M(Lib_GetWindowWidth,      "GetWindowWidth",          "()I"),               \
    M(Lib_GetWindowHeight,     "GetWindowHeight",         "()I"),               \
    M(Lib_GetManufacturer,     "GetManufacturer",         "()" STR),            \
    M(Lib_GetDeviceName,       "GetDeviceName",           "()" STR),            \
    M(Lib_GetDeviceFirmware,   "GetDeviceFirmware",       "()" STR),            \
    M(Lib_exitGame,            "exitGame",                "()V"),               \
    M(Lib_SwapBuffer,          "SwapBuffer",              "()V"),               \
    M(Lib_getDeviceLanguage,   "getDeviceLanguage",       "()" STR),            \
    GLF_GEN_METHODS                                                             \
    {NULL}

#define CLS GloftGL2JNIActivity
const ManagedMethod gl2jniActivityMethods[] = { MC4_GLF_METHODS };
#undef CLS
#define CLS GloftGL2JNILib
const ManagedMethod gl2jniLibMethods[] = { MC4_GLF_METHODS };
#undef CLS

Class GloftGL2JNIActivity::clazz = {
    .classpath       = "com/gameloft/glf/GL2JNIActivity",
    .classname       = "GL2JNIActivity",
    .managed_methods = gl2jniActivityMethods,
    .native_methods  = {NULL},
    .fields          = {NULL},
    .instance_size   = sizeof(GloftGL2JNIActivity),
};

Class GloftGL2JNILib::clazz = {
    .classpath       = "com/gameloft/glf/GL2JNILib",
    .classname       = "GL2JNILib",
    .managed_methods = gl2jniLibMethods,
    .native_methods  = {NULL},
    .fields          = {NULL},
    .instance_size   = sizeof(GloftGL2JNILib),
};

/* ---------------------------------------------------------------- MC4 */

GloftMC4 g_game_activity;

static jobject Game_getMainFileLocation(JNIEnv *env, jclass c)  { (void)c; return (jobject)env->NewStringUTF(mc4_obb_path(false)); }
static jobject Game_getPatchFileLocation(JNIEnv *env, jclass c) { (void)c; return (jobject)env->NewStringUTF(mc4_obb_path(true)); }
static jobject Game_getSDFolder(JNIEnv *env, jclass c)          { (void)c; return (jobject)env->NewStringUTF(mc4_resource_dir()); }

/*
 * The device class. The game's Java works it out from the model name (1 and 2
 * for a list of 2011-2012 phones, 9 for everything it does not know); the three
 * repacked APKs differ in exactly this method: "low" answers 0, "medium" 1 and
 * "Ultra" 3 for every device. 3 here, <PREFIX>_GENERATION to try the others.
 */
static jint Game_getGeneration(JNIEnv *e, jclass c) { (void)e; (void)c; return (jint)port_getenv_long("GENERATION", 3); }

/* /proc/cpuinfo's "CPU implementer" and "CPU part" in the Java: an ARM Cortex-A9. */
static jint Game_getArchitecture(JNIEnv *e, jclass c)      { (void)e; (void)c; return 0x41; }
static jint Game_getMicroArchitecture(JNIEnv *e, jclass c) { (void)e; (void)c; return 0xc09; }
static jint Game_getProcessorNum(JNIEnv *e, jclass c)      { (void)e; (void)c; return 4; }
/* Build.VERSION.SDK_INT: Android 4.1. */
static jint Game_getBuildVersion(JNIEnv *e, jclass c)      { (void)e; (void)c; return 16; }
static jobject Game_getCountry(JNIEnv *env, jclass c)      { (void)c; return (jobject)env->NewStringUTF("US"); }
static jobject Game_getUserAgent(JNIEnv *env, jclass c)    { (void)c; return (jobject)env->NewStringUTF("Mozilla/5.0 (Linux; Android 4.1.2)"); }
/* AudioManager's music stream volume, 0..15. */
static jint Game_getMasterVolume(JNIEnv *e, jclass c)      { (void)e; (void)c; return 15; }

#include "game_gen.inc"

#define CLS GloftMC4
const ManagedMethod gloftMC4Methods[] = {
    M(Game_getMainFileLocation,  "getMainFileLocation",  "()" STR),
    M(Game_getPatchFileLocation, "getPatchFileLocation", "()" STR),
    M(Game_getSDFolder,          "getSDFolder",          "()" STR),
    M(Game_getGeneration,        "getGeneration",        "()I"),
    M(Game_getArchitecture,      "getArchitecture",      "()I"),
    M(Game_getMicroArchitecture, "getMicroArchitecture", "()I"),
    M(Game_getProcessorNum,      "getProcessorNum",      "()I"),
    M(Game_getBuildVersion,      "getBuildVersion",      "()I"),
    M(Game_getCountry,           "getCountry",           "()" STR),
    M(Game_getUserAgent,         "getUserAgent",         "()" STR),
    M(Game_getMasterVolume,      "getMasterVolume",      "()I"),
    GAME_GEN_METHODS
    {NULL},
};
#undef CLS

Class GloftMC4::clazz = {
    .classpath       = MC4_PACKAGE "/MC4",
    .classname       = "MC4",
    .managed_methods = gloftMC4Methods,
    .native_methods  = {NULL},
    .fields          = {NULL},
    .instance_size   = sizeof(GloftMC4),
};

/* ------------------------------------------------------------- Device */

static jobject Device_getUserAgent(JNIEnv *env, jclass c) { (void)c; return (jobject)env->NewStringUTF("Mozilla/5.0 (Linux; Android 4.1.2)"); }
static jobject Device_getAndroidId(JNIEnv *env, jclass c) { (void)c; return (jobject)env->NewStringUTF("mc40000deadbeef"); }

#include "device_gen.inc"

#define CLS GloftDevice
const ManagedMethod gloftDeviceMethods[] = {
    M(Device_getUserAgent, "getUserAgent", "()" STR),
    M(Device_getAndroidId, "getAndroidId", "()" STR),
    DEVICE_GEN_METHODS
    {NULL},
};
#undef CLS

Class GloftDevice::clazz = {
    .classpath       = MC4_PACKAGE "/GLUtils/Device",
    .classname       = "Device",
    .managed_methods = gloftDeviceMethods,
    .native_methods  = {NULL},
    .fields          = {NULL},
    .instance_size   = sizeof(GloftDevice),
};

/* ------------------------------------------------------------- SUtils */

static jobject SUtils_getPreferenceString(JNIEnv *env, jclass clazz, jstring key, jstring prefs)
{
    (void)clazz;
    const char *k = cstr(key);
    trace("SUtils.getPreferenceString('%s', '%s')", k, cstr(prefs));
    /* The installer stores the data directory it unpacked into under this key. */
    if (strcmp(k, "SDFolder") == 0)
        return (jobject)env->NewStringUTF(mc4_resource_dir());
    return (jobject)env->NewStringUTF("");
}

static jobject SUtils_getPreferenceString3(JNIEnv *env, jclass clazz, jstring key, jstring fallback, jstring prefs)
{
    (void)prefs;
    if (strcmp(cstr(key), "SDFolder") == 0)
        return SUtils_getPreferenceString(env, clazz, key, prefs);
    return (jobject)env->NewStringUTF(cstr(fallback));
}

static jobject SUtils_getPackage(JNIEnv *env, jclass c)
{
    (void)c;
    return (jobject)env->NewStringUTF("com.gameloft.android.ANMP.GloftM4HM");
}

static jobject SUtils_getSaveFolder(JNIEnv *env, jclass c)
{
    (void)c;
    /* Context.getFilesDir() on Android. */
    return (jobject)env->NewStringUTF(io_writable_dir());
}

static jobject SUtils_getSDFolder(JNIEnv *env, jclass c)
{
    (void)c;
    return (jobject)env->NewStringUTF(mc4_resource_dir());
}

static jobject SUtils_getContext(JNIEnv *env, jclass c)
{
    (void)env; (void)c;
    static AndroidContext the_context;
    return (jobject)&the_context;
}

static void SUtils_shareInfo(JNIEnv *e, jclass c, jstring a, jstring b, jstring d)
{
    (void)e; (void)c; (void)a; (void)b; (void)d;
}

static jbyteArray SUtils_getAssetAsString(JNIEnv *env, jclass c, jstring name)
{
    (void)c;
    return read_asset(env, cstr(name));
}

/*
 * ApplicationInfo.sourceDir: the game's own APK. The engine opens it as a zip
 * (BITrackingManager::LaunchGame looks its library up in it) and does not check
 * that the open worked, so this has to name a real APK: <game>/game.apk.
 */
static jobject SUtils_GetApkPath(JNIEnv *env, jclass c)
{
    (void)c;
    static char path[PATH_MAX];
    if (!path[0])
        snprintf(path, sizeof(path), "%s/game.apk", io_game_dir());
    return (jobject)env->NewStringUTF(path);
}

#include "sutils_gen.inc"

#define CLS GloftSUtils
const ManagedMethod gloftSUtilsMethods[] = {
    M(SUtils_getAssetAsString,     "getAssetAsString",    "(" STR ")[B"),
    M(SUtils_GetApkPath,           "GetApkPath",          "()" STR),
    SUTILS_GEN_METHODS
    M(SUtils_getPreferenceString,  "getPreferenceString", "(" STR STR ")" STR),
    M(SUtils_getPreferenceString3, "getPreferenceString", "(" STR STR STR ")" STR),
    M(SUtils_getPackage,           "getPackage",          "()" STR),
    M(SUtils_getSaveFolder,        "getSaveFolder",       "()" STR),
    M(SUtils_getSDFolder,          "getSDFolder",         "()" STR),
    M(SUtils_getContext,           "getContext",          "()Landroid/content/Context;"),
    M(SUtils_shareInfo,            "shareInfo",           "(" STR STR STR ")V"),
    {NULL},
};
#undef CLS

Class GloftSUtils::clazz = {
    .classpath       = MC4_PACKAGE "/GLUtils/SUtils",
    .classname       = "SUtils",
    .managed_methods = gloftSUtilsMethods,
    .native_methods  = {NULL},
    .fields          = {NULL},
    .instance_size   = sizeof(GloftSUtils),
};

/* ------------------------------------------------------ GameInstaller */

String GloftGameInstaller::mPreferencesName("GloftM4HMPrefs");
String GloftGameInstaller::SaveFolder("");
String GloftGameInstaller::sd_folder("");
String GloftGameInstaller::DATA_PATH("");

const FieldId gloftGameInstallerFields[] = {
    REGISTER_STATIC_FIELD(GloftGameInstaller, mPreferencesName),
    REGISTER_STATIC_FIELD(GloftGameInstaller, SaveFolder),
    REGISTER_STATIC_FIELD(GloftGameInstaller, sd_folder),
    REGISTER_STATIC_FIELD(GloftGameInstaller, DATA_PATH),
    {NULL},
};

Class GloftGameInstaller::clazz = {
    .classpath       = MC4_PACKAGE "/installer/GameInstaller",
    .classname       = "GameInstaller",
    .managed_methods = {NULL},
    .native_methods  = {NULL},
    .fields          = gloftGameInstallerFields,
    .instance_size   = sizeof(GloftGameInstaller),
};

/* ----------------------------------------------- SplashScreenActivity */

/* The "welcome screen": a web page fetched from Gameloft. Never ready, never active. */
static jint Splash_isWSReady(JNIEnv *e, jclass c, jstring s)          { (void)e; (void)c; (void)s; return 0; }
static jint Splash_isWSReady2(JNIEnv *e, jclass c, jstring s, jint i) { (void)e; (void)c; (void)s; (void)i; return 0; }
static jint Splash_isActive(JNIEnv *e, jclass c)                      { (void)e; (void)c; return 0; }
static void Splash_str(JNIEnv *e, jclass c, jstring s)                { (void)e; (void)c; (void)s; }
static void Splash_strInt(JNIEnv *e, jclass c, jstring s, jint i)     { (void)e; (void)c; (void)s; (void)i; }

static void Lib_voidInt(JNIEnv *e, jclass c, jint i) { (void)e; (void)c; (void)i; }

#define CLS GloftSplashScreen
const ManagedMethod gloftSplashMethods[] = {
    M(Splash_isWSReady,  "isWSReady",     "(" STR ")I"),
    M(Splash_isWSReady2, "isWSReady",     "(" STR "I)I"),
    M(Lib_voidInt,       "SetWSLanguage", "(I)V"),
    M(Splash_str,        "downloadWS",    "(" STR ")V"),
    M(Splash_strInt,     "downloadWS",    "(" STR "I)V"),
    M(Splash_str,        "deleteWS",      "(" STR ")V"),
    M(Splash_str,        "showLocalWS",   "(" STR ")V"),
    M(Splash_strInt,     "showLocalWS",   "(" STR "I)V"),
    M(Splash_isActive,   "isActive",      "()I"),
    {NULL},
};
#undef CLS

Class GloftSplashScreen::clazz = {
    .classpath       = MC4_PACKAGE "/SplashScreenActivity",
    .classname       = "SplashScreenActivity",
    .managed_methods = gloftSplashMethods,
    .native_methods  = {NULL},
    .fields          = {NULL},
    .instance_size   = sizeof(GloftSplashScreen),
};

/* ---------------------------------------------- com/gameloft/glot/PortingJNI */

/* The tracking library's Java helper: the device's identity and whether there
 * is a network (there is not). All of it generated. */
#include "glot_gen.inc"

#define CLS GlotPortingJNI
const ManagedMethod glotPortingMethods[] = {
    GLOT_GEN_METHODS
    {NULL},
};
#undef CLS

Class GlotPortingJNI::clazz = {
    .classpath       = "com/gameloft/glot/PortingJNI",
    .classname       = "PortingJNI",
    .managed_methods = glotPortingMethods,
    .native_methods  = {NULL},
    .fields          = {NULL},
    .instance_size   = sizeof(GlotPortingJNI),
};

/* ----------------------------------------------------------- registry */

static const int registered[] = {
    ClassRegistry::register_class(GloftGL2JNIActivity::clazz),
    ClassRegistry::register_class(GloftGL2JNILib::clazz),
    ClassRegistry::register_class(GloftMC4::clazz),
    ClassRegistry::register_class(GlotPortingJNI::clazz),
    ClassRegistry::register_class(GloftDevice::clazz),
    ClassRegistry::register_class(GloftSUtils::clazz),
    ClassRegistry::register_class(GloftGameInstaller::clazz),
    ClassRegistry::register_class(GloftSplashScreen::clazz),
};
