public class Test {
    public static void main(String [] argv) {
        // check invokespecial
        try { 
            Class cl = Class.forName("TestInvokeSpecial");
            cl.newInstance();
            System.out.println("TestInvokeSpecial: Fails");
        } catch (LinkageError e) {
            System.out.println("TestInvokeSpecial: Passes: " + e);
        } catch (Throwable e) {
            System.out.println("Test Failed, caught unexpected exception");
            e.printStackTrace(System.out);
        }
    }
}
