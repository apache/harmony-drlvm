package org.apache.harmony.drlvm.tests.regression.h3225;

/**
 * The class launches methods which contain invalid <code>jsr</code> usage and
 * should be rejected by a verifier.
 */
public class NegativeJsrTest extends junit.framework.TestCase {
    public static void main(String args[]) {
        junit.textui.TestRunner.run(org.apache.harmony.drlvm.tests.regression.h3225.NegativeJsrTest.class);
    }

    private void checkVerifyError() {
        final String testMethodName = Thread.currentThread().getStackTrace()[3].getMethodName();
        assertEquals("test", testMethodName.substring(0, 4));

        final String testClassName = "org.apache.harmony.drlvm.tests.regression.h3225."
           + testMethodName.substring(4) + "NegativeTest";
        try {
            Class.forName(testClassName).getConstructors();
        } catch (VerifyError ve) {
            return;
        } catch (ClassNotFoundException cnfe) {
            fail("Failed to load " + testClassName);
        }
        fail(testClassName + " should throw java.lang.VerifyError");
    }

    public void testMergeExecution() {
        checkVerifyError();
    }

    public void testMergeEmptyStack() {
        checkVerifyError();
    }

    public void testMergeIntFloat() {
        checkVerifyError();
    }

    public void testMergeStack() {
        checkVerifyError();
    }

    public void testRetOrder() {
        checkVerifyError();
    }
}

