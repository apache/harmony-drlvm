package org.apache.harmony.drlvm.tests.regression.h4514;
import junit.framework.*;
public class Test extends TestCase {
    
    public void testTrace1() {
        try {
            assertEquals(0, 1); //the number of this line must be in tracktracelement
        } catch (Throwable e) {
            StackTraceElement thisFrame = findThisFrame(e);
            assertEquals(7, thisFrame.getLineNumber());
        }
    }

    public void testTrace2() {
        try {
            fail();//the number of this line must be in tracktracelement
        } catch (Throwable e) {
            StackTraceElement thisFrame = findThisFrame(e);
            assertEquals(16, thisFrame.getLineNumber());
        }
    }

    public void testTrace3() {
        try {
            assertEquals(true, false);//the number of this line must be in tracktracelement
        } catch (Throwable e) {
            StackTraceElement thisFrame = findThisFrame(e);
            assertEquals(25, thisFrame.getLineNumber());
        }

    }

    public void testTrace4() {
        try {
            assertNotNull(null);//the number of this line must be in tracktracelement
        } catch (Throwable e) {
            StackTraceElement thisFrame = findThisFrame(e);
            assertEquals(35, thisFrame.getLineNumber());
        }

    }

    public void testTrace5() {
        try {
            assertEquals("", "fail");//the number of this line must be in tracktracelement
        } catch (Throwable e) {
            StackTraceElement thisFrame = findThisFrame(e);
            assertEquals(45, thisFrame.getLineNumber());
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
