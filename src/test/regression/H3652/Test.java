package org.apache.harmony.drlvm.tests.regression.h3652;

import junit.framework.TestCase;

public class Test extends TestCase {

    public void test() {
       // assertEquals(20, foo()); 
       foo(10);
       System.out.println(buf); 
    } 

    String buf =""; 
    public Test foo(long l) { 
        boolean flag = false; 
        if (l >= 20L) { 
            flag = true; 
        } 
        if(flag || l >= 10L) { 
            int k = (int)(l / 11L); 
            writeDigits(k, flag); 
        } 
        return this; 

    } 

    void writeDigits(int i, boolean flag) { 
        buf+=i; 
    } 
}
