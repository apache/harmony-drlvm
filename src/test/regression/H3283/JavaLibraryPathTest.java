package org.apache.harmony.drlvm.tests.regression.h3283;

import junit.framework.TestCase;

/**
 * Tests that VM doesn't crash in server mode with java.library.path property
 * redefined via command line option.
 */
public class JavaLibraryPathTest extends TestCase {

    public static void main(String args[]) {
        (new JavaLibraryPath()).test();
    }

    public void test() {
        String actual = System.getProperty("java.library.path");
        String expected = System.getProperty("expected.value");

        boolean status = actual.equals(expected);

        if (! status) {
            System.out.println("java.library.path = \"" + actual + "\"");
            System.out.println("expected.value = \"" + expected + "\"");
            System.out.println("Are not equal !!!");
        }

        assertEquals(expected, actual);
    }
}
