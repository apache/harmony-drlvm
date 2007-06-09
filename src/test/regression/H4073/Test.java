package org.apache.harmony.drlvm.tests.regression.h4073;

import junit.framework.TestCase;

public class Test extends TestCase {

    public static void main(String args[]) {
        (new Test()).test();
    }

    public void test() {
        assertTrue(lessThenZero(Long.MIN_VALUE));
    }

    boolean lessThenZero(long v) {
        v = -v;
        return v < 0; 
    }
}