package org.apache.harmony.drlvm.tests.regression.h2846;

import junit.framework.TestCase;

public class NoVerifyTest extends TestCase {

    public void test() throws Exception {
        for (int i = 0; i < 3; i++) {
            try {
                Class cl = Class.forName("org.apache.harmony.drlvm.tests.regression.h2846.NoVerify");  
                fail();
            } catch (VerifyError ok) {}
        }
    }
}
