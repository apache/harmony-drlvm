#include <open/vm_gc.h>
#include <jni.h>
#include "gc_types.h"


#ifdef __cplusplus
extern "C" {
#endif

/*
 * Class:     org_apache_harmony_drlvm_gc_cc_GCHelper
 * Method:    getCurrentOffset
 * Signature: ()I
 */
JNIEXPORT jint JNICALL Java_org_apache_harmony_drlvm_gc_1cc_GCHelper_getCurrentOffset(JNIEnv *e, jclass c)
{
    return (jint)tls_offset_current;
}

/*
 * Class:     org_apache_harmony_drlvm_gc_cc_GCHelper
 * Method:    getCleanedOffset
 * Signature: ()I
 */
JNIEXPORT jint JNICALL Java_org_apache_harmony_drlvm_gc_1cc_GCHelper_getCleanedOffset(JNIEnv *e, jclass c)
{
    return (jint)tls_offset_clean;
}

#ifdef __cplusplus
}
#endif
