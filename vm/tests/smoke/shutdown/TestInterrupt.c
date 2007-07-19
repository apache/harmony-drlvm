#include "TestInterrupt.h"

#if defined (_WIN32) || defined (__WIN32__) || defined (WIN32)
#include <windows.h>

JNIEXPORT void JNICALL Java_shutdown_TestInterrupt_sendInterrupt
  (JNIEnv * jni_env, jclass clazz)
{
    printf("HAH Signalling....");
    GenerateConsoleCtrlEvent(CTRL_C_EVENT, 0);
}

#else

#include <signal.h>

JNIEXPORT void JNICALL Java_shutdown_TestInterrupt_sendInterrupt
  (JNIEnv * jni_env, jclass clazz)
{
    printf("Signalling....");
    raise(SIGINT);
}

#endif
