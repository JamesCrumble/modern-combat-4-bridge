#!/bin/bash
# Regenerates the quiet Java stubs (port/game/jni/*_gen.inc) from the dex listing and the library's
# strings. The names after the class are the methods written by hand in gl2jni_activity.cpp.
P=$(cd "$(dirname "$0")/.." && pwd)
G=com/gameloft/android/ANMP/GloftM4HM
python3 "$P/tools/gen_java_stubs.py" "Lcom/gameloft/glf/GL2JNILib;" Glf CLS \
  getResource setupPaths createView setViewSettings setCurrentContext GetWindowWidth GetWindowHeight \
  GetManufacturer GetDeviceName GetDeviceFirmware exitGame SwapBuffer getDeviceLanguage \
  > "$P/port/game/jni/glf_gen.inc"
python3 "$P/tools/gen_java_stubs.py" "L$G/MC4;" Game GloftMC4 \
  getMainFileLocation getPatchFileLocation getSDFolder getGeneration getArchitecture getMicroArchitecture \
  getProcessorNum getBuildVersion getCountry getUserAgent getMasterVolume \
  > "$P/port/game/jni/game_gen.inc"
python3 "$P/tools/gen_java_stubs.py" "L$G/GLUtils/SUtils;" SUtils GloftSUtils \
  getPreferenceString getPackage getSaveFolder getSDFolder getContext shareInfo getAssetAsString GetApkPath \
  > "$P/port/game/jni/sutils_gen.inc"
python3 "$P/tools/gen_java_stubs.py" "L$G/GLUtils/Device;" Device GloftDevice \
  getUserAgent getAndroidId \
  > "$P/port/game/jni/device_gen.inc"
python3 "$P/tools/gen_java_stubs.py" "Lcom/gameloft/glot/PortingJNI;" Glot GlotPortingJNI \
  > "$P/port/game/jni/glot_gen.inc"
