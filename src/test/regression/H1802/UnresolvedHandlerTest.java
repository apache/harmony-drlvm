package org.apache.harmony.drlvm.tests.regression.h1802;

import junit.framework.TestCase;

public class UnresolvedHandlerTest extends TestCase {

    public void test() throws Exception {
       try {
           X x = new X();
           x.foo(false);
           fail();
       } catch (NoClassDefFoundError ok) {}
    }

    static class X {

    public void foo(boolean a) {
        try {
            if (a) {
                throw new MissedThrowable();
            }
        } catch (MissedThrowable e) {
            System.out.println(e.getMessage());
        }
    }
    }
}

class MissedThrowable extends Throwable {}
