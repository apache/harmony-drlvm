package org.apache.harmony.drlvm.tests.regression.h3738;

import junit.framework.TestCase;
import org.vmmagic.unboxed.*;

public class Test extends TestCase {

    static Address a;
    static long val;

    public static void test() {
        a = Address.fromLong(1L);
        //crash in I8Lowerer here
        val = a.toLong();
    } 
} 

