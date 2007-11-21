package ncai.funcs;

/**
 * @author Petr Ivanov
 * @version $Revision: 1.1.1.1 $
 *
 */
public class GetStackTrace03 extends Thread{
    public native void TestFunction();
    public native void TestFunction1();
    public static native boolean stopsignal();

    static boolean NoLibrary = false;
    static {
        try{
            System.loadLibrary("GetStackTrace03");
        }
        catch(Throwable e){
            NoLibrary = true;
        }
    }

    GetStackTrace03(String name)
    {
        super(name);
    }

    static public void main(String args[]) {
        if(NoLibrary) return;
        new GetStackTrace03("java_thread").start();
        special_method();
        return;
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
        TestFunction();
    }
}


