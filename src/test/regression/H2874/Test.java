package org.apache.harmony.drlvm.tests.regression.H2874;

import junit.framework.TestCase;



class aeo0 {
    private long num;
    public aeo0() { num = 0; }
    public void inc(aeo1 i) { num++; }
    public void inc1() { num++; }
    public long getNum() { return num; }
    public void reset() { num = 0; }
}
class aeo1 {
}

public class Test extends TestCase {
    static final long limit = 100000000;
    static aeo0 obj = new aeo0();

    public void test() {
        long before = 0, after = 0;
        for (int i = 0; i < 5; i++) {    
            obj.reset();
            before = System.currentTimeMillis();
            for (long k = 0; k < limit; k++ ) {
                dofc(k);
            }
            after = System.currentTimeMillis();
            System.out.println("Calls per millisecond: " + (obj.getNum() / (after - before)));
        }
    }
    static void dofc(long i) {
        aeo1 i1 = new aeo1();
        obj.inc1();
        if (i<0) {
            obj.inc(i1);
        }
    }
}
