package org.apache.harmony.drlvm.tests.regression.h3909;

import junit.framework.TestCase;

/**
 * Test case for finalizer threads thread group
 */
public class FinalizerThreadGroup extends TestCase {

    public static void main(String args[]) {
        (new FinalizerThreadGroup()).test();
    }

    public void test() {

        try {
            System.err.println("[Java]: Throwing an exception to invoke agent");
            // pass execution to the agent
            throw new InvokeAgentException();
        } catch (Exception e) {
            System.err.println("[Java]: Exception caught");
        }

        System.err.println("[Java]: test done");
        assertTrue(Status.status);
    }
}

class InvokeAgentException extends Exception {}

class Status {
    /** the field should be modified by jvmti agent to determine test result. */
    public static boolean status = false;
}
