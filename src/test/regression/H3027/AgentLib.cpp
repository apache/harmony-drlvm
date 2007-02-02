#include <string.h>
#include <jvmti.h>

static void JNICALL vm_init_callback(jvmtiEnv* jvmti_env, JNIEnv* jni_env, jthread thread)
{
    jclass cl = jni_env->FindClass("org/apache/harmony/drlvm/tests/regression/h3027/Status");
    if (NULL == cl)
        return;

    jfieldID fid = jni_env->GetStaticFieldID(cl, "status", "Z");
    if (NULL == fid)
        return;

    jni_env->SetStaticBooleanField(cl, fid, JNI_TRUE);
}

JNIEXPORT jint JNICALL Agent_OnLoad(JavaVM *vm, char *options, void *reserved)
{
    jvmtiEnv *jvmti_env;
    if(vm->GetEnv((void**)&jvmti_env, JVMTI_VERSION_1_0) != JNI_OK)
        return JNI_ERR;

    // Get all supported capabilities
    jvmtiCapabilities capabilities;
    jvmtiError result = jvmti_env->GetPotentialCapabilities(&capabilities);
    if (JVMTI_ERROR_NONE != result)
        return JNI_ERR;

    // Enabled all supported capabilities
    result = jvmti_env->AddCapabilities(&capabilities);
    if (JVMTI_ERROR_NONE != result)
        return JNI_ERR;

    jvmtiEventCallbacks callbacks;
    memset(&callbacks, 0, sizeof(jvmtiEventCallbacks));
    callbacks.VMInit = vm_init_callback;

    // Set callback for VMInit
    result = jvmti_env->SetEventCallbacks(&callbacks, (jint)sizeof(callbacks));
    if (JVMTI_ERROR_NONE != result)
        return JNI_ERR;

    // Set event mode to true
    result = jvmti_env->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_VM_INIT, NULL);
    if (JVMTI_ERROR_NONE != result)
        return JNI_ERR;

    return JNI_OK;
}
