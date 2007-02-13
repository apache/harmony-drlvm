package org.apache.harmony.drlvm.tests.regression.h3150;

import junit.framework.TestCase;

public class Test extends TestCase {

    public void test() throws Exception {

        try {
            classToDelete[] classToDeletearr = new classToDelete[2];
            fail();
        } catch (NoClassDefFoundError ok) {}
    }
}

class classToDelete {}

