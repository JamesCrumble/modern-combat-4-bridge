static jobject SUtils_gen_0(JNIEnv *env, jclass c, jstring a0) { (void)env; GEN_SAID("getGLUID(Ljava/lang/String;)[I"); return (jobject)env->NewIntArray(0); }
static jobject SUtils_gen_1(JNIEnv *env, jclass c) { (void)env; GEN_SAID("getGameName()Ljava/lang/String;"); return (jobject)env->NewStringUTF(""); }
static jobject SUtils_gen_2(JNIEnv *env, jclass c) { (void)env; GEN_SAID("getInjectedIGP()Ljava/lang/String;"); return (jobject)env->NewStringUTF(""); }
static jobject SUtils_gen_3(JNIEnv *env, jclass c) { (void)env; GEN_SAID("getInjectedSerialKey()Ljava/lang/String;"); return (jobject)env->NewStringUTF(""); }
static jint SUtils_gen_4(JNIEnv *env, jclass c, jobject a0, jint a1) { (void)env; GEN_SAID("getInt([BI)I"); return 0; }
static jint SUtils_gen_5(JNIEnv *env, jclass c) { (void)env; GEN_SAID("hasConnectivity()I"); return 0; }
static void SUtils_gen_6(JNIEnv *env, jclass c, jboolean a0) { (void)env; GEN_SAID("inGameVideoSetSkipEnabled(Z)V");  }
static jint SUtils_gen_7(JNIEnv *env, jclass c) { (void)env; GEN_SAID("initCheckConnectionType()I"); return 0; }
static jobject SUtils_gen_8(JNIEnv *env, jclass c, jobject a0) { (void)env; GEN_SAID("nativeGetPreference(Landroid/os/Bundle;)Landroid/os/Bundle;"); return (jobject)a0; }
static void SUtils_gen_9(JNIEnv *env, jclass c, jobject a0) { (void)env; GEN_SAID("nativeSetPreference(Landroid/os/Bundle;)V");  }
static jboolean SUtils_gen_10(JNIEnv *env, jclass c, jstring a0, jboolean a1) { (void)env; GEN_SAID("playVideo(Ljava/lang/String;Z)Z"); return 0; }
static void SUtils_gen_11(JNIEnv *env, jclass c) { (void)env; GEN_SAID("release()V");  }
static jobject SUtils_gen_12(JNIEnv *env, jclass c) { (void)env; GEN_SAID("retrieveBarrels()[I"); return (jobject)env->NewIntArray(0); }
static void SUtils_gen_13(JNIEnv *env, jclass c, jint a0) { (void)env; GEN_SAID("showCantGoBackPopup(I)V");  }
static void SUtils_gen_14(JNIEnv *env, jclass c) { (void)env; GEN_SAID("stopVideo()V");  }
#define SUTILS_GEN_METHODS \
    ManagedMethod::RegisterStatic<&SUtils_gen_0>(GloftSUtils::clazz, "getGLUID", "(Ljava/lang/String;)[I"), \
    ManagedMethod::RegisterStatic<&SUtils_gen_1>(GloftSUtils::clazz, "getGameName", "()Ljava/lang/String;"), \
    ManagedMethod::RegisterStatic<&SUtils_gen_2>(GloftSUtils::clazz, "getInjectedIGP", "()Ljava/lang/String;"), \
    ManagedMethod::RegisterStatic<&SUtils_gen_3>(GloftSUtils::clazz, "getInjectedSerialKey", "()Ljava/lang/String;"), \
    ManagedMethod::RegisterStatic<&SUtils_gen_4>(GloftSUtils::clazz, "getInt", "([BI)I"), \
    ManagedMethod::RegisterStatic<&SUtils_gen_5>(GloftSUtils::clazz, "hasConnectivity", "()I"), \
    ManagedMethod::RegisterStatic<&SUtils_gen_6>(GloftSUtils::clazz, "inGameVideoSetSkipEnabled", "(Z)V"), \
    ManagedMethod::RegisterStatic<&SUtils_gen_7>(GloftSUtils::clazz, "initCheckConnectionType", "()I"), \
    ManagedMethod::RegisterStatic<&SUtils_gen_8>(GloftSUtils::clazz, "nativeGetPreference", "(Landroid/os/Bundle;)Landroid/os/Bundle;"), \
    ManagedMethod::RegisterStatic<&SUtils_gen_9>(GloftSUtils::clazz, "nativeSetPreference", "(Landroid/os/Bundle;)V"), \
    ManagedMethod::RegisterStatic<&SUtils_gen_10>(GloftSUtils::clazz, "playVideo", "(Ljava/lang/String;Z)Z"), \
    ManagedMethod::RegisterStatic<&SUtils_gen_11>(GloftSUtils::clazz, "release", "()V"), \
    ManagedMethod::RegisterStatic<&SUtils_gen_12>(GloftSUtils::clazz, "retrieveBarrels", "()[I"), \
    ManagedMethod::RegisterStatic<&SUtils_gen_13>(GloftSUtils::clazz, "showCantGoBackPopup", "(I)V"), \
    ManagedMethod::RegisterStatic<&SUtils_gen_14>(GloftSUtils::clazz, "stopVideo", "()V"), \
    /* end */
