package org.apache.harmony.drlvm.tests.regression.h1857;

import junit.framework.TestCase;

public class Test extends TestCase {

    public void test() throws Exception {
       try {
            new TestClass();
            fail("NoClassDefFoundError expected. Misconfiguration?");
        } catch (NoClassDefFoundError ok) {}
    }
}

class MissedThrowable extends Throwable {}

class TestClass {
    public void run() throws MissedThrowable {}

    public void tryMe() {
        try {
            run();
        } catch (MissedThrowable e) {}
    }
}
