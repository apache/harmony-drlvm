package org.apache.harmony.drlvm.tests.regression.h4267;

import junit.framework.TestCase;

public class UnresolvedArrayElem extends TestCase {

    public void test() throws Exception {
       try {
           new TClass().test1(); 
           System.out.println("FAILED: NoClassDefFoundError was not thrown");
           fail();
       } catch (NoClassDefFoundError e) {
           System.out.println("PASSED: " + e.getMessage());
       }
    }
}

class TClass { 
    
    void test1() { 
        RemoveMe[] arr = test(); 
        RemoveMe element = arr[3]; 
    } 

    RemoveMe[] test() { 
        return new RemoveMe[10]; 
    } 
} 

class RemoveMe {} 
