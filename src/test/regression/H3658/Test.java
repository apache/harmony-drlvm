package org.apache.harmony.drlvm.tests.regression.h3658;

import junit.framework.TestCase;

public class Test extends TestCase {

    public void test() {
        int x = 1;
        try {
            new Foo(x=0);
        } catch (Error e) {}
       assertEquals(1, x); 
    } 
}

class Foo {
    static {
        exception();
    }

    static void exception() {
        throw new RuntimeException();
    }

    Foo(int x) {
        System.out.println("FAILED!");
    }
} 

