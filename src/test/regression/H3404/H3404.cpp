#include <jni.h>
#include <assert.h>

#ifdef __cplusplus
extern "C" {
#endif

JNIEXPORT void JNICALL Java_org_apache_harmony_drlvm_tests_regression_h3404_H3404_mmm
  (JNIEnv *env, jclass that, jint depth)
{
    if (depth > 0)
    {
        jmethodID mid = env->GetStaticMethodID(that, "mmm", "(I)V");
        assert(mid);
        env->CallStaticVoidMethod(that, mid, depth - 1);
    }
    else
    {
        jclass ecl = env->FindClass("org/apache/harmony/drlvm/tests/regression/h3404/MyException");
        assert(ecl);
        env->ThrowNew(ecl, "Crash me");
    }
}

#ifdef __cplusplus
}
#endif
