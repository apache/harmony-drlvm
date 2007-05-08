#include <jni.h>
#include <assert.h>

#ifndef _CHECK3784_
#define _CHECK3784_

#ifdef __cplusplus
extern "C" {
#endif


static jlong lorig = (jlong)-1;
static jint  iorig = (jint)-1;


JNIEXPORT jint JNICALL 
Java_org_apache_harmony_drlvm_tests_regression_h3784_Test_getPointerSize(JNIEnv *, jclass)  {
    return (jint)sizeof(void*);
}

JNIEXPORT jlong JNICALL 
Java_org_apache_harmony_drlvm_tests_regression_h3784_Test_getAddress(JNIEnv *, jclass) 
{
    if (sizeof(void*)==4) {
        return (jlong)iorig;
    } 
    assert(sizeof(void*)==8);
    return lorig;
}

JNIEXPORT jboolean JNICALL 
Java_org_apache_harmony_drlvm_tests_regression_h3784_Test_check(JNIEnv *, jclass, jlong val) 
{
    if (sizeof(void*)==4) {
        return iorig == (jint)val;
    }
    assert(sizeof(void*)==8);
    return lorig == val;
}


#ifdef __cplusplus
}
#endif
#endif
