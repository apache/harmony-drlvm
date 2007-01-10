#include <open/vm_gc.h>
#include <jni.h>
#include "../thread/gc_thread.h"
#include "../gen/gen.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Class:     org_apache_harmony_drlvm_gc_gen_GCHelper
 * Method:    TLSFreeOffset
 * Signature: ()I
 */
JNIEXPORT jint JNICALL Java_org_apache_harmony_drlvm_gc_1gen_GCHelper_TLSGCOffset(JNIEnv *e, jclass c)
{
    return (jint)tls_gc_offset;
}

 

JNIEXPORT jint JNICALL Java_org_apache_harmony_drlvm_gc_1gen_GCHelper_getNosBoundary(JNIEnv *e, jclass c)
{
    return (jint)nos_boundary;
}



#ifdef __cplusplus
}
#endif
