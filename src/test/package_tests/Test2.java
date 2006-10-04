public class Test2 {
    public static void main(final String[] args) throws Exception {
        try {
            Package p = Package.getPackage("java.lang");
            if (p==null) {
                System.out.println("Error0: test didn't work");
                return;
            }
            p.isSealed((java.net.URL)null);
            System.out.println("Test failed.");
        } catch (NullPointerException _) {
            System.out.println("Test passed.");
        }
    }
}
