package VMInit1;

import junit.framework.TestCase;

/**
 * Test case for VMInit event. Should be executed with all JVMTI capabilies
 * enabled.
 */
public class VMInit1 extends TestCase {
    public static void main(String args[]) {
        (new VMInit1()).test();
    }

    public void test() {
        System.out.println("test done");
        assertTrue(Status.status);
    }
}

class Status {
    public static boolean status = false;
}

