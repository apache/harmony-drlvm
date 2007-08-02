package org.apache.harmony.drlvm.tests.regression.h4595;

import junit.framework.*;

public class Test extends TestCase {


/// test 1
    class A {
        void in(long l1, long l2, long l3){
            assertEquals(10000002, l1);
        }
    }

    XGraphics2D g2d = new XGraphics2D();
    A a = new A();

    public void test1() {
        long l = get();
        before(l);
        a.in(g2d.display, g2d.drawable, g2d.imageGC);
    }

    long get(){return 4;}
    void before(long l){ /*do nothing*/}




/// test 2
    public void test2() {
        long[] x = new long[] { g2d.drawable};
        //check that no exception is thrown
    }

/// test 3
    
    double d = 30d;
    static Test t = new Test();
    static long [] arr = new long [] {6, 25, 50};
    
    public void test3() {
        double v = t3();
        assertEquals(v, 5d);
    }

    double t3() {
        double d1 = t.d / arr[0];
        return d1;
    }

}

class XGraphics2D {
    long drawable = 10000001;
    long display  = 10000002;
    long imageGC  = 10000003;
}
