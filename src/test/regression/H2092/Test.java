
package org.apache.harmony.drlvm.tests.regression.H2092;

import junit.framework.TestCase;

public class Test extends TestCase { 

    static int NTHREADS=10; 
    static int NSECONDS=10; 
    static volatile long val=1; 
    static boolean passed = true; 


    public void testMain() { 
        Worker wks[] = new Worker[NTHREADS]; 
        for (int i=0;i<NTHREADS;i++) { 
            wks[i]=new Worker(); 
            wks[i].start(); 
        } 
        try {
            for (int i=0;i<NTHREADS;i++) { 
                wks[i].join(); 
            } 
        } catch (Exception e) {
            passed = false;
        }
        assertTrue(passed);
/*        if (failed) { 
            System.out.println("FAILED"); 
        } else { 
            System.out.println("PASSED"); 
        } 
*/
    } 

    static class Worker extends Thread{ 
        public void run() { 
            long endTime = System.currentTimeMillis() + NSECONDS*1000L; 
            while (System.currentTimeMillis()<endTime && passed) { 
                for (int i=0;i<10000;i++) { 
                    long v = val; 
                    val = -v; 
                    if (v!=1 && v!=-1) { 
                        System.out.println("v="+v); 
                        passed = false; 
                        break; 
                    } 
                } 
            } 
        } 
    } 
}
