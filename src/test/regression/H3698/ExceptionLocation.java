package org.apache.harmony.drlvm.tests.regression.h3698;

import junit.framework.TestCase;

/**
 * Test case for Exception event location parameter.
 */
public class ExceptionLocation extends TestCase {

    public static void main(String args[]) {
        (new ExceptionLocation()).test();
    }

    public void test() {
        try {
            System.out.println("[Java]: Throwing an exception");
            throw new InvokeAgentException();
        } catch (Exception e) {
            System.out.println("[Java]: Exception caught");
        }

        System.out.println("[Java]: test done");
        assertTrue(Status.status);
    }
}

class InvokeAgentException extends Exception {}

class Status {
    /** the field should be modified by jvmti agent to determine test result. */
    public static boolean status = false;
}
