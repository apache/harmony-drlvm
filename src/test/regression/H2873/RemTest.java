package org.apache.harmony.drlvm.tests.regression.H2873;

import junit.framework.TestCase;

public class RemTest extends TestCase {
    
    public void testDcmp() throws Exception {
        double d1 = 3.3d;
        double d2 = Double.MIN_VALUE;
        double dd = d1 % d2;
        assertEquals(0.0d, dd);
    }
    
    public void testFrem() throws Exception {
        float f1 = 5.5f;
        float f2 = Float.MIN_VALUE;
        float ff = f1 % f2;
        assertEquals(0.0f, ff);
    }    
}