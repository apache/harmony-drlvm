/**
 * Microbenchmark for System.arraycopy.
 * Should be run with -Xms1048M -Xmx1048M options.
 */
class ArraycopyTest {

    public static int length = 100000000;

    public static void main(String[] args) {

        int arrA[] = new int[length];
        int arrB[] = new int[length];

        for (int i=0;i<length;i++) {
            arrA[i] = i;
        }
// this part is for debugging
/*
        printArr(arrA,0,10, "arrA initial");
        printArr(arrB,0,10, "arrB initial");
        
        System.arraycopy(arrA,1,arrB,3,5);
        printArr(arrB,0,10, "arrB changed");
        
        System.arraycopy(arrB,3,arrB,8,5);
        printArr(arrB,0,10, "arrB self forward");
        
        System.arraycopy(arrB,4,arrB,0,3);
        printArr(arrB,0,10, "arrB self backward");
*/        
        System.out.println("");
        System.out.println("START!");

        long start = System.currentTimeMillis();

        System.arraycopy(arrA,1,arrB,0,length-1);
        System.arraycopy(arrA,1,arrB,0,length-1);
        System.arraycopy(arrA,1,arrB,0,length-1);
        System.out.print(". ");
        System.arraycopy(arrA,2,arrB,0,length-2);
        System.arraycopy(arrA,2,arrB,0,length-2);
        System.arraycopy(arrA,2,arrB,0,length-2);
        System.out.print(". ");
        System.arraycopy(arrA,3,arrB,0,length-3);
        System.arraycopy(arrA,3,arrB,0,length-3);
        System.arraycopy(arrA,3,arrB,0,length-3);
        System.out.print(". ");
        System.arraycopy(arrA,4,arrB,0,length-4);
        System.arraycopy(arrA,4,arrB,0,length-4);
        System.arraycopy(arrA,4,arrB,0,length-4);
        System.out.print(". ");
        System.arraycopy(arrA,5,arrB,0,length-5);
        System.arraycopy(arrA,5,arrB,0,length-5);
        System.arraycopy(arrA,5,arrB,0,length-5);
        System.out.print(". ");
        System.arraycopy(arrA,6,arrB,0,length-6);
        System.arraycopy(arrA,6,arrB,0,length-6);
        System.arraycopy(arrA,6,arrB,0,length-6);
        System.out.print(". ");
        System.arraycopy(arrA,7,arrB,0,length-7);
        System.arraycopy(arrA,7,arrB,0,length-7);
        System.arraycopy(arrA,7,arrB,0,length-7);
        System.out.print(". ");
        System.arraycopy(arrA,2,arrB,0,length-2);
        System.arraycopy(arrA,2,arrB,0,length-2);
        System.arraycopy(arrA,2,arrB,0,length-2);
        System.out.print(". ");
        System.arraycopy(arrA,3,arrB,0,length-3);
        System.arraycopy(arrA,3,arrB,0,length-3);
        System.arraycopy(arrA,3,arrB,0,length-3);
        System.out.print(". ");
        System.arraycopy(arrA,4,arrB,0,length-4);
        System.arraycopy(arrA,4,arrB,0,length-4);
        System.arraycopy(arrA,4,arrB,0,length-4);
        System.out.print(". ");
        System.arraycopy(arrA,5,arrB,0,length-5);
        System.arraycopy(arrA,5,arrB,0,length-5);
        System.arraycopy(arrA,5,arrB,0,length-5);
        System.out.print(". ");
        System.arraycopy(arrA,6,arrB,0,length-6);
        System.arraycopy(arrA,6,arrB,0,length-6);
        System.arraycopy(arrA,6,arrB,0,length-6);
        System.out.print(". ");
        System.arraycopy(arrA,7,arrB,0,length-7);
        System.arraycopy(arrA,7,arrB,0,length-7);
        System.arraycopy(arrA,7,arrB,0,length-7);
        System.out.print(". ");
        System.arraycopy(arrA,2,arrB,0,length-2);
        System.arraycopy(arrA,2,arrB,0,length-2);
        System.arraycopy(arrA,2,arrB,0,length-2);
        System.out.print(". ");
        System.arraycopy(arrA,3,arrB,0,length-3);
        System.arraycopy(arrA,3,arrB,0,length-3);
        System.arraycopy(arrA,3,arrB,0,length-3);
        System.out.print(". ");
        System.arraycopy(arrA,4,arrB,0,length-4);
        System.arraycopy(arrA,4,arrB,0,length-4);
        System.arraycopy(arrA,4,arrB,0,length-4);
        System.out.print(". ");
        System.arraycopy(arrA,5,arrB,0,length-5);
        System.arraycopy(arrA,5,arrB,0,length-5);
        System.arraycopy(arrA,5,arrB,0,length-5);
        System.out.print(". ");
        System.arraycopy(arrA,6,arrB,0,length-6);
        System.arraycopy(arrA,6,arrB,0,length-6);
        System.arraycopy(arrA,6,arrB,0,length-6);
        System.out.print(". ");
        System.arraycopy(arrA,7,arrB,0,length-7);
        System.arraycopy(arrA,7,arrB,0,length-7);
        System.arraycopy(arrA,7,arrB,0,length-7);
        System.out.print(". ");
        System.arraycopy(arrA,2,arrB,0,length-2);
        System.arraycopy(arrA,2,arrB,0,length-2);
        System.arraycopy(arrA,2,arrB,0,length-2);
        System.out.print(". ");
        System.arraycopy(arrA,3,arrB,0,length-3);
        System.arraycopy(arrA,3,arrB,0,length-3);
        System.arraycopy(arrA,3,arrB,0,length-3);
        System.out.print(". ");
        System.arraycopy(arrA,4,arrB,0,length-4);
        System.arraycopy(arrA,4,arrB,0,length-4);
        System.arraycopy(arrA,4,arrB,0,length-4);
        System.out.print(". ");
        System.arraycopy(arrA,5,arrB,0,length-5);
        System.arraycopy(arrA,5,arrB,0,length-5);
        System.arraycopy(arrA,5,arrB,0,length-5);
        System.out.print(". ");
        System.arraycopy(arrA,6,arrB,0,length-6);
        System.arraycopy(arrA,6,arrB,0,length-6);
        System.arraycopy(arrA,6,arrB,0,length-6);
        System.out.println(". ");
        System.arraycopy(arrA,7,arrB,0,length-7);
        System.arraycopy(arrA,7,arrB,0,length-7);
        System.arraycopy(arrA,7,arrB,0,length-7);

        long end = System.currentTimeMillis();

        System.out.println("FINISHED");

        System.out.println("duration = "+(end - start)+" millis");

    }

    public static void printArr(int[] arr, int pos, int count, String prefix) {

        String out = prefix + " : ";
        for(int i=pos;i < pos+count; i++) {
            out = out + arr[i] + " ";
        }
        System.out.println(out);
    }
}

