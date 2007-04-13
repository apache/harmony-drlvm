package org.apache.harmony.drlvm.tests.regression.h3619;

import junit.framework.TestCase;

public class ObjectLongWaitTest extends TestCase {
    
    public static void main(String args[]) throws Exception {
       (new ObjectLongWaitTest()).test();
    }

    public void test() throws Exception {
        TestThread testThread = new TestThread();
        System.out.println("TestThread is starting...");
        testThread.start();

        Thread.sleep(2000); // give time to TestThread to behave
        
        if (testThread.finished) {
            testThread.join();
            fail("TestThread is returned from Object.wait(Long.MAX_VALUE)");
        } else {
            System.out.println("TestThread expectedly hanged up. Interrupting...");
            testThread.interrupt();
        }
    }
}

class TestThread extends Thread {

    public volatile boolean finished = false;

    public void run() {
        try {
            System.out.println("TestThread started");
            String lock = "lock";
            synchronized (lock) {
                lock.wait(Long.MAX_VALUE);
            }
            finished = true;
            System.out.println("Object.wait(Long.MAX_VALUE) exited in couple of seconds");
        } catch (InterruptedException e) {
        }
    }
}
