package org.apache.harmony.drlvm.tests.regression.h4579;

import junit.framework.TestCase;

public class test extends TestCase {
    
    public void testNeg() throws Exception {
        try {
            neg.test();
            fail("VerifyError expected");
        } catch( VerifyError e) {
        }
    }
}