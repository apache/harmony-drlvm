package org.apache.harmony.drlvm.tests.regression.h3862;

import junit.framework.TestCase;

public class SubrBounds extends TestCase {
    
    public void testPos() throws Exception {
        pos.test();
    }
    
    public void testNeg() throws Exception {
        try {
            neg.test();
            fail("VerifyError expected");
        } catch( VerifyError e) {
        }
    }

    public void testPos2() throws Exception {
        pos2.test();
    }
    
    public void testNeg2() throws Exception {
        try {
            neg2.test();
            fail("VerifyError expected");
        } catch( VerifyError e) {
        }
    }
    
    public void testNeg3() throws Exception {
        try {
            neg3.test();
            fail("VerifyError expected");
        } catch( VerifyError e) {
        }
    }
}