package org.apache.harmony.drlvm.tests.regression.h788;

import junit.framework.TestCase;

public class Test extends TestCase {

    public void test() throws Exception {
        // check multianewarray
        try { 
            Class cl = Class.forName("org.apache.harmony.drlvm.tests.regression.h788.TestArray");
            cl.newInstance();
            fail();
        } catch (LinkageError e) {
            System.out.println("TestArray:     Passes: " + e);
        }
        // check invokespecial
        try { 
            Class cl = Class.forName("org.apache.harmony.drlvm.tests.regression.h788.TestSpecial");
            cl.newInstance();
            fail();
        } catch (LinkageError e) {
            System.out.println("TestSpecial:   Passes: " + e);
        }
        // check invokevirtual
        try { 
            Class cl = Class.forName("org.apache.harmony.drlvm.tests.regression.h788.TestVirtual");
            cl.newInstance();
            fail();
        } catch (LinkageError e) {
            System.out.println("TestVirtual:   Passes: " + e);
        }        
        // check invokeinterface
        try { 
            Class cl = Class.forName("org.apache.harmony.drlvm.tests.regression.h788.TestInterface");
            cl.newInstance();
            fail();
        } catch (LinkageError e) {
            System.out.println("TestInterface: Passes: " + e);
        }
        // check invokestatic
        try { 
            Class cl = Class.forName("org.apache.harmony.drlvm.tests.regression.h788.TestStatic");
            cl.newInstance();
            fail();
        } catch (LinkageError e) {
            System.out.println("TestStatic:    Passes: " + e);
        }
    }
}
