package org.apache.harmony.drlvm.tests.regression.h1863;

import java.io.*;
import junit.framework.TestCase;

public class H1863Test extends TestCase {

    public static int NumberArgs = 50;
    public final static String ARG_S = "arg";
    public final static String ARG_S1 = "0123456789";

    public void test1() {
        int len = 0;
        String [] args = new String [NumberArgs];
        for (int i = 0; i < NumberArgs; i++) {
            args[i] = ARG_S + Integer.toString(i) + "_";
            String s = "";
            for (int j = 0; j < i; j++) {
                s = s + ARG_S1;
            }
            s = s + "a";
            args[i] = args[i] + s;
            len += args[i].length();
        }

        for (int k = 0; k < 8; k++) {
            String c = "";
            String r = "";
            String fName = null;
            String fs = File.separator;
            String s = "org" + fs + "apache" + fs + "harmony" + fs + "drlvm" 
                    + fs + "tests" + fs + "regression" + fs + "h1863" + fs 
                    + "testExec2_App";
            try {
                fName = new File(ClassLoader.getSystemClassLoader().getResource(s + ".class").toURI()).toString();
            } catch (Exception e) {
                fail("Unexpected exception " + e);
            }
            String cp = fName.substring(0, fName.indexOf(s));
            c += System.getProperty("java.home")
                + File.separator + "bin" + File.separator + "java"
                + " -cp " + cp + " " + s.replace(File.separatorChar, '.');

            for (int i = 0; i < NumberArgs; i++) {
                for (int j = 0; j <= k; j++) {
                    c += " " + args[i];
                }
            }
            c += " ZAPREYEV";
            try {
                Process p = Runtime.getRuntime().exec(c);
                InputStream is = p.getInputStream(); 
                while (is.available() == 0) {
                    Thread.sleep(10);
                }
                p.waitFor();
                len = 0;
                while (is.available() != 0) { 
                    char ch = (char)is.read();
                    r += String.valueOf(ch);
                    len++;
                } 
                int ans = p.exitValue();
                for (int j = 0; j < 10; j++) {
                    Thread.sleep(100);
                    while (is.available()!=0) { 
                        char ch = (char)is.read();
                        r += String.valueOf(ch);
                        System.out.print(ch); len++;
                    } 
                } 
                if (ans != 77) {
                    System.out.println("TEST FAILED: Incorrect exitValue: "+ans);
                    return;
                }                        
                if (r.indexOf("ZAPREYEV") == -1) {
                    fail("Missing last argument");
                    return;
                }                        
            } catch (IOException e) {
                // could happen on Windows - it's OK when args list is too long 
                break;
            } catch (Exception e) {
                e.printStackTrace();
                fail("Unexpected exception: " + e);
            }
        }
    }
}
 
class testExec2_App {
    public static void main(String[] args) {
        System.out.println(args[args.length - 1]); 
        System.exit(77);
    }
}
