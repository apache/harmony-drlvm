package org.apache.harmony.drlvm.tests.regression.h5021;

import junit.framework.TestCase;

public class Test extends TestCase
{
    static final char bigchar = 0x740;
    
    static native char bigChar(char bigChar);

    static {
        System.loadLibrary("char16");
    }

    public void testJNIRetChar16() {
       assertEquals(bigchar, bigChar(bigchar));
    }
}
