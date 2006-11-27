package org.apache.harmony.drlvm.tests.regression.h1800;

import junit.framework.TestCase;
import java.io.*;

public class ExecTest extends TestCase {
    
    public void testExec() throws Exception {
        String [] cmdL = new String[5];
        cmdL[0] = System.getProperty("java.home")+File.separator+"bin"+File.separator+"java";
        cmdL[1] = "-classpath";
        cmdL[2] = ".";
        cmdL[3] = "testExec1_App";
        cmdL[4] = null;
        try {
            Process p = Runtime.getRuntime().exec(cmdL);
            p.waitFor();
            int ans = p.exitValue();
            InputStream is = p.getErrorStream();
            int toRead = is.available();
            byte[] out = new byte[100];
            int sz = 0;
            while (true) {
                int r = is.read();
                if (r == -1) {
                    break;
                }
                out[sz] = (byte)r;
                sz++;
                if (sz == 100) {
                    break;
                }
            }
            System.out.println("========Application error message======");
            System.out.println(new String (out, 0, sz));
            System.out.println("=======================================");

            fail("NullPointerException was not thrown. exitValue = " + ans);
        } catch (NullPointerException e) {
        }
    }
}

