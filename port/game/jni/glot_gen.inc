static jobject Glot_gen_0(JNIEnv *env, jclass c) { (void)env; GEN_SAID("GetBarrels()[I"); return (jobject)env->NewIntArray(0); }
static jobject Glot_gen_1(JNIEnv *env, jclass c) { (void)env; GEN_SAID("getDeviceCarrier()Ljava/lang/String;"); return (jobject)env->NewStringUTF(""); }
static jobject Glot_gen_2(JNIEnv *env, jclass c) { (void)env; GEN_SAID("getDeviceCountry()Ljava/lang/String;"); return (jobject)env->NewStringUTF(""); }
static jobject Glot_gen_3(JNIEnv *env, jclass c) { (void)env; GEN_SAID("getDeviceFirmware()Ljava/lang/String;"); return (jobject)env->NewStringUTF(""); }
static jobject Glot_gen_4(JNIEnv *env, jclass c) { (void)env; GEN_SAID("getDeviceLanguage()Ljava/lang/String;"); return (jobject)env->NewStringUTF(""); }
static jobject Glot_gen_5(JNIEnv *env, jclass c) { (void)env; GEN_SAID("getDeviceName()Ljava/lang/String;"); return (jobject)env->NewStringUTF(""); }
static jobject Glot_gen_6(JNIEnv *env, jclass c) { (void)env; GEN_SAID("getIdentifier()Ljava/lang/String;"); return (jobject)env->NewStringUTF(""); }
static jobject Glot_gen_7(JNIEnv *env, jclass c) { (void)env; GEN_SAID("getMac()Ljava/lang/String;"); return (jobject)env->NewStringUTF(""); }
static jobject Glot_gen_8(JNIEnv *env, jclass c) { (void)env; GEN_SAID("getSerial()Ljava/lang/String;"); return (jobject)env->NewStringUTF(""); }
static jobject Glot_gen_9(JNIEnv *env, jclass c) { (void)env; GEN_SAID("getSerialNo()Ljava/lang/String;"); return (jobject)env->NewStringUTF(""); }
static jlong Glot_gen_10(JNIEnv *env, jclass c) { (void)env; GEN_SAID("getSystemUpTimeMillis()J"); return 0; }
static jboolean Glot_gen_11(JNIEnv *env, jclass c) { (void)env; GEN_SAID("hasConnectivity()Z"); return 0; }
#define GLOT_GEN_METHODS \
    ManagedMethod::RegisterStatic<&Glot_gen_0>(GlotPortingJNI::clazz, "GetBarrels", "()[I"), \
    ManagedMethod::RegisterStatic<&Glot_gen_1>(GlotPortingJNI::clazz, "getDeviceCarrier", "()Ljava/lang/String;"), \
    ManagedMethod::RegisterStatic<&Glot_gen_2>(GlotPortingJNI::clazz, "getDeviceCountry", "()Ljava/lang/String;"), \
    ManagedMethod::RegisterStatic<&Glot_gen_3>(GlotPortingJNI::clazz, "getDeviceFirmware", "()Ljava/lang/String;"), \
    ManagedMethod::RegisterStatic<&Glot_gen_4>(GlotPortingJNI::clazz, "getDeviceLanguage", "()Ljava/lang/String;"), \
    ManagedMethod::RegisterStatic<&Glot_gen_5>(GlotPortingJNI::clazz, "getDeviceName", "()Ljava/lang/String;"), \
    ManagedMethod::RegisterStatic<&Glot_gen_6>(GlotPortingJNI::clazz, "getIdentifier", "()Ljava/lang/String;"), \
    ManagedMethod::RegisterStatic<&Glot_gen_7>(GlotPortingJNI::clazz, "getMac", "()Ljava/lang/String;"), \
    ManagedMethod::RegisterStatic<&Glot_gen_8>(GlotPortingJNI::clazz, "getSerial", "()Ljava/lang/String;"), \
    ManagedMethod::RegisterStatic<&Glot_gen_9>(GlotPortingJNI::clazz, "getSerialNo", "()Ljava/lang/String;"), \
    ManagedMethod::RegisterStatic<&Glot_gen_10>(GlotPortingJNI::clazz, "getSystemUpTimeMillis", "()J"), \
    ManagedMethod::RegisterStatic<&Glot_gen_11>(GlotPortingJNI::clazz, "hasConnectivity", "()Z"), \
    /* end */
