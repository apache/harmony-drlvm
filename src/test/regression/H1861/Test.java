package org.apache.harmony.drlvm.tests.regression.h1861;

import junit.framework.TestCase;

public class Test extends TestCase {
    public static long [] arr = new long [] {6, 25, 50};
    public static Test t = new Test();
    public double d = 30d;


    public void test() {
        double d1 = t.d / arr[0];
        assertEquals(5.0d, d1, 0);

        d1 = t.d % arr[1];
        assertEquals(5.0d, d1, 0);

        d1= t.d - arr[1];
        assertEquals(5.0d, d1, 0);

        d1= t.d + arr[0];
        assertEquals(36.0d, d1, 0);

        boolean b = (t.d >= arr[0]);
        assertTrue(b);

        b = (t.d < arr[1]);
        assertFalse(b);
    }
}
