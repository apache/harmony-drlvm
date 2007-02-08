#include <jni.h>

JNIEXPORT void JNICALL 
Java_org_apache_harmony_drlvm_tests_regression_h3130_CallNativeTest_testCallNative(JNIEnv *, jobject); 

JNIEXPORT jobject JNICALL 
Java_org_apache_harmony_drlvm_tests_regression_h3130_CallNativeTest_getNull(JNIEnv *, jclass);


JNIEXPORT void JNICALL 
Java_org_apache_harmony_drlvm_tests_regression_h3130_CallNativeTest_testCallNative(JNIEnv *jenv, jobject obj) 
{
    jclass clazz = (*jenv)->GetObjectClass(jenv, obj);
    jmethodID mid = (*jenv)->GetMethodID(jenv, clazz, "getNull", "()Ljava/lang/Object;");
    jobject res = (*jenv)->CallObjectMethod(jenv, obj, mid);
    if (res) {
        (*jenv)->ThrowNew(jenv, (*jenv)->FindClass(jenv, "junit/framework/AssertionFailedError"), "Non-null returned");
    }
}

JNIEXPORT jobject JNICALL 
Java_org_apache_harmony_drlvm_tests_regression_h3130_CallNativeTest_getNull(JNIEnv *jenv, jclass jcl) {
    return NULL;
}
