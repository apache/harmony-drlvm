package shutdown;

public class TestFatalError {
    private static Object sync = new Object();

    static {
        System.loadLibrary("TestFatalError");
    }

    public static native void sendFatalError();

    public static void main(String[] args) {
        synchronized (sync) {
            Thread permanentThread = new PermanentThread();
            permanentThread.start();
            Thread hook = new HookThread();
            Runtime.getRuntime().addShutdownHook(hook);
            sendFatalError();
            System.out.println("FAILED");
        }
    }

    static class PermanentThread extends Thread {
        public void run() {
            synchronized (sync) {
                System.out.println("FAILED");
            }                
        }
        
    }

    static class HookThread extends Thread {
        public void run() {
           System.out.println("FAILED");
        }
    }
}