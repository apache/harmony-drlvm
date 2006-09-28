import java.util.Vector;

class Fin {
    public void finalize() {}
}

public class Test {
    public static void main(String[] args) {
        try {
            Vector v = new Vector();
            while (true) {
                int fin = 0, obj = 0;
                Object[] array = new Object[4096];
                for(int i = 0; i < 512; i++) {
                    array[i * 8] = new Object();
                    for(int j = 1; j < 8; j++) {
                        array[i * 8 + j] = new Fin();
                    }
                }
                for(int i = 0; i < 4096; i++) {
                    if (array[i] instanceof Fin) {
                        fin++;
                    } else {
                        obj++;
                    }
                }
                System.err.print(".");
                v.add(array);
            }
        } catch (OutOfMemoryError e) {
            System.out.println("PASSED");
        }
    }
}
