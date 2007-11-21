package ncai.funcs;

/**
 * @author Petr Ivanov
 * @version $Revision: 1.1.1.1 $
 *
 */
public class TerminateThread02 extends Thread{
    public native void TestFunction();
    public native void TestFunction1();
    public static native boolean stopsignal();
    public static native void resumeagent();

    static boolean NoLibrary = false;
    static {
        try{
            System.loadLibrary("TerminateThread02");
        }
        catch(Throwable e){
            NoLibrary = true;
        }
    }

    TerminateThread02(String name)
    {
        super(name);
    }

    static public void main(String args[]) {
        if(NoLibrary) return;
        for(int i = 0; i < 50; i++){
            new TerminateThread02("java_thread").start();
        }
        special_method();
        return;
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
        resumeagent();
        while(!stopsignal())
        {
            try {
                sleep(100, 0); // milliseconds
            } catch (java.lang.InterruptedException ie) {}
        }
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
        test_java_func3();
    }
}


