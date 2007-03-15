/**
 * Microbenchmark on scalar replacement optimization (virtual method test).
 * To see the effect, tests need to be run with the following switches:
 * -Xem:server -XX:jit.SD2_OPT.arg.optimizer.escape=on
 * -Xem:server -XX:jit.SD2_OPT.arg.optimizer.escape=off
 */

class Cls1 { 
    private long num;
    public Cls1() { num = 0; }
    public void inc(Integer i) { num= i+1; }
    public long getNum() { return num; }
    public void reset() { num = 0; }
}

public class test1 {
    static final long limit = 100000000;
    static Cls1 obj = new Cls1();

    public static void main(String[] args) {
        long before = 0, after = 0;
        for (int i = 0; i < 5; i++) {    
            obj.reset();
            before = System.currentTimeMillis();
            for (long k = 0; k < limit; k++ ) {
                dovc(k);
            }
            after = System.currentTimeMillis();
            System.out.println("Calls per millisecond: " + (obj.getNum() / (after - before)));
        }
    }  
    static void dovc(long i) {
        Integer i1 = new Integer((int)i);
        obj.inc(i1);
    }
}
