package org.apache.harmony.drlvm.tests.regression.h0000;

import java.nio.Buffer;
import java.nio.ByteBuffer;
import junit.framework.TestCase;

public class DirectByteBufferTest extends TestCase {

    static { System.loadLibrary("DirectByteBufferTest"); }

    public static void main(String[] args) {
        new DirectByteBufferTest().testValidBuffer();
    }

    private static native String tryDirectBuffer();
    private static native String checkSameDirectStorage(Buffer b1, Buffer b2);
    
    private static void assertView(String message, ByteBuffer b1, Buffer b2, int capacityRatio) {
        assertEquals(message + ":capacity", b1.capacity()/capacityRatio, b2.capacity());
        String err = checkSameDirectStorage(b1, b2);
        assertNull(message + " : " + err, err);
    }
    
    public void testValidBuffer() {
        String err = tryDirectBuffer();
        assertNull(err, err);
    }

    /**
     * A regression test for HARMONY-3591: 
     * JNI operations fail on non-byte views of a direct buffer.
     */
    public void testBufferView() {
        ByteBuffer b = ByteBuffer.allocateDirect(100);
        assertTrue(b.isDirect());
        assertView("duplicate", b, b.duplicate(), 1);
        assertView("char view", b, b.asCharBuffer(), 2);
        assertView("short view", b, b.asShortBuffer(), 2);
        assertView("int view", b, b.asIntBuffer(), 4);
        assertView("float view", b, b.asFloatBuffer(), 4);
        assertView("double view", b, b.asDoubleBuffer(), 8);
        assertView("long view", b, b.asLongBuffer(), 8);
    }
}
