package org.apache.harmony.drlvm.tests.regression.h3751;

import junit.framework.TestCase;
import org.vmmagic.unboxed.*;

public class Test extends TestCase {

    //both methods caused failure in OPT
    public static void testStatic() {
        Foo.testStatic();
    }
    
    public static void testNonStatic() {
        new Foo().testNonStatic();
    } 
} 


class Foo {
    static long val;
    static Address staticAddr;
    Address nonStaticAddr;

    static void testStatic() {
        Address localAddr = Address.fromLong(val);
        staticAddr = localAddr;
        val = staticAddr.toLong();
        System.gc();
        staticAddr = localAddr;
    }


    void testNonStatic() {
        Address localAddr = Address.fromLong(val);
        nonStaticAddr = localAddr;
        val = nonStaticAddr.toLong();
        System.gc();
        nonStaticAddr = localAddr;
    }

}

