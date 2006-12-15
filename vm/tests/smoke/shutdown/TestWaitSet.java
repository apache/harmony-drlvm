package shutdown;

public class TestWaitSet {
    private static Object sync = new Object();

    public static void main(String[] args) {
        synchronized (sync) {
            try {
                Thread worker = new WorkerThread();
                worker.setDaemon(true);
                worker.start();
                sync.wait();
            } catch (InterruptedException e) {
                System.out.println("FAILED");
            }
        }
        System.out.println("PASSED");
    }

    static class WorkerThread extends Thread {
        public void run() {
            synchronized (sync) {
                try {
                    sync.notify();
                    sync.wait();
                } catch (InterruptedException e) {
                    System.out.println("FAILED");
                }
                System.out.println("FAILED");
            }
            System.out.println("FAILED");
        }
    }
}