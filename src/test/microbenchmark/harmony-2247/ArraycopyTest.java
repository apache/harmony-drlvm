/**
 * Microbenchmark for System.arraycopy.
 * Should be run with -Xms1048M -Xmx1048M options.
 */
class ArraycopyTest {

    public static int length = 100000000;

    public static void copy(int[] src, int srcPos, int[] dst, int dstPos,
            int length) {

        System.arraycopy(src, srcPos, dst, dstPos, length);
    }

    public static void main(String[] args) {
        int arrA[] = new int[length];
        int arrB[] = new int[length];

        for (int i = 0; i < length; i++) {
            arrA[i] = i;
        }
        // this part is for debugging
        /*
        * printArr(arrA,0,10, "arrA initial"); printArr(arrB,0,10, "arrB
        * initial");
        * 
        * System.arraycopy(arrA,1,arrB,3,5); printArr(arrB,0,10, "arrB
        * changed");
        * 
        * System.arraycopy(arrB,3,arrB,8,5); printArr(arrB,0,10, "arrB self
        * forward");
        * 
        * System.arraycopy(arrB,4,arrB,0,3); printArr(arrB,0,10, "arrB self
        * backward");
        */

        if (args.length == 0) {
            args = new String[]{"100"};
        }
        for (String s : args) {
            int lim = Integer.parseInt(s);

            System.out.println("");
            System.out.println("START!");

            long start = System.currentTimeMillis();

            for (int i = 0; i < lim; i++) {
                copy(arrA, i, arrB, i + 2, length - i - 2);
                // System.out.print(". ");
            }

            long end = System.currentTimeMillis();

            System.out.println("FINISHED");

            System.out.println("duration = " + (end - start) + " millis");
        }
    }

    public static void printArr(int[] arr, int pos, int count, String prefix) {

        String out = prefix + " : ";
        for (int i = pos; i < pos + count; i++) {
            out = out + arr[i] + " ";
        }
        System.out.println(out);
    }
}
