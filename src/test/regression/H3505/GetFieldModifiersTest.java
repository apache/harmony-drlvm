package org.apache.harmony.drlvm.tests.regression.h3505;

import junit.framework.TestCase;

/**
 * Test case for GetFieldModifiers() jvmti function.
 */
public class GetFieldModifiersTest extends TestCase {
    public static void main(String args[]) {
        (new GetFieldModifiersTest()).test();
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
