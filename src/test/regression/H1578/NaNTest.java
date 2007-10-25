package org.apache.harmony.drlvm.tests.regression.H1578;

import junit.framework.TestCase;

public class NaNTest extends TestCase {
    
    public void testDcmp() throws Exception {
        assertEquals(1, dcmp1.test());
    }
    
    public void testFcmp() throws Exception {
        assertEquals(-1, fcmp1.test());
    }
    
    public void testFloatNaN() {
        assertTrue(Float.isNaN(Float.NaN));
    }
    
    public void testDoubleNaN() {
        assertTrue(Double.isNaN(Double.NaN));
    }
}