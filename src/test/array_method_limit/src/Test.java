public class Test {
    public static void main(String [] argv) {
        // check multianewarray
        try { 
            Class cl = Class.forName("TestArray");
            cl.newInstance();
            System.out.println("TestArray:     Fails");
        } catch (LinkageError e) {
            System.out.println("TestArray:     Passes: " + e);
        } catch (Throwable e) {
            System.out.println("Test Failed, caught unexpected exception");
            e.printStackTrace(System.out);
        }

        // check invokespecial
        try { 
            Class cl = Class.forName("TestSpecial");
            cl.newInstance();
            System.out.println("TestSpecial:   Fails");
        } catch (LinkageError e) {
            System.out.println("TestSpecial:   Passes: " + e);
        } catch (Throwable e) {
            System.out.println("Test Failed, caught unexpected exception");
            e.printStackTrace(System.out);
        }

        // check invokevirtual
        try { 
            Class cl = Class.forName("TestVirtual");
            cl.newInstance();
            System.out.println("TestVirtual:   Fails");
        } catch (LinkageError e) {
            System.out.println("TestVirtual:   Passes: " + e);
        } catch (Throwable e) {
            System.out.println("Test Failed, caught unexpected exception");
            e.printStackTrace(System.out);
        }
        
        // check invokeinterface
        try { 
            Class cl = Class.forName("TestInterface");
            cl.newInstance();
            System.out.println("TestInterface: Fails");
        } catch (LinkageError e) {
            System.out.println("TestInterface: Passes: " + e);
        } catch (Throwable e) {
            System.out.println("Test Failed, caught unexpected exception");
            e.printStackTrace(System.out);
        }

        // check invokestatic
        try { 
            Class cl = Class.forName("TestStatic");
            cl.newInstance();
            System.out.println("TestStatic:    Fails");
        } catch (LinkageError e) {
            System.out.println("TestStatic:    Passes: " + e);
        } catch (Throwable e) {
            System.out.println("Test Failed, caught unexpected exception");
            e.printStackTrace(System.out);
        }
    }
}
