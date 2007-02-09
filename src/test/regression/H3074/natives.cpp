#include <jni.h>

#ifdef __cplusplus
extern "C" jint JNIEXPORT JNICALL JNI_OnLoad(JavaVM *vm, void *reserved);
#endif

jint JNIEXPORT JNICALL JNI_OnLoad(JavaVM *vm, void *reserved)
{
    JNIEnv *env;
    jint status = vm->GetEnv((void **)&env, JNI_VERSION_1_4);
    if (status != JNI_OK)
    {
        printf("Could not get JNIenv\n");
        return 0;
    }

    jclass cl = env->FindClass("org/apache/harmony/drlvm/tests/regression/h3074/AnotherClass");
    if (NULL == cl)
    {
        printf("Failed to load class:\n");
        env->ExceptionDescribe();
        return 0;
    }

    jmethodID mid = env->GetStaticMethodID(cl, "method", "()V");
    if (NULL == mid)
    {
        printf("Failed to get method ID:\n");
        env->ExceptionDescribe();
        return 0;
    }

    env->CallStaticVoidMethod(cl, mid);
    return JNI_VERSION_1_4;
}
