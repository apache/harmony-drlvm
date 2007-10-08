package org.apache.harmony.drlvm.tests.regression.h3382;
public class TestX {
	public static void main(String[] args) {
		foo();
	}  
	static Object foo() {
		try {
			return null;
		} catch(Exception e) {
			return null;
		} finally {
			System.out.println("SUCCESS");
		}
	}
}
