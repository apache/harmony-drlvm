package org.apache.harmony.drlvm.tests.regression.h2644;

import junit.framework.TestCase;

public class BootDelegationTest extends TestCase {

    public void test1() throws Exception {
        try {
            System.out.println("SubClass=" + Class.forName("org.apache.harmony.drlvm.tests.regression.h2644.SubClass"));
            fail("Misconfigured!");
        } catch (ClassNotFoundException e) {
            fail();
        } catch (NoClassDefFoundError ok) {}
    }

    public void test2() throws Exception {
        try {
            System.out.println("SubClass=" + Class.forName("org.apache.harmony.drlvm.tests.regression.h2644.SubClass", true, null));
            fail("Misconfigured!");
        } catch (ClassNotFoundException e) {
            fail();
        } catch (NoClassDefFoundError ok) {}
    }
}

class SuperClass {}
class SubClass extends SuperClass {}


