#include <open/vm_gc.h>
#include <jni.h>
#include "open/vm_util.h"
#include "environment.h"
#include "../thread/gc_thread.h"
#include "../gen/gen.h"
#include "java_support.h"

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

JNIEXPORT jobject JNICALL Java_org_apache_harmony_drlvm_gc_1gen_GCHelper_getNosBoundary(JNIEnv *e, jclass c)
{
    return (jobject)nos_boundary;
}

JNIEXPORT jboolean JNICALL Java_org_apache_harmony_drlvm_gc_1gen_GCHelper_getGenMode(JNIEnv *e, jclass c)
{
    return (jboolean)gc_is_gen_mode();
}

JNIEXPORT void JNICALL Java_org_apache_harmony_drlvm_gc_1gen_GCHelper_helperCallback(JNIEnv *e, jclass c)
{
    java_helper_inlined = TRUE;

    POINTER_SIZE_INT obj = *(POINTER_SIZE_INT*)c;
    /* a trick to get the GCHelper_class j.l.c in order to manipulate its 
       fields in GC native code */ 
    Class_Handle *vm_class_ptr = (Class_Handle *)(obj + VM_Global_State::loader_env->vm_class_offset);
    GCHelper_clss = *vm_class_ptr;
}

#ifdef __cplusplus
}
#endif
