package org.apache.harmony.drlvm.tests.regression.h1859;

import junit.framework.TestCase;

public class Test extends TestCase {

    public void test() {
        boolean passed = false;
        try {
            foo(1, 2, 3, 4, 5, 6);
        } catch (StackOverflowError e) {
            passed = true;
        }
        assertTrue(passed);
        assertEquals(0, i);
    }

    static int i=0;

    static void foo(int i1, int i2, int i3, int i4, int i5, int i6) {
        try {
            i++;
            foo(i1, i2, i3, i4, i5, i6);
        } finally {
            i--;
        }
    }

}

