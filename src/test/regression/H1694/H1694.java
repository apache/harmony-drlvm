package org.apache.harmony.drlvm.tests.regression.h1694;

import junit.framework.TestCase;

public class H1694 extends TestCase {

    public void test() {
        Object arrayOfObjects[] = new Object[10000]; // array of objects

        // padding memory
        System.out.println("Padding memory...");           
        int numObjects=0;
        
        try {
            while (true) {
                arrayOfObjects[numObjects] = new StressTest01Object1(); // padding memory by big objects
                numObjects++;
            }
        }
        catch (OutOfMemoryError oome) {
        }

        System.out.println("Test passed");
    }
}
 
/* big padding object */
class StressTest01Object1 {
    int testArray[][][] = new int[100][100][100];
}
