package org.apache.harmony.drlvm.tests.regression.h4654;

/**
 * Tests that VM doesn't crash in printStackTrace() for OOME.
 */

public class OOMEPrintStackTrace {

    public static void main(String[] args) {
        int size = 1;

        try {
            for (int i = 0; 0 < 10; i++) {
                System.out.println("Allocating 10^" + i + " longs...");
                long[] b = new long[size];
                size *= 10;
            }
        } catch (OutOfMemoryError e) {
            printStackTrace(e);
        }
    }

    static public void printStackTrace(Throwable e) {
        e.printStackTrace();
    }

}
