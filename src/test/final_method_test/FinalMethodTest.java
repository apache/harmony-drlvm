import finalmethodtest.A;


class B extends A {
	    private void m() {
                   System.out.println("PASSED");
		        }
	        public void test() {
			        m();
				    }
}

public class FinalMethodTest {
	    public static void main(String[] args) {
		            new B().test();
			        }

} 
