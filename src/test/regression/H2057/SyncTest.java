
package org.apache.harmony.drlvm.tests.regression.H2057;

import junit.framework.TestCase;

public class SyncTest extends TestCase {


    public void testMain() {
        Waiter w = new Waiter();
        w.start();         
        while(!w.passed){}
    }

}

class Stopper extends Thread {
    Waiter w;
    Stopper(Waiter w) {this.w=w;}

    public void run() {
//        System.out.println("stopper started..");
        try {sleep(1000);} catch (Exception e) {e.printStackTrace();}
//        System.out.println("stopping..");
        w.finish();
//        System.out.println("stopped..");
    }
}

class Waiter extends Thread {
    boolean done = false;
    boolean passed = false;

    synchronized void finish(){
//        System.out.println("inside finish()..");
        done = true;
    }

    public void run() {
//        System.out.println("waiter started..");
        new Stopper(this).start();
        int i=0;
        while(!done) {
            synchronized(this) {
                i++;
            }
        }
        passed = true;
//        System.out.println("passed!");
    }
}
