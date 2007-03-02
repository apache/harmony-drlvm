package org.apache.harmony.drlvm.tests.regression.h3228;

import junit.framework.TestCase;


/*  The test checks that the JIT correctly optimizes nested monenter/monexit
 *  pairs when they are enclosed into another monenter/monexit with the same
 *  synchronization object.
 *  The synchronized method sync_run() contains a synchronized statement which
 *  can be safely removed. The issue described in Harmony-3228 caused
 *  a crash during the JIT compilation stage.
 */
public class H3228 extends TestCase {

    public void test() {
        boolean b = false;
        for (int i = 0; i < 1000000000; i++) {
            b = sync_run();
        }
        System.out.println("Test passed");
    }

    public synchronized boolean sync_run() {
        boolean a = false;
        synchronized (this) {
            a = !a;
        }
        return a;
    }
}
