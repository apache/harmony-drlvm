public class Test {
    public static void main(String [] argv) {
        // check empty init method
        try { 
            Class cl = Class.forName("TestEmptyInit");
            System.out.println("TestEmptyInit: Fails");
        } catch (LinkageError e) {
            System.out.println("TestEmptyInit: Passes: " + e);
        } catch (Throwable e) {
            System.out.println("TestEmptyInit: Failed, caught unexpected exception");
            e.printStackTrace(System.out);
        }
    }
}
