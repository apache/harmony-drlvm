#include <jni.h>

#ifdef __cplusplus
extern "C" {
#endif

JNIEXPORT jchar JNICALL Java_org_apache_harmony_drlvm_tests_regression_h5021_Test_bigChar
  (JNIEnv* env, jclass this_class, jchar input)
{
    return input;
}

#ifdef __cplusplus
}
#endif
