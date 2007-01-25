package org.apache.harmony.drlvm.tests.regression.h2083;

import junit.framework.TestCase;

public class Test extends TestCase {
    static final int N_THREADS = 100;
    static final int N_ITERS = 500;

    public void test() throws Exception {
        Thread threads[] = new Thread[N_THREADS];

        for (int i = 0; i < N_THREADS; i++)
            threads[i] = new TestThread();

        System.out.println("START");
        for (int i = 0; i < N_THREADS; i++)
            threads[i].start();

        System.out.println("JOIN");
        for (int i = 0; i < N_THREADS; i++)
            threads[i].join();

        System.out.println("PASSED");
    }
}

class TestThread extends Thread {
    public void run() {
        for (int i = 0; i < Test.N_ITERS; i++) {
            try {
                new Missed();
            } catch (Throwable e) {}
        }
    }
}

class Missed {
}
