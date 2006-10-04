public class Test1 {
    public static void main(final String[] args) throws Exception {
        Package p = Package.getPackage("java.lang");
        if (p==null) {
            System.out.println("Error0: test didn't work");
            return;
        }
        try {
            p.isCompatibleWith("");
            System.out.println("Error1: NumberFormatException should be thrown");
        } catch (NumberFormatException _) {
            System.out.println("Test passed.");
        } catch (Throwable e) {
              e.printStackTrace();
              System.out.println("Error2: " + e.toString());
        }
    }
}
