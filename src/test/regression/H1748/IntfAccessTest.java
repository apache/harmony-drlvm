package org.apache.harmony.drlvm.tests.regression.h1748;

import junit.framework.TestCase;

public class IntfAccessTest extends TestCase{

    public void test() {
		try {
		 	Intf i = new BadIntfImpl();
			i.test();
			fail("Test failed, IllegalAccessError is not thrown");
		} catch (IllegalAccessError ok) {
		 	System.out.println("Test passed");
			ok.printStackTrace();
		}
    }
}


interface Intf {

	void test();
}
