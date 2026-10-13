#pragma once

/*
 * What the parts of this port need from each other.
 *
 * Nothing generic belongs here - portbase owns the loader, the thunks, the
 * fake JVM and the input interface. This is only the handful of facts that are
 * about Modern Combat 4 specifically and are needed in more than one file.
 */

struct so_module;
struct _JNIEnv;

/* The loaded guest library. Same pointer port_guest_module() returns; declared
 * here so the JNI classes can forward to the game's own native methods without
 * threading it through the class registry. */
so_module *mc4_module(void);

/*
 * Every entry point this port drives lives on one Java class, and the mangled
 * name is long enough that spelling it out at each call site is how a typo
 * becomes a null function pointer. so_symbol() answers NULL for a name that
 * does not exist and says nothing about it.
 */
#define MC4_GL2JNILIB(m) "Java_com_gameloft_glf_GL2JNILib_" m

/* The class the game's own entry points live on (MC4.nativeMC4Init and friends). */
#define MC4_JNI(m) "Java_com_gameloft_android_ANMP_GloftM4HM_" m

/* Gives the four GDRM lock pointers storage. patch.cpp */
void mc4_unlock_drm(so_module *mod);

/* Shows the frame the engine has finished: GL2JNILib.SwapBuffer(). main.cpp */
void mc4_present(void);

/* The expansion file the engine opens itself: main (false) or patch (true).
 * <game>/obb/<the name the engine knows>. io_paths.cpp */
const char *mc4_obb_path(bool patch);

/* Engine settings forced over the device profile. tweaks.cpp */
void mc4_tweaks_install(so_module *mod);
/* Once per frame: the frame-rate target and the scheduler's clamp on the main thread. tweaks.cpp */
void mc4_sched_tick(long frames);

/*
 * Hand the engine its four directories.
 *
 * On Android this is Java: GL2JNILib.setupPaths() works out the obb, data,
 * home and temp directories and calls the native setPaths with them. The
 * engine calls setupPaths back through JNI from inside GL2JNILib.init(), so
 * the normal path through this function is the fake class in
 * jni/gl2jni_activity.cpp. main() calls it too, after init(), for the case
 * where the callback never arrives - it runs once and traces which of the two
 * got there first.
 */
void mc4_set_paths(_JNIEnv *env);

/*
 * Create the directories under the writable storage that the engine writes
 * into without ever creating them. Called from main() before the boot
 * sequence; implemented next to the path rules in game/io_paths.cpp.
 */
void mc4_prepare_writable_storage(void);

/* The directory the engine reads its data from: <game>/files, which is what
 * /sdcard/Android/data/<package>/files maps to (the cache archive's layout). */
const char *mc4_resource_dir(void);

/*
 * Whether the engine says it is sitting in the main menu or the in-game menu.
 *
 * The game exports this as GL2JNILib.nativeIsMainMenuOrIGM and its own Java
 * layer polls it to decide what a button means. Here it is the cheap way to
 * tell "the port drew 600 frames of the title screen" apart from "the port is
 * playing" - note the polarity, true means *in a menu*. Implemented in
 * game/input_bridge.cpp, next to the symbol it resolves.
 */
bool mc4_is_main_menu(void);

/*
 * Tell the fake activity what GetWindowWidth/Height should answer.
 *
 * The engine lays its interface out against those two numbers and the port
 * resizes the renderer with its own; a disagreement between them puts the
 * touch targets somewhere the pixels are not. One source, set from main().
 * Implemented in jni/gl2jni_activity.cpp.
 */
void mc4_set_view_size(int w, int h);

/*
 * Put the engine into its gamepad input path.
 *
 * Called from main() after GL2JNILib.init() has returned, because the flags it
 * sets live in state that init() creates. Implemented in game/input_bridge.cpp
 * next to the rest of the input wiring.
 */
void mc4_input_select_gamepad_mode(void);
