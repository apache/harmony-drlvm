package org.apache.harmony.drlvm.tests.regression.h2261;

import junit.framework.TestCase;

public class RCETest extends TestCase
{
    private int maximum, minimum, visibleAmount;

    public RCETest()
    {
        visibleAmount = 10;
        minimum = -100;
    }

    public static void main(String[] args)
    {
        (new RCETest()).test();
    }

    public void test() {
        minimum = -2147483648;
        maximum = 100;
        System.out.println(".");
        if (maximum - minimum < 0) {
            maximum = minimum + Integer.MAX_VALUE;
        }
        visibleAmount = Math.min(maximum - minimum, visibleAmount);
        assertTrue(visibleAmount == 10);
    }

}
