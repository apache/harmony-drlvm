package org.apache.harmony.drlvm.tests.regression.h3216;

import junit.framework.TestCase;

public class ASETest extends TestCase { 
    Object[] oo1 = new String[3];

    public void testASE() { 
        
        Integer[] oo2 = new Integer[oo1.length]; 
        for (int i=0; i<oo2.length; i++) { 
            oo2[i] = new Integer(i); 
        } 
        try { 
            System.arraycopy(oo2, 0, oo1, 0, oo1.length); 
            fail("ArrayStoreException should be thrown");
        } catch (ArrayStoreException ok) {} 
    } 
} 
