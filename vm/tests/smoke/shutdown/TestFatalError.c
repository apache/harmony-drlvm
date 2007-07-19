#include "TestFatalError.h"

JNIEXPORT void JNICALL Java_shutdown_TestFatalError_sendFatalError
  (JNIEnv * jni_env, jclass clazz)
{
    (*jni_env)->FatalError(jni_env, "PASSED");
}
