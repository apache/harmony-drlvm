package org.apache.harmony.drlvm.tests.regression.h1802;

import junit.framework.TestCase;

public class UnresolvedParamTest extends TestCase {

    public void test() throws Exception {
       try {
           X.foo(null);
       } catch (NoClassDefFoundError ok) {}
    }
    static class X {
       static void foo(Missed m){
           System.out.println(m);
       }
    }
}

class Missed {
}
