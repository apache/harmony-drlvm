package org.apache.harmony.drlvm.tests.regression.h2103;

import junit.framework.TestCase;

/**
 * Loads class and tries to invoke a method which should fail
 * verification.
 *
 * SubClass contains an incorrect invokespecial instruction which invokes
 * a method from a subclass of the current class, while only superclass
 * constructors can be called using this instruction.
 */
public class Test extends TestCase {
    public static void main(String args[]) {
        (new Test()).test();
    }

    public void test() {
        try {
            SupClass.test();
        } catch (VerifyError ve) {
            return;
        } catch (Exception e) {
        }
        fail("A method of SupClass class should throw VerifyError");
    }
}


