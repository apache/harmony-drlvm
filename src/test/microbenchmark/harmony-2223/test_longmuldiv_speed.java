import java.util.*;

/**
 * Microbenchmark on  64bit multiplications, division and remainder.
 */
public class test_longmuldiv_speed {
    public static void main(String[] args) {
        //
        // warm-up - force the method to be recompiled
        //
        System.out.println("Warming up ...");
        for (int i=0; i<20000; i++) {
            test(false);
        }
        //
        // The real measure
        //
    System.out.println("Measuring ...");
        long startTime = System.currentTimeMillis();
        test(true);
        long endTime = System.currentTimeMillis();
        //
        //
        long spentTime = endTime - startTime;
        System.out.println("... done.");
        System.out.println("The test took: "+spentTime+"ms");
    }

    static void test(boolean do_test) {
        int problem_size = do_test ? 10000000 : 5;
        int array_size = 300000;
        long[] array = new long[array_size];
        Random rndValue = new Random(0);

        for (int i=0; i<problem_size; i++) {
            int index = i % array.length;
            long v1 = rndValue.nextLong();
            long v2 = rndValue.nextLong();
            int what = (rndValue.nextInt() % 3);
            if (what == 0) {
                array[index] = v1*v2;
            }
            else if (what == 1) {
                array[index] = v1/v2;
            }
            else {
                array[index] = v1%v2;
            }
        }
    }
}