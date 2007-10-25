import java.util.*;

/**
 * Microbenchmark for float & integer computations.
 */
public class test_f2i_speed {
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
        int[] array = new int[array_size];
        Random rndValue = new Random(0);

        for (int i=0; i<problem_size; i++) {
            int index = i % array.length;
            float value = rndValue.nextFloat();
            array[index] = (int)value;
        }
    }
}