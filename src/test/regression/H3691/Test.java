package org.apache.harmony.drlvm.tests.regression.h3691;

import junit.framework.TestCase;

public class Test extends TestCase {

    public void test() {
        assertEquals(50, foo()); 
    } 

    static int foo() { 
        int a = 10; 
        boolean b = false; 
        if ( (a > 10 ? 20 : 30) == 30 ) { 
            b = true; 
        } 

        if (!b) { 
            return 40; 
        } 
        return 50; 
    }
}
