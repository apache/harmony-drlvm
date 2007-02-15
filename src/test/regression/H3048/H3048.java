package org.apache.harmony.drlvm.tests.regression.h3048;

import junit.framework.TestCase;

public class H3048 extends TestCase {
    public void test() {
        boolean passed = false;
        try {
            new MyTest();
        } catch (NoClassDefFoundError e) {
            passed = true;
        }
        assertTrue(passed);
    }
}

class MyTest extends MyTestSuper {
    static {
        System.out.println("MyTest.<clinit>");
    }
}

class MyTestSuper {
    static {
        try {
            throw new MyException();
        } catch(Exception e) {
        }
    }
}

class MyException extends Exception {}
