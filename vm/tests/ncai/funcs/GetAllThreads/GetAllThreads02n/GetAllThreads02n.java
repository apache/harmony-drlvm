package ncai.funcs;

/**
 * @author Petr Ivanov
 * @version $Revision: 1.1.1.1 $
 *
 */
public class GetAllThreads02n extends Thread{
    public native void TestFunction();
    public native void TestFunction1();
    public static native boolean stopsignal();
    public static native void resumeagent();

    static boolean NoLibrary = false;
    static {
        try{
            System.loadLibrary("GetAllThreads02n");
        }
        catch(Throwable e){
            NoLibrary = true;
        }
    }

    GetAllThreads02n(String name)
    {
        super(name);
    }

    static public void main(String args[]) {
        if(NoLibrary) return;
        new GetAllThreads02n("java_thread1").start();
        new GetAllThreads02n("java_thread2").start();
        new GetAllThreads02n("java_thread3").start();
        special_method();
        return;
    }

    public void test_java_func1(){
        System.out.println("thread - java func1\n");
        TestFunction1();
    }

    public void test_java_func2(){
        System.out.println("thread - java func2\n");
        test_java_func3();
    }

    public void test_java_func3(){
        System.out.println("thread - java func3\n");
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


