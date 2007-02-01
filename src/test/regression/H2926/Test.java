package org.apache.harmony.drlvm.tests.regression.h2926;

import junit.framework.TestCase;

/**
 * Test correct locations of catch and throw in JVMTI Exception event
 */
public class Test extends TestCase {
    public void test() {
        TestClass tc = new TestClass();
        tc.f1();
        tc.f2();
        tc.f3();
        tc.f4();
        System.out.println("test done");
        assertTrue(Status.status);
    }
}

class Status {
    public static boolean status = false;
}

