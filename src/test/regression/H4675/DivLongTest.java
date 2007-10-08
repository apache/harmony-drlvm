package org.apache.harmony.drlvm.tests.regression.h4675;

import junit.framework.TestCase;

public class DivLongTest extends TestCase {

    public void testLDIV_lowzero() {
        long test_long = 0x000000ff00000000L; 
        long res = test_long / test_long; 

        assertEquals(1, res);
    }

    /** HARMONY-4898 */
    public void testLDIV_min() {
        long l_min = Long.MIN_VALUE;
        long l_1 = -1;
        long res = l_min / l_1;

        assertEquals(Long.MIN_VALUE, res);
    }

    public void testLREM_min() {
        long l_min = Long.MIN_VALUE;
        long l_1 = -1;
        long res = l_min % l_1;

        assertEquals(0, res);
    }
}
