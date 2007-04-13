package org.apache.harmony.drlvm.tests.regression.h3225;

/**
 * The class launches methods which contain invalid <code>jsr</code>
 * usage and should be rejected by a verifier.
 */
public class NegativeJsrTest extends junit.framework.TestCase {
    public static void main(String args[]) {
        NegativeJsrTest t = new NegativeJsrTest();
        t.testMergeExecution();
        t.testMergeEmptyStack();
        t.testMergeIntFloat();
        t.testMergeStack();
    }

    public void testMergeExecution() {
        try {
            MergeExecutionNegativeTest.test();
        } catch (VerifyError ve) {
            return;
        }
        fail("The test should throw java.lang.VerifyError");
    }

    public void testMergeEmptyStack() {
        try {
            MergeEmptyStackNegativeTest.test();
        } catch (VerifyError ve) {
            return;
        }
        fail("The test should throw java.lang.VerifyError");
    }

    public void testMergeIntFloat() {
        try {
            MergeIntFloatNegativeTest.test();
        } catch (VerifyError ve) {
            return;
        }
        fail("The test should throw java.lang.VerifyError");
    }

    public void testMergeStack() {
        try {
            MergeStackNegativeTest.test();
        } catch (VerifyError ve) {
            return;
        }
        fail("The test should throw java.lang.VerifyError");
    }
}

