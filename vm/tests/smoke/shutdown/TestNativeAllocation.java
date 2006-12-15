package shutdown;

public class TestNativeAllocation {
    private static Object start = new Object();
    private static Object sync = new Object();

    public static void main(String[] args) {
        synchronized (start) {
            try {
                Thread worker = new WorkerThread();
                worker.setDaemon(true);
                worker.start();
                start.wait();
            } catch (InterruptedException e) {
                System.out.println("FAILED");
            }
        }
    }

    static class WorkerThread extends Thread {
        private int recursion = 0;
    
        static {
            System.loadLibrary("TestNativeAllocation");
        }


        public native void callJNI();
    
        public void calledFromJNI() {
            if (recursion < 30) {
                 ++recursion;
                 run();
            }
            synchronized (sync) {
                synchronized (start) {
                    start.notify();
                }
                try {
                    // wait here forever
                    sync.wait();
                } catch (Throwable e) {
                    System.out.println("FAILED");
                } finally {
                    System.out.println("FAILED");
                }
            }
            System.out.println("FAILED");
        }

        public void run() {
            callJNI();
            System.out.println("FAILED");
        }
    }
}
