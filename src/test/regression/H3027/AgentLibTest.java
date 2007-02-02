package org.apache.harmony.drlvm.tests.regression.h3027;

import junit.framework.TestCase;

/**
 * Test case for -agentlib: command line option. 
 * Should be executed with all JVMTI capabilie enabled.
 */
public class AgentLibTest extends TestCase {
    public static void main(String args[]) {
        (new AgentLibTest()).test();
    }

    public void test() {
        System.out.println("test done");

        assertTrue(Status.status);
    }
}

class Status {
    public static boolean status = false;
}

