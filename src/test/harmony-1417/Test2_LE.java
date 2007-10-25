public class Test2_LE { 
    static int num = 0;
    static int ln = 10000000;
    static int cln = ln/5;
    public static void main(String[] args) { 
        System.out.println("Start Test_LE:");
        Test2_LE test = new Test2_LE();
        long start1, start2, end1, end2, time1, time2;

        start1 = System.currentTimeMillis();
        for(int i=0; i<ln; i++) {
            try {
                if (i%cln == 0)
                    System.out.println("...");
                test.test();
            } catch(Exception e) {
            }
        }
        end1 = System.currentTimeMillis();
        System.out.println("Total time: " + (time1=end1-start1));

        start2 = System.currentTimeMillis();
        for(int i=0; i<ln; i++) {
            try {
                if (i%cln == 0) 
                    System.out.println("...");
                test.test2();
            } catch(Exception e) {
            }
        }
        end2 = System.currentTimeMillis();
        System.out.println("Total time: " + (time2=end2-start2));
        if (time2/time1 > 1)
            System.out.println("Test passed " + time2/time1);
        else
            System.out.println("Test failed " + time2/time1);
    }
    void test() throws Exception {
        Exception e = new Exception();
        throw e; 
    }
    void test2() throws Exception {
        Exception e = new Exception();
	if (e.getMessage()!=null)
	    System.out.println("null");
        throw e;
    }
} 
