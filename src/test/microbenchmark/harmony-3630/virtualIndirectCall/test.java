/**
 * Microbenchmark to measure performance for virtual calls
 * in case when a formal call receiver is of a parent type.
 */


class Parent {
    public void inc() {}
    public long getNum() {return 0;}
    public void reset() {}
}

class Child extends Parent {

    private long num;

    public Child() { num = 0; }
    public void inc() { num++; }
    public long getNum() { return num; }
    public void reset() { num = 0; }
}

public class test {
    
    static final long limit = 1000000000;

    static Parent obj = new Child();

    public static void main(String[] args) {
        test testObject = new test();

        long before = 0, after = 0;
	    long best = 0;

        for (int i = 0; i < 5; i++) {    
            obj.reset();

            before = System.currentTimeMillis();
            testObject.run();
            after = System.currentTimeMillis();
            
            long current = obj.getNum() / (((after - before)==0) ? 1 : (after - before));
            System.out.println("Current score: " + current);
            if (current > best) best = current;
        }
        System.out.println("Calls per millisecond: " + best);
    }

    public void run() {

        for (long k = 0; k < limit; k++ ) {
            obj.inc();
	    }
    }

}
