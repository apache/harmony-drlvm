public class NegativeArray {

	    public static void main(String[] args) {
	        try {
		   int i[][] = new int [10][-10];
	           System.out.println("Test failed");
	        } catch (NegativeArraySizeException ex) {
	            System.out.println("Test passed: " + ex);
	        }
	     }
}
