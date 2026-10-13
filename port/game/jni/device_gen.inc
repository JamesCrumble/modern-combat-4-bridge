static jobject Device_gen_0(JNIEnv *env, jclass c) { (void)env; GEN_SAID("getDeviceFirmware()Ljava/lang/String;"); return (jobject)env->NewStringUTF(""); }
static jobject Device_gen_1(JNIEnv *env, jclass c) { (void)env; GEN_SAID("getDeviceIMEI()Ljava/lang/String;"); return (jobject)env->NewStringUTF(""); }
static jobject Device_gen_2(JNIEnv *env, jclass c) { (void)env; GEN_SAID("getDeviceId()Ljava/lang/String;"); return (jobject)env->NewStringUTF(""); }
static jobject Device_gen_3(JNIEnv *env, jclass c) { (void)env; GEN_SAID("getDeviceName()Ljava/lang/String;"); return (jobject)env->NewStringUTF(""); }
static jobject Device_gen_4(JNIEnv *env, jclass c) { (void)env; GEN_SAID("getGLDID()Ljava/lang/String;"); return (jobject)env->NewStringUTF(""); }
static jobject Device_gen_5(JNIEnv *env, jclass c) { (void)env; GEN_SAID("getHDIDFV()Ljava/lang/String;"); return (jobject)env->NewStringUTF(""); }
static jobject Device_gen_6(JNIEnv *env, jclass c) { (void)env; GEN_SAID("getHDIDFVVersion()Ljava/lang/String;"); return (jobject)env->NewStringUTF(""); }
static jobject Device_gen_7(JNIEnv *env, jclass c) { (void)env; GEN_SAID("getIMEI()Ljava/lang/String;"); return (jobject)env->NewStringUTF(""); }
static jobject Device_gen_8(JNIEnv *env, jclass c) { (void)env; GEN_SAID("getMacAddress()Ljava/lang/String;"); return (jobject)env->NewStringUTF(""); }
static jobject Device_gen_9(JNIEnv *env, jclass c) { (void)env; GEN_SAID("getPhoneManufacturer()Ljava/lang/String;"); return (jobject)env->NewStringUTF(""); }
static jobject Device_gen_10(JNIEnv *env, jclass c) { (void)env; GEN_SAID("getPhoneModel()Ljava/lang/String;"); return (jobject)env->NewStringUTF(""); }
static jobject Device_gen_11(JNIEnv *env, jclass c) { (void)env; GEN_SAID("getSerial()Ljava/lang/String;"); return (jobject)env->NewStringUTF(""); }
static jobject Device_gen_12(JNIEnv *env, jclass c) { (void)env; GEN_SAID("getSerialNo()Ljava/lang/String;"); return (jobject)env->NewStringUTF(""); }
static jlong Device_gen_13(JNIEnv *env, jclass c) { (void)env; GEN_SAID("getSystemUpTimeMillis()J"); return 0; }
static jobject Device_gen_14(JNIEnv *env, jclass c) { (void)env; GEN_SAID("retrieveCPUSerial()Ljava/lang/String;"); return (jobject)env->NewStringUTF(""); }
static jobject Device_gen_15(JNIEnv *env, jclass c) { (void)env; GEN_SAID("retrieveDeviceCarrier()Ljava/lang/String;"); return (jobject)env->NewStringUTF(""); }
static jobject Device_gen_16(JNIEnv *env, jclass c) { (void)env; GEN_SAID("retrieveDeviceCountry()Ljava/lang/String;"); return (jobject)env->NewStringUTF(""); }
static jobject Device_gen_17(JNIEnv *env, jclass c) { (void)env; GEN_SAID("retrieveDeviceLanguage()Ljava/lang/String;"); return (jobject)env->NewStringUTF(""); }
static jobject Device_gen_18(JNIEnv *env, jclass c) { (void)env; GEN_SAID("retrieveDeviceRegion()Ljava/lang/String;"); return (jobject)env->NewStringUTF(""); }
#define DEVICE_GEN_METHODS \
    ManagedMethod::RegisterStatic<&Device_gen_0>(GloftDevice::clazz, "getDeviceFirmware", "()Ljava/lang/String;"), \
    ManagedMethod::RegisterStatic<&Device_gen_1>(GloftDevice::clazz, "getDeviceIMEI", "()Ljava/lang/String;"), \
    ManagedMethod::RegisterStatic<&Device_gen_2>(GloftDevice::clazz, "getDeviceId", "()Ljava/lang/String;"), \
    ManagedMethod::RegisterStatic<&Device_gen_3>(GloftDevice::clazz, "getDeviceName", "()Ljava/lang/String;"), \
    ManagedMethod::RegisterStatic<&Device_gen_4>(GloftDevice::clazz, "getGLDID", "()Ljava/lang/String;"), \
    ManagedMethod::RegisterStatic<&Device_gen_5>(GloftDevice::clazz, "getHDIDFV", "()Ljava/lang/String;"), \
    ManagedMethod::RegisterStatic<&Device_gen_6>(GloftDevice::clazz, "getHDIDFVVersion", "()Ljava/lang/String;"), \
    ManagedMethod::RegisterStatic<&Device_gen_7>(GloftDevice::clazz, "getIMEI", "()Ljava/lang/String;"), \
    ManagedMethod::RegisterStatic<&Device_gen_8>(GloftDevice::clazz, "getMacAddress", "()Ljava/lang/String;"), \
    ManagedMethod::RegisterStatic<&Device_gen_9>(GloftDevice::clazz, "getPhoneManufacturer", "()Ljava/lang/String;"), \
    ManagedMethod::RegisterStatic<&Device_gen_10>(GloftDevice::clazz, "getPhoneModel", "()Ljava/lang/String;"), \
    ManagedMethod::RegisterStatic<&Device_gen_11>(GloftDevice::clazz, "getSerial", "()Ljava/lang/String;"), \
    ManagedMethod::RegisterStatic<&Device_gen_12>(GloftDevice::clazz, "getSerialNo", "()Ljava/lang/String;"), \
    ManagedMethod::RegisterStatic<&Device_gen_13>(GloftDevice::clazz, "getSystemUpTimeMillis", "()J"), \
    ManagedMethod::RegisterStatic<&Device_gen_14>(GloftDevice::clazz, "retrieveCPUSerial", "()Ljava/lang/String;"), \
    ManagedMethod::RegisterStatic<&Device_gen_15>(GloftDevice::clazz, "retrieveDeviceCarrier", "()Ljava/lang/String;"), \
    ManagedMethod::RegisterStatic<&Device_gen_16>(GloftDevice::clazz, "retrieveDeviceCountry", "()Ljava/lang/String;"), \
    ManagedMethod::RegisterStatic<&Device_gen_17>(GloftDevice::clazz, "retrieveDeviceLanguage", "()Ljava/lang/String;"), \
    ManagedMethod::RegisterStatic<&Device_gen_18>(GloftDevice::clazz, "retrieveDeviceRegion", "()Ljava/lang/String;"), \
    /* end */
