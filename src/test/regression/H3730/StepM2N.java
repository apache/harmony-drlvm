package org.apache.harmony.drlvm.tests.regression.h3730;

import junit.framework.TestCase;
import java.lang.reflect.Method;

/**
 * Test case for Single Step through the M2N frame.
 */
public class StepM2N extends TestCase {

    public static void main(String args[]) {
        (new StepM2N()).test();
    }

    public void test() {
        try {
        	System.out.println("[Java]: Begin");
        	outer();
        	System.out.println("[Java]: End");
        } catch (Exception e) {
        	e.printStackTrace();
        }

        System.out.println("[Java]: test done");
        assertTrue(Status.status);
    }

    void outer() throws Exception {
    	Class cls = this.getClass();
    	Method method = cls.getDeclaredMethod("inner");
    	method.invoke(null);
    	// should be stopped here after Step out
    }

    static void inner() {
    	// set breakpoint to the next line
    	System.out.println("[Java]: hello");
    }
}

class Status {
    /** the field should be modified by jvmti agent to determine test result. */
    public static boolean status = false;
}
