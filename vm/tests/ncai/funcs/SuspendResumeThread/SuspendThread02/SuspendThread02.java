package ncai.funcs;

/**
 * @author Petr Ivanov
 * @version $Revision: 1.1.1.1 $
 *
 */
public class SuspendThread02 extends Thread{
    public native void TestFunction();
    public native void TestFunction1();
    public static native boolean stopsignal();
    public static native void resumeagent();
    public static native void increment();

    static boolean NoLibrary = false;
    static {
        try{
            System.loadLibrary("SuspendThread02");
        }
        catch(Throwable e){
            NoLibrary = true;
        }
    }

    SuspendThread02(String name)
    {
        super(name);
    }

    static public void main(String args[]) {
        if(NoLibrary) return;
        new SuspendThread02("java_thread").start();
        special_method();
    }

    public void test_java_func1(){
        System.out.println("thread - java func1\n");
        TestFunction();
    }

    public void test_java_func2(){
        System.out.println("thread - java func2\n");
        test_java_func3();
    }

    public void test_java_func3(){
        System.out.println("thread - java func3\n");
        TestFunction1();
        System.out.println("thread - java func3\n");
        resumeagent();
        while(!stopsignal())
        {
            try {
                sleep(100, 0); // milliseconds
            } catch (java.lang.InterruptedException ie) {}
        }
        increment();
        resumeagent();
    }

    static public void special_method() {
        /*
         * Transfer control to native part.
         */
        try {
            throw new InterruptedException();
        } catch (Throwable tex) { }
        return;
    }

    public void run() {
        System.out.println("thread - java run\n");
        test_java_func1();
    }
}


