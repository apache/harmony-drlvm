interface Intf {
    public void inc();
    public long getNum();
    public void reset();
}

class IntfImpl implements Intf {

    private long num;

    public IntfImpl() { num = 0; }
    public void inc() { num++; }
    public long getNum() { return num; }
    public void reset() { num = 0; }
}

public class test2012 {
    
    static final long limit = 10000000;

    static Intf obj = new IntfImpl();

    public static void main(String[] args) {
        test2012 testObject = new test2012();

        long before = 0, after = 0;
	long best = 0;

        for (int i = 0; i < 10; i++) {    
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
