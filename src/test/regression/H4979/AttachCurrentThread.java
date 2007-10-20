package org.apache.harmony.drlvm.tests.regression.h4979;

import junit.framework.TestCase;

/**
 * Test case for AttachCurrentThread() Invocation API function.
 */
public class AttachCurrentThread extends TestCase {

    public static void main(String args[]) {
        (new AttachCurrentThread()).test();
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
