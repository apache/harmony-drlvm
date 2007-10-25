/**
 * Microbenchmark on scalar replacement optimization (interface method test).
 * To see the effect, tests need to be run with the following switches:
 * -Xem:server -XX:jit.SD2_OPT.arg.optimizer.escape=on
 * -Xem:server -XX:jit.SD2_OPT.arg.optimizer.escape=off
 */

interface Intf2 {
    public void inc(Integer i) ;
    public long getNum();
    public void reset();
}

class Cls2 implements Intf2 {
    private long num;
    public Cls2() { num = 0; }
    public void inc(Integer i) { num= i+1; }
    public long getNum() { return num; }
    public void reset() { num = 0; }
}

public class test2 {  
    static final long limit = 100000000;
    static Intf2 obj = new Cls2();

    public static void main(String[] args) {
        long before = 0, after = 0;
        for (int i = 0; i < 5; i++) {    
            obj.reset();
            before = System.currentTimeMillis();
            for (long k = 0; k < limit; k++ ) {
                doic(k);
            }
            after = System.currentTimeMillis();
            System.out.println("Calls per millisecond: " + (obj.getNum() / (after - before)));
        }
    }
    static void doic(long i) {
        Integer i1 = new Integer((int)i);
        obj.inc(i1);
    }
}
