package shutdown;

public class TestLock {
    private static Object start = new Object();
    private static Object sync = new Object();

    public static void main(String[] args) {
        synchronized (start) {
            try {
                Thread worker = new WorkerThread();
                worker.setDaemon(true);
                worker.start();
                start.wait();
                Thread locked = new LockedThread();
                locked.setDaemon(true);
                locked.start();
                start.wait();
            } catch (InterruptedException e) {
                System.out.println("FAILED");
            }
        }
        System.out.println("PASSED");
    }

    static class WorkerThread extends Thread {
        public void run() {
            synchronized (sync) {
                synchronized (start) {
                    start.notify();
                }
                while (true) {
                    Thread.yield();
                }
            }
        }
    }

    static class LockedThread extends Thread {
        public void run() {
            synchronized (start) {
                start.notify();
            }
            synchronized (sync) {
                System.out.println("FAILED");
            }
            System.out.println("FAILED");
        }
    }

}