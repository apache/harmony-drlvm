package org.apache.harmony.drlvm.tests.regression.h3067;

import junit.framework.TestCase;

/**
 * Loads class and tries to invoke a method which should fail
 * verification.
 *
 * <code>wide</code> bytecode should be followed by an instruction
 * which uses local variables. We patch WIDE_CLASS class, so <code>wide</code>
 * is followed by <code>goto</code>. This should fail verification.
 */
public class Test extends TestCase {
    public static void main(String args[]) {
        (new Test()).test();
    }

    final static int NOPS = 20;
    final static String PACKAGE_NAME = Test.class.getPackage().getName();
    final static String WIDE_CLASS = PACKAGE_NAME + ".WideGoto";
    final static int OPCODE_WIDE = 0xC4;

    public void test() {
        final Loader loader = new Loader();
        try {
            Class c = loader.loadClass(WIDE_CLASS);
            ((Test) c.newInstance()).test();
        } catch (VerifyError ve) {
            return;
        } catch (Exception e) {
        }
        fail("A method of SupClass class should throw VerifyError");
    }


    class Loader extends ClassLoader {
        final static int LENGTH = 50000;

        public Class loadClass(String name) throws ClassNotFoundException {
            if (!name.equals(WIDE_CLASS)) {
                return getParent().loadClass(name);
            }
            final String path = name.replace('.', '/') + ".class";
            java.io.InputStream is = ClassLoader.getSystemResourceAsStream(path);
            if (is == null) {
                System.out.println("Cannot find " + path);
                return null;
            }
            int offset, nops = 0;
            byte[] buffer = new byte[LENGTH];
            for (offset = 0; ; offset++) {
                int b;
                try {
                    b = is.read();
                } catch (java.io.IOException ioe) {
                    return null;
                }
                if (b == -1) {
                    break;
                }
                if (offset == LENGTH) {
                    System.out.println("Class too big, please increase LENGTH = "
                            + LENGTH);
                    return null;
                }

                if (b == 0) {
                    nops++;
                    if (nops == NOPS) {
                        b = OPCODE_WIDE;
                    }
                } else {
                    nops = 0;
                }
                buffer[offset] = (byte) b;
            }
            try {
                return defineClass(name, buffer, 0, offset);
            } catch (Exception e) {
                return null;
            }
            
        }
    }
}


