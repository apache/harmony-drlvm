package org.apache.harmony.drlvm.tests.regression.h3317;

import junit.framework.TestCase;

/**
 * Test case for GetFieldDeclaringClass() jvmti function.
 */
public class GetFieldDeclaringClassTest extends TestCase {
    public static void main(String args[]) {
        (new GetFieldDeclaringClassTest()).test();
    }

    public void test() {
        System.out.println("test done");
        assertTrue(Status.status);
    }
}

class Status {
    /** the field should be modified by jvmti agent to determine test result. */
    public static boolean status = false;
}

/** Base class declaring the field */
class Parent {
    int intField;
}

/** Derived class inherits the field */
class Child extends Parent { }
