package org.apache.harmony.drlvm.tests.regression.h4706;

/**
 * Checks that interruption of several threads doesn't cause VM crash due to
 * race condition in thread manager implementation for thread interrupting.
 * First main thread creates and starts a number of test threads.
 * Second main thead interrupts all of the test threads.
 * Each of the test threads waits on the same monitor.
 */
public class ThreadArrayInterrupt {

    static final int threadNum = 32;
    static Object barrier = new Object();

    public static void main(String[] args) {
        Thread[] threads = new Thread[threadNum];

        synchronized (barrier) {
            System.out.println("starting threads...");

            for (int i = 0; i < threadNum; i++) {
                threads[i] = new TestThread("Thread-" + i);
                threads[i].start();
            }

            System.out.println("all threads started");
        }

        System.out.println("Interrupting all threads...");

        for (int i = 0; i < threadNum; i++) {
            threads[i].interrupt();
        }
    }

    static class TestThread extends Thread {

        TestThread(String name) {
            super(name);
        }

        public void run() {
            synchronized (barrier) {
                try {
                    barrier.wait();
                } catch (InterruptedException e) {
                    System.out.println("Interrupted: " + getName());
                }
            }
        }
    }
}
