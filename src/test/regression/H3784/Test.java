package org.apache.harmony.drlvm.tests.regression.h3784;

import junit.framework.TestCase;
import org.vmmagic.unboxed.*;

public class Test extends TestCase {

    static {
        System.loadLibrary("check");
    }

    public static final Address staticField = Address.fromLong(getAddress());

    public static final long staticVal = -1;
    public static final Address staticField2 = Address.fromLong(staticVal);
 
    public static void test1() {
        boolean result = check(staticField.toLong());
        assertTrue(result);
    }


    public static void test2() {
        long val = staticField.toLong();
        int ptrSize = getPointerSize();
        if (ptrSize == 4) {
            assertEquals((int)val, (int)staticVal);
        } else {
            assertTrue(ptrSize == 8);
            assertEquals(val, staticVal);
        }
    }
    
    static native long    getAddress();
    static native boolean check(long addr);
    static native int     getPointerSize();
}
