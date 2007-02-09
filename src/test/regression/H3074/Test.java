package org.apache.harmony.drlvm.tests.regression.h3074;

import junit.framework.TestCase;

public class Test extends TestCase {
    public void test() {
        System.out.println("Test is loaded with " + Test.class.getClassLoader());
        try {
            System.loadLibrary("natives");
        } catch (Throwable e) {
            System.out.println("Test failed on exception:");
            e.printStackTrace();
        }
        assertTrue(AnotherClass.status);
    }
}
