#include <jni.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

JNIEXPORT jstring JNICALL Java_DirectByteBufferTest_testValidBuffer0
  (JNIEnv *, jobject);


/*
 * Class:     DirectByteBufferTest
 * Method:    testValidBuffer0
 * Signature: ()Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL Java_DirectByteBufferTest_testValidBuffer0
  (JNIEnv *jenv, jobject unused)
{
    char* error = (char*)calloc(256, 1);
    const jlong BUF_SIZE = 100;
    void* buf = malloc(BUF_SIZE);
    jobject jbuf = (*jenv)->NewDirectByteBuffer(jenv, buf, BUF_SIZE);
    void* addr = (*jenv)->GetDirectBufferAddress(jenv, jbuf);
    jlong size = (*jenv)->GetDirectBufferCapacity(jenv, jbuf);
    jstring jstr;
    if (jbuf) {
        if (addr != buf) {
            sprintf(error, "invalid buffer address: expected %p but was %p\n", buf, addr);
        } 
        if (size != BUF_SIZE) {
            sprintf(error + strlen(error), 
                "invalid buffer capacity: expected %d but was %d\n", BUF_SIZE, size);
        }
    } else {
        // access to direct buffers not supported
        if (addr != NULL | size != -1) {
            sprintf(error, "inconsistent NIO support:\n" 
                "NewDirectByteBuffer() returned NULL;\n"
                "GetDirectBufferAddress() returned %p\n"
                "GetDirectBufferCapacity() returned %d\n", addr, size);
        } else {
            sprintf(error, "no NIO support\n");
        }
    }

    jstr = strlen(error) ? (*jenv)->NewStringUTF(jenv, error) : NULL;
    free(buf);
    free(error);

    return jstr;
}
