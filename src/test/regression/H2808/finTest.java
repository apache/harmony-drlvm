package org.apache.harmony.drlvm.tests.regression.h2808;

import junit.framework.TestCase;

public class finTest extends TestCase {
    public void test() {
        try {
            new finTest1().test();
            fail("Test failed. VerifyError was not thrown");
        } catch (VerifyError e) {
            System.out.println("Test passed: "+e);
        } catch (Throwable e) {
            fail("Test failed: unexpected "+e);
            e.printStackTrace();
        }
    }
}

