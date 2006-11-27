package org.apache.harmony.drlvm.tests.regression.h0000;

import junit.framework.TestCase;

public class DirectByteBufferTest extends TestCase {

    static { System.loadLibrary("DirectByteBufferTest"); }

    public static void main(String[] args) {
        new DirectByteBufferTest().testValidBuffer();
    }

    private native String testValidBuffer0();
    
    public void testValidBuffer() {
        assertNull(testValidBuffer0());
    }
}
