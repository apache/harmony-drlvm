package shutdown;

/**
 * Tests that VM doen't hang on shutdown if a daemon thread is on Object.wait()
 */
public class TestDaemonOnWait {
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

            System.out.println("PASSED");
        }
    }

    static class WorkerThread extends Thread {
        private int recursion = 0;

        static {
            System.loadLibrary("TestDaemonOnWait");
        }


        public native void callJNI();

        public void calledFromJNI() throws InterruptedException {
            if (recursion < 30) {
                 ++recursion;
                 run();
            }

            // when desired stack frame count is achieved
            synchronized (sync) {
                synchronized (start) {
                    // release main thread in order to initiate VM shutdown
                    start.notify();
                }

                // wait here forever.
                // actually this whait() will be interrupted by VM shutdown
                // process with the exception.
                sync.wait();
            }
        }

        public void run() {
            // recursively calls JNI method which calls java method in order
            // to create a number of M2n and java frames on stack.
            callJNI();
        }
    }
}
