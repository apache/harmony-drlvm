package shutdown;

public class TestInterrupt {
    private static Object sync = new Object();

    static {
        System.loadLibrary("TestInterrupt");
    }

    public static native void sendInterrupt();

    public static void main(String[] args) {
        synchronized (sync) {
            try {
                Thread hook = new HookThread();
                Runtime.getRuntime().addShutdownHook(hook);
                sendInterrupt();
                sync.wait();                 
                sync.wait();                 
            } catch (InterruptedException e) {
                System.out.println("FAILED");
            }
        }
        System.out.println("FAILED");
    }

    static class HookThread extends Thread {
        public void run() {
            synchronized (sync) {
                sync.notify();
            }
            System.out.println("PASSED");
        }
    }
}