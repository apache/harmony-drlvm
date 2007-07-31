package org.apache.harmony.drlvm.tests.regression.h4514;
import junit.framework.*;
public class Test extends TestCase {
    
    public void testTrace1() {
        //some useless code
        int i=0;
        int j=1;
        try {
            assertEquals(0, 1); //the number of this line must be in tracktracelement
        } catch (Throwable e) {
            StackTraceElement thisFrame = findThisFrame(e);
            assertEquals(10, thisFrame.getLineNumber());
        }
    }

    public void testTrace2() {
        //some useless code
        int i=0;
        int j=1;
        try {
            fail();//the number of this line must be in tracktracelement
        } catch (Throwable e) {
            StackTraceElement thisFrame = findThisFrame(e);
            assertEquals(22, thisFrame.getLineNumber());
        }
    }

    public void testTrace3() {
        //some useless code
        int i=0;
        int j=1;
        try {
            assertEquals(true, false);//the number of this line must be in tracktracelement
        } catch (Throwable e) {
            StackTraceElement thisFrame = findThisFrame(e);
            assertEquals(34, thisFrame.getLineNumber());
        }

    }

    public void testTrace4() {
        //some useless code
        int i=0;
        int j=1;
        try {
            assertNotNull(null);//the number of this line must be in tracktracelement
        } catch (Throwable e) {
            StackTraceElement thisFrame = findThisFrame(e);
            assertEquals(47, thisFrame.getLineNumber());
        }

    }

    public void testTrace5() {
        //some useless code
        int i=0;
        int j=1;
        try {
            assertEquals("", "fail");//the number of this line must be in tracktracelement
        } catch (Throwable e) {
            StackTraceElement thisFrame = findThisFrame(e);
            assertEquals(60, thisFrame.getLineNumber());
        }

    }

    static StackTraceElement findThisFrame(Throwable e) {
        StackTraceElement[] frames =  e.getStackTrace();
        for (StackTraceElement frame : frames) {
            if (frame.getClassName().equals(Test.class.getName())) {
                return frame;
            }
        }
        return null;
    }
}
