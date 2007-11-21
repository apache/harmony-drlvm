package ncai.funcs;

/**
 * @author Petr Ivanov
 * @version $Revision: 1.1.1.1 $
 *
 */
public class TerminateThread02n extends Thread{
    public native void TestFunction();
    public native void TestFunction1();
    public static native boolean stopsignal();
    public static native void resumeagent();

    static boolean NoLibrary = false;
    static {
        try{
            System.loadLibrary("TerminateThread02n");
        }
        catch(Throwable e){
            NoLibrary = true;
        }
    }

    TerminateThread02n(String name)
    {
        super(name);
    }

    static public void main(String args[]) {
        if(NoLibrary) return;
        new TerminateThread02n("java_thread").start();
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


