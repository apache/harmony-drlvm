package org.apache.harmony.drlvm.tests.regression.h3380;

import java.util.Formatter;
import junit.framework.TestCase;

public class LongToBitsTest extends TestCase {

    public void test() {
        long a = 0x00100000000L; 
        long b = 0x01010101010L; 
        assertEquals("1110101010", format(a,b)); 
    } 

    static String format( long a, long b ) { 
        StringBuilder sb = new StringBuilder();
        new Formatter(sb).format("%1$x", (a|b) ); 
        return sb.toString();
    } 
}
