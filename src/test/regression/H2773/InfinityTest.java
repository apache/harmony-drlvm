package org.apache.harmony.drlvm.tests.regression.H2773;

import junit.framework.TestCase;

public class InfinityTest extends TestCase {
	public void test1() throws Exception {
		double a = -1.0d;
		double b = 0.0d;
		double c = a / b;
		assertEquals(Double.NEGATIVE_INFINITY, c);
	}

	public void test2() throws Exception {
		double a2 = 1.0d;
		double b2 = -0.0d;

		double c = a2 / b2;
		assertEquals(Double.NEGATIVE_INFINITY, c);
	}
}