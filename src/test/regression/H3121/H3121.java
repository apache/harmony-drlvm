package org.apache.harmony.drlvm.tests.regression.h3121;

import junit.framework.TestCase;


/*  The test checks that the JIT correctly optimizes virtual calls
 *  when the receiver is of interface type. Loops are needed to force
 *  JIT recompilation. The method vc() contains both an interface call
 *  and a virtual call, an attempt to devirtualize a virtual call caused
 *  segfault in the ValueProfileCollector (Harmony-3121 issue).
 */
public class H3121 extends TestCase {

    Intf io = new IntfClass(); 
    Object o = new Object();

    public void test() {
	boolean b = false;
        for (int i = 0; i < 10000000; i++) {
            b = vc();
        }
        System.out.println("Test passed");
    }

    public boolean vc() {
        io.fake();
	return io.equals(o);
    }
}
 
interface Intf {
    public void fake();
}

class IntfClass implements Intf {
    public void fake() {
        for (int i = 0; i < 100; i++) {}
    }
}
