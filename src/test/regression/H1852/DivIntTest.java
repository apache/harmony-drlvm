package org.apache.harmony.drlvm.tests.regression.h1852;

import junit.framework.TestCase;

public class DivIntTest extends TestCase {

    public void testIDIV() {
        int i_min = Integer.MIN_VALUE;
        int i_1 = -1;
        int res = i_min / i_1;

        assertEquals(Integer.MIN_VALUE, res);
    }

    public void testIREM() {
        int i_min = Integer.MIN_VALUE;
        int i_1 = -1;
        int res = i_min % i_1;

        assertEquals(0, res);
    }
}
