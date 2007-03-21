package org.apache.harmony.drlvm.tests.regression.h3404;

import junit.framework.TestCase;

public class H3404 extends TestCase {
    public static native void mmm(int depth) throws MyException;

    static {
        System.loadLibrary("H3404");
    }
    
    public H3404(String message) {
        super(message);
    }
    
    public void test() {
        try {
            mmm(10);
        } catch (MyException e) {
            System.out.println("Test passed");
            return;
        }
        fail("No exception was thrown from native code");
    }
}

class MyException extends Exception {
    public MyException(String message) {
        super(message);
    }
}

