#include <string.h>
#include <vector>

#include "jni.h"
#include "jni_internals.h"

/*
 * String.getBytes(charsetName). Strings here are UTF-8 C strings already, so
 * the bytes are the string whatever charset is named. First importer: Wild
 * Blood's jstringTochar, which converts every string Java hands it this way.
 */
static jbyteArray String_getBytes(JNIEnv *env, jobject self, jstring charset)
{
    (void)charset;
    const char *str = ((String *)self)->str;
    jsize len = str ? (jsize)strlen(str) : 0;
    jbyteArray bytes = env->NewByteArray(len);
    if (len)
        env->SetByteArrayRegion(bytes, 0, len, (const jbyte *)str);
    return bytes;
}

static const ManagedMethod stringMethods[] = {
    ManagedMethod::Register<&String_getBytes>(String::clazz, "getBytes", "(Ljava/lang/String;)[B"),
    {NULL},
};

Class String::clazz = {
    .classpath = "java/lang/String",
    .classname = "String",
    .managed_methods = stringMethods,
    .fields = {NULL},
    .instance_size = sizeof(String)
};

String::String(char *str) {
    this->str = str;
}

String::String(const char *str) {
    this->str = strdup(str);
}

static const int registered = ClassRegistry::register_class(String::clazz);
