public class Test {
    public static void main(String[] args) {
        // verify aastore instruction
        try {            
            new TestAastore().test();
            System.out.println("TestAastore: failed");
        } catch (LinkageError e) {
            System.out.println("TestAastore: passed: " + e);
        } catch (Throwable e) {
            System.out.println("TestAastore: failed: unexpected error " + e);
            e.printStackTrace();
        }
   }
}