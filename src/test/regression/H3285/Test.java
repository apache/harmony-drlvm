package org.apache.harmony.drlvm.tests.regression.h3285;

import junit.framework.TestCase;

public class Test extends TestCase {

    public void test() throws Exception {
        process(123L);
    }

    void process(long ll) {
        boolean flag = false;

        if(ll >= 0L && ll > 0L && ll > 10L)
        {
            flag = true;        
        }   
    }
}
