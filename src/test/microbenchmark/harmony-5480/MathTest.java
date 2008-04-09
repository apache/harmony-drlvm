package math;

/**
 * A microbenchmark on trigonometric/logarithmic math methods.
 * On DRLVM, specific optimization can be switched on/off from commandline: 
 * -XX:jit.arg.Math_as_magic=true/false.
 */

public class MathTest {
	static void f() {
		Math.abs(-123987.1236d);
		Math.asin(0.7);
		Math.acos(0.7);		
		Math.log(123.123);
		Math.log10(123.123);
		Math.log1p(123.123);		
		Math.sin(12312.123);
		Math.cos(12312.123);
		Math.sqrt(234234.234234);
		Math.tan(234234.12342134);
		Math.atan(2347.234);
		Math.atan2(231.123, 0);
		Math.abs(-123.1231123f);				
	}
    public static void main(String[] args) {    	
    	System.out.println("Warmup started....");
    	for (int i = 0; i < 2500 * 2500; i ++) {
		     f();
		}
    	for (int i = 0; i < 250 * 2500; i ++) {
		     f();
		}
    	System.out.println("Warmup ended....");
    	long start = System.currentTimeMillis();		
		for (int i = 0; i < 2500 * 2500; i ++) {
    		f();
		}
		System.out.println("floor result: " + (System.currentTimeMillis() - start));	
	}
}
