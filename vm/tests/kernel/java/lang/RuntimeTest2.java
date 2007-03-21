/*
 *  Licensed to the Apache Software Foundation (ASF) under one or more
 *  contributor license agreements.  See the NOTICE file distributed with
 *  this work for additional information regarding copyright ownership.
 *  The ASF licenses this file to You under the Apache License, Version 2.0
 *  (the "License"); you may not use this file except in compliance with
 *  the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

/**
 * @author Serguei S.Zapreyev
 * @version $Revision$
 */

package java.lang;

import java.io.File;
import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStreamReader;

import junit.framework.TestCase;

/*
 * Created on January 5, 2005
 *
 * This RuntimeTest class is used to test the Core API Runtime class
 * 
 */


/**
 * ###############################################################################
 * ###############################################################################
 * REMINDER("XXX") LIST:
 * 1. [Jun 11, 2005] test_availableProcessors, test_freeMemory, test_gc, test_runFinalization, 
 *    test_runFinalizersOnExit fail on "ORP+our Runtime+CLASSPATH API" platform
 *    because the availableProcessors, freeMemory, runFinalization (runFinalizersOnExit?)
 *    methods aren't correctly supported yet in orp/drl_natives/src
 * 2. [Jun 11, 2005] test_maxMemory, test_totalMemory fail on "ORP+CLASSPATH API" platform
 *    because the maxMemory
 *    method isn't correctly supported yet in orp/drl_natives/src:
 *    (Exception: java.lang.UnsatisfiedLinkError: Error compiling method java/lang/Runtime.maxMemory()J)
 * 3. [Jun 11, 2005] test_availableProcessors fails on "ORP+CLASSPATH API" platform
 *    because the availableProcessors
 *    method isn't correctly supported yet in orp/drl_natives/src:
 *    (Exception: java.lang.UnsatisfiedLinkError: Error compiling method java/lang/Runtime.availableProcessors()I)
 * ###############################################################################
 * ###############################################################################
 **/

public class RuntimeTest2 extends TestCase {

    protected void setUp() throws Exception {
    }

    protected void tearDown() throws Exception {
    }    

    
    /**
     *  
     */
    static class forInternalUseOnly {
        String stmp;

        forInternalUseOnly () {
            this.stmp = "";
            for (int ind2 = 0; ind2 < 100; ind2++) {
                this.stmp += "0123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789"+
                "0123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789"+
                "0123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789"+
                "0123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789"+
                "0123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789"+
                "0123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789"+
                "0123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789"+
                "0123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789"+
                "0123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789"+
                "0123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789"+
                "0123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789";
            }
        }
        protected void finalize() throws Throwable {
            runFinalizationFlag = true;
            super.finalize();
        }
    }

    static boolean runFinalizationFlag = false;
    public void test_runFinalization() {
        //System.out.println("test_runFinalization");
        runFinalizationFlag = false;
        for (int ind2 = 0; ind2 < 10; ind2++) {
            forInternalUseOnly ins = new forInternalUseOnly();
            ins.stmp += "";
            ins = null;
            // Runtime.getRuntime().runFinalization();
            try {
                Thread.sleep(10);
            } catch (Exception e) {
                fail("check001: Unexpected exception: " + e);
            }
            Runtime.getRuntime().gc();
            try {
                Thread.sleep(10);
            } catch (Exception e) {
                fail("check002: Unexpected exception: " + e);
            }
            Runtime.getRuntime().runFinalization();
        }
        assertTrue("finalization has not been run", runFinalizationFlag);
    }
    
    /**
     *  
     */
    public void test_runFinalizersOnExit() {
        /**/System.out.println("test_runFinalizersOnExit");
        runFinalizationFlag = false;
        for (int ind2 = 0; ind2 < 5; ind2++) {
            Runtime.runFinalizersOnExit(false);
            forInternalUseOnly ins = new forInternalUseOnly();
            ins.stmp += "";
            ins = null;
            // / Runtime.getRuntime().runFinalization();
            try {
                Thread.sleep(10);
            } catch (Exception e) {
                fail("check001: Unexpected exception: " + e);
            }
            Runtime.getRuntime().gc();
            try {
                Thread.sleep(10);
            } catch (Exception e) {
                fail("check002: Unexpected exception: " + e);
            }
            // / Runtime.getRuntime().runFinalization();
        }
        assertTrue("check003: finalizers were not run", runFinalizationFlag);

        runFinalizationFlag = false;
        for (int ind2 = 0; ind2 < 5; ind2++) {
            Runtime.runFinalizersOnExit(true);
            forInternalUseOnly ins = new forInternalUseOnly();
            ins.stmp += "";
            ins = null;
            // / Runtime.getRuntime().runFinalization();
            try {
                Thread.sleep(10);
            } catch (Exception e) {
                fail("check004: Unexpected exception: " + e);
            }
            Runtime.getRuntime().gc();
            try {
                Thread.sleep(10);
            } catch (Exception e) {
                fail("check005: Unexpected exception: " + e);
            }
            // / Runtime.getRuntime().runFinalization();
        }
        assertTrue("check006: finalizers were not run", runFinalizationFlag);
    }
    
    /**
     *  
     */
    class threadForInternalUseOnly1 extends Thread {
        public void run() {
            // System.out.println("START threadForInternalUseOnly1");
            int I = threadForInternalUseOnly2.getI();
            int counter = 0;
            while ((I < 50 || number < I) && counter < 24000) {
                try {
                    Thread.sleep(50);
                } catch (Exception e) {
                }
                I = threadForInternalUseOnly2.getI();
                counter += 1;
            }
            // System.out.println("FINISH threadForInternalUseOnly1");
        }

        protected void finalize() throws Throwable {
            // System.out.println(">>>>>>>>>>>>>>START
            // threadForInternalUseOnly1.finalize");
            if (runFinalizationFlag2 == 1 || runFinalizationFlag2 == 11
                    || runFinalizationFlag2 == 21) {
                // :) // assertTrue( "FAILED: addShutdownHook.check001", false);
            }
            super.finalize();
            //System.out.println(">>>>>>>>>>>>>>FINISH threadForInternalUseOnly1.finalize");
        }
    }

    static class threadForInternalUseOnly2 extends Thread {
        static int I = 0;
        long NM;
        int ORD;

        synchronized static void incrI() {
            I++;
        };

        synchronized static int getI() {
            return I;
        };

        threadForInternalUseOnly2(int ind) {
            super();
            NM = System.currentTimeMillis();
            ORD = ind;
        }

        public void run() {
            // System.out.println("START threadForInternalUseOnly2: "+ORD);
            if (ORD == 1 || ORD == 11 || ORD == 21) {
                synchronized (threadForInternalUseOnly2.class) {
                    runFinalizationFlag2 = ORD;
                }
            }
            incrI();
            for (int j = 0; j < 30/* 100 */; j++) {
                try {
                    Thread.sleep(10);
                } catch (Exception e) {
                }
            }
            // System.out.println("FINISH threadForInternalUseOnly2: "+ORD);
        }

        protected void finalize() throws Throwable {
            // System.out.println("<<<<<<<<<<<<<<<<<START
            // threadForInternalUseOnly2.finalize");
            if (runFinalizationFlag2 == 1 || runFinalizationFlag2 == 11
                    || runFinalizationFlag2 == 21) {
                // :) // assertTrue( "FAILED: addShutdownHook.check002", false);
            }
            super.finalize();
            // System.out.println("<<<<<<<<<<<<<<<<<FINISH
            // threadForInternalUseOnly2.finalize");
        }
    }

    static class threadForInternalUseOnly3 extends Thread {
        static int I = 0;

        synchronized static void incrI() {
            I++;
        };

        synchronized static int getI() {
            return I;
        };

        public void run() {
            // System.out.println("Run threadForInternalUseOnly3: "+getI());
            incrI();
        }
    }
    
    static int runFinalizationFlag2 = -1;
    static int number = 4; //100;
    static int nthr   = 2; //21;

    public void test_addShutdownHook() {
        /**/System.out.println("test_addShutdownHook");
        Thread[] thr = new Thread[number];
        for (int i = 0; i < number / 2; i++) {
            Runtime
                    .getRuntime()
                    .addShutdownHook(
                            thr[2 * i + 0] = new threadForInternalUseOnly3());// null);//
            try {
                Thread.sleep(5);
            } catch (Exception e) {
            }
            Runtime.getRuntime().addShutdownHook(
                    thr[2 * i + 1] = new threadForInternalUseOnly2(2 * i + 1));
            try {
                Thread.sleep(5);
            } catch (Exception e) {
            }
        }
        // System.out.println("2test_addShutdownHook");
        Runtime.runFinalizersOnExit(true);
        new threadForInternalUseOnly1().start();
        try {
            Runtime.getRuntime().addShutdownHook(thr[nthr]);
            fail("IllegalArgumentException has not been thrown");
        } catch (IllegalArgumentException e) {
        }
        //System.out.println("3test_addShutdownHook");
    }
    
    /**
     *  
     */
    public void test_removeShutdownHook() {
        /**/System.out.println("test_removeShutdownHook");
        Thread[] thr = new Thread[number];
        for (int i = 0; i < number / 2; i++) {
            Runtime.getRuntime().addShutdownHook(
                    thr[2 * i + 0] = new threadForInternalUseOnly3());
            try {
                Thread.sleep(5);
            } catch (Exception e) {
            }
            Runtime.getRuntime().addShutdownHook(
                    thr[2 * i + 1] = new threadForInternalUseOnly2(2 * i + 1));
            try {
                Thread.sleep(5);
            } catch (Exception e) {
            }
        }
        // Runtime.getRuntime().removeShutdownHook(thr[1]);
        // Runtime.getRuntime().removeShutdownHook(thr[11]);
        Runtime.getRuntime().removeShutdownHook(thr[nthr]);
        new threadForInternalUseOnly1().start();
        try {
            // Runtime.getRuntime().addShutdownHook(thr[1]);
            // Runtime.getRuntime().addShutdownHook(thr[11]);
            Runtime.getRuntime().addShutdownHook(thr[nthr]);
            // Runtime.getRuntime().removeShutdownHook(thr[1]);
            // Runtime.getRuntime().removeShutdownHook(thr[11]);
            Runtime.getRuntime().removeShutdownHook(thr[nthr]);
            // Runtime.getRuntime().removeShutdownHook(thr[1]);
            // Runtime.getRuntime().removeShutdownHook(thr[11]);
            Runtime.getRuntime().removeShutdownHook(thr[nthr]);
        } catch (Exception e) {
            fail("Unexpected Exception: " + e);
        }
    }
    
    /**
     *  
     */
    public void test_exec_Str() {
        /**/System.out.println("test_exec_Str");
        if (System.getProperty("os.name").toLowerCase().indexOf("windows") != -1) {
            try {
                String pathList = System.getProperty("java.library.path");
                String[] paths = pathList.split(File.pathSeparator);
                String cmnd = null;
                int ind1;
                for (ind1 = 0; ind1 < paths.length; ind1++) {
                    if (paths[ind1] == null) {
                        continue;
                    }
                    File asf = new java.io.File(paths[ind1] + File.separator
                            + "cmd.exe");
                    if (asf.exists()) {
                        cmnd = paths[ind1] + File.separator + "cmd.exe";
                        break;
                    }
                }
                if (cmnd == null) {
                    if (new java.io.File(
                            // XXX:IA64?
                            (cmnd = "C:\\WINNT\\system32\\cmd.exe")).exists()) {
                    } else if (new java.io.File(
                            // XXX:IA64?
                            (cmnd = "C:\\WINDOWS\\system32\\cmd.exe")).exists()) {
                    } else {
                        fail("cmd.exe hasn't been found! Please, set the path" +
                                " to cmd.exe via java.library.path property.");
                    }
                }
                cmnd = cmnd + " /C date";
                Process pi3 = Runtime.getRuntime().exec(cmnd);
                java.io.OutputStream os = pi3.getOutputStream();
                pi3.getErrorStream();
                java.io.InputStream is = pi3.getInputStream();
                // wait for is.available != 0
                int count = 100;
                while (is.available() < 1 && count-- > 0) {
                    try {
                        Thread.sleep(200);
                    } catch (Exception e) {
                    }
                }
                if (count < 0) {
                    fail("check001: the date's reply has not been received");
                }

                int ia = is.available();
                byte[] bb = new byte[ia];
                is.read(bb);
                // System.out.println("3test_exec_Str");
                String r1 = new String(bb);
                if (r1.indexOf("The current date is") == -1
                        || r1.indexOf("Enter the new date") == -1) {
                    fail("exec(String[], String[], File).check002: where is " +
                            "the date's answer/request?");
                }
                // System.out.println("4test_exec_Str");
                for (int ii = 0; ii < ia; ii++) {
                    bb[ii] = (byte) 0;
                }

                os.write('x');
                os.write('x');
                os.write('-');
                os.write('x');
                os.write('x');
                os.write('-');
                os.write('x');
                os.write('x');
                os.write('\n');
                os.flush();

                // wait for is.available > 9 which means that 'is' contains
                // both the above written value and the consequent 
                // 'date' command's reply
                count = 300;
                while (is.available() < 10 && count-- > 0) {
                    try {
                        Thread.sleep(200);
                    } catch (Exception e) {
                    }
                }
                if (count < 0) {
                    fail("check003: the date's reply has not been received");
                }
                ia = is.available();
                byte[] bbb = new byte[ia];
                is.read(bbb);
                r1 = new String(bbb);
                if (r1.indexOf("The system cannot accept the date entered") == -1
                        && r1.indexOf("Enter the new date") == -1) {
                    fail("check004: unexpected output: " + r1);
                }
                os.write('\n');
                try {
                    pi3.exitValue();
                } catch (IllegalThreadStateException e) {
                    os.flush();
                    try {
                        pi3.waitFor();
                    } catch (InterruptedException ee) {
                    }
                }
                // System.out.println("5test_exec_Str");
                // os.write('\n');
                // os.write('\n');
                // os.flush();
                pi3.destroy();
                // pi3.waitFor();
            } catch (java.io.IOException eeee) {
                eeee.printStackTrace();
                System.out.println("test_exec_Str test hasn't finished correctly because of the competent IOException.");
                return;
            } catch (Exception eeee) {
                eeee.printStackTrace();
                fail("check005: UnexpectedException on " +
                        "exec(String[], String[], File)");
            }
        } else if (System.getProperty("os.name").toLowerCase()
                .indexOf("linux") != -1) {
            // TODO
        } else {
            //UNKNOWN
        }
    }
    
    /**
     *  
     */
    public void test_exec_StrArr() {
        /**/System.out.println("test_exec_StrArr");
        String[] command = null;
        if (System.getProperty("os.name").toLowerCase()
                .indexOf("windows") != -1) {
            command = new String[]{"cmd", "/C", "echo S_O_M_E_T_H_I_N_G"};
        } else {
            command = new String[]{"/bin/sh", "-c", "echo S_O_M_E_T_H_I_N_G"};
        }
        String procValue = null;
        try {
            Process proc = Runtime.getRuntime().exec(command);
            BufferedReader br = new BufferedReader(new InputStreamReader(
                proc.getInputStream()));
            procValue = br.readLine();
        } catch (IOException e) {
            fail("Unexpected IOException");
        }
        assertTrue("echo command has not been run",
                procValue.indexOf("S_O_M_E_T_H_I_N_G") != -1);
    }
    
    /**
     *  
     */
    public void test_exec_StrArr_StrArr() {
        /**/System.out.println("test_exec_StrArr");
        String[] command = null;
        if (System.getProperty("os.name").toLowerCase()
                .indexOf("windows") != -1) {
            command = new String[] {"cmd", "/C", "echo %Z_S_S%"};
        } else {
            command = new String[] {"/bin/sh", "-c", "echo $Z_S_S"};
        }
        String procValue = null;
        try {
            Process proc = Runtime.getRuntime().exec(command,
                    new String[] {"Z_S_S=S_O_M_E_T_H_I_N_G"});
            BufferedReader br = new BufferedReader(new InputStreamReader(proc
                    .getInputStream()));
            procValue = br.readLine();
        } catch (IOException e) {
            fail("Unexpected IOException");
        }
        assertTrue("echo command has not been run",
                procValue.indexOf("S_O_M_E_T_H_I_N_G") != -1);
    }
    
    /**
     * 
     */
    public void test_exec_StrArr_StrArr_Fil() {
        /**/System.out.println("test_exec_StrArr");
        String[] command = null;
        if (System.getProperty("os.name").toLowerCase()
                .indexOf("windows") != -1) {
            command = new String[] {"cmd", "/C", "env"};
        } else {
            command = new String[] {"/bin/sh", "-c", "env"};
        }
        String as[];
        int len = 0;
        try {
            Process proc = Runtime.getRuntime().exec(command);
            BufferedReader br = new BufferedReader(new InputStreamReader(proc
                    .getInputStream()));
            while (br.readLine() != null) {
                len++;
            }

        } catch (IOException e) {
            fail("check001: Unexpected IOException");
        }
        as = new String[len];
        try {
            Process proc = Runtime.getRuntime().exec(command);
            BufferedReader br = new BufferedReader(new InputStreamReader(proc
                    .getInputStream()));
            for (int i = 0; i < len; i++) {
                as[i] = br.readLine();
            }

        } catch (IOException e) {
            fail("check002: Unexpected IOException");
        }
/**/
                if (System.getProperty("os.name").toLowerCase().indexOf("windows") != -1) {
                    as = new String[]{"to_avoid=#s1s2f1t1"}; // <<<<<<<<<<< !!! to remember
                    command = new String[]{"cmd", "/C", "dir"};
                } else {
                    command = new String[]{"sh", "-c", "pwd"};
                }
                try {
                    Process proc = Runtime.getRuntime().exec(command, as, new File(System.getProperty("java.io.tmpdir")));
                    BufferedReader br = new BufferedReader(new InputStreamReader(
                        proc.getInputStream()));
                    //for (int i = 0; i < len; i++) {
                    String ln;
                    while ( (ln = br.readLine()) != null) {
                        if(ln.indexOf(System.getProperty("java.io.tmpdir").substring(0,System.getProperty("java.io.tmpdir").length() -1 ))!=-1) {
                            return;
                        }
                   }
                    fail("Error3");
                } catch (IOException e) {
                    e.printStackTrace();
                    fail("Error4");
                }
                fail("Error5");
/**/
    }
    
    /**
     *  
     */
    public void test_exec_Str_StrArr() {
        /**/System.out.println("test_exec_StrArr");
        String command = null;
        if (System.getProperty("os.name").toLowerCase()
                .indexOf("windows") != -1) {
            command = "cmd /C \"echo %Z_S_S_2%\"";
        } else {
            //command = "/bin/sh -c \"echo $Z_S_S_2\"";
            //command = "/bin/echo $Z_S_S_2";
            command = "/usr/bin/env";
        }
        String procValue = null;
        try {
            Process proc = Runtime.getRuntime().exec(command,
                    new String[] {"Z_S_S_2=S_O_M_E_T_H_I_N_G"});
            BufferedReader br = new BufferedReader(new InputStreamReader(proc
                    .getInputStream()));
            while ((procValue = br.readLine()) != null) {
                if (procValue.indexOf("S_O_M_E_T_H_I_N_G") != -1) {
                    return;
                }
                fail("It should be the only singl environment variable here (" + procValue + ")");
            }
            fail("Z_S_S_2 var should be present and assingned correctly.");
        } catch (IOException e) {
            fail("Unexpected IOException");
        }
        /**/
        // Commented because the drlvm issue
                       try {
                           Process proc = Runtime.getRuntime().exec(command, new String[] {
                                   "Z_S_S_2=S_O_M_E_T_H_I_N_G_s1s2f1t1", //<<<<<<<<<<< !!! to remember
                                   "Z_S_S_3=S_O_M_E_T_H_I_N_G L_O_N_GGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGG",
                                   "Z_S_S_3=S_O_M_E_T_H_I_N_G L_O_N_GGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGG",
                                   "Z_S_S_3=S_O_M_E_T_H_I_N_G L_O_N_GGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGG",
                                   "Z_S_S_3=S_O_M_E_T_H_I_N_G L_O_N_GGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGG",
                                   "Z_S_S_3=S_O_M_E_T_H_I_N_G L_O_N_GGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGG",
                                   "Z_S_S_3=S_O_M_E_T_H_I_N_G L_O_N_GGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGG",
                                   "Z_S_S_3=S_O_M_E_T_H_I_N_G L_O_N_GGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGG",
                                   "Z_S_S_3=S_O_M_E_T_H_I_N_G L_O_N_GGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGG",
                                   "Z_S_S_3=S_O_M_E_T_H_I_N_G L_O_N_GGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGG",
                                   "Z_S_S_3=S_O_M_E_T_H_I_N_G L_O_N_GGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGG",
                                   "Z_S_S_3=S_O_M_E_T_H_I_N_G L_O_N_GGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGG",
                                   "Z_S_S_3=S_O_M_E_T_H_I_N_G L_O_N_GGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGG",
                                   "Z_S_S_3=S_O_M_E_T_H_I_N_G L_O_N_GGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGG",
                                   "Z_S_S_3=S_O_M_E_T_H_I_N_G L_O_N_GGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGG",
                                   "Z_S_S_3=S_O_M_E_T_H_I_N_G L_O_N_GGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGG",
                                   "Z_S_S_3=S_O_M_E_T_H_I_N_G L_O_N_GGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGG",
                                  });
                           BufferedReader br = new BufferedReader(new InputStreamReader(
                               proc.getInputStream()));
                           procValue = br.readLine();
                       } catch (IOException e) {
                           e.printStackTrace();
                           fail("Error3");
                       }
                       assertTrue("Error4",procValue.indexOf("S_O_M_E_T_H_I_N_G")!=-1);
       /**/
    }
    
    /**
     * 
     */
    public void test_exec_Str_StrArr_Fil() {
        /**/System.out.println("test_exec_Str_StrArr_Fil");
        String[] command = null;
        if (System.getProperty("os.name").toLowerCase()
                .indexOf("windows") != -1) {
            command = new String[] {"cmd", "/C", "env"};
        } else {
            command = new String[] {"/bin/sh", "-c", "env"};
        }
        String as[];
        int len = 0;
        try {
            Process proc = Runtime.getRuntime().exec(command);
            BufferedReader br = new BufferedReader(new InputStreamReader(proc
                    .getInputStream()));
            while (br.readLine() != null) {
                len++;
            }
        } catch (IOException e) {
            fail("check001: Unexpected IOException");
        }
        as = new String[len];
        try {
            Process proc = Runtime.getRuntime().exec(command);
            BufferedReader br = new BufferedReader(new InputStreamReader(proc
                    .getInputStream()));
            for (int i = 0; i < len; i++) {
                as[i] = br.readLine();
            }

        } catch (IOException e) {
            fail("check002: Unexpected IOException");
        }
/**/
                String command2;
                if (System.getProperty("os.name").toLowerCase().indexOf("windows") != -1) {
                    as = new String[]{"to_avoid=#s1s2f1t1"};//<<<<<<<<<<< !!! to remember
                    command2 = "cmd /C dir";
                } else {
                    command2 = "sh -c pwd";
                }
                try {
                    Process proc = Runtime.getRuntime().exec(command2, as, new File(System.getProperty("java.io.tmpdir")));
                    BufferedReader br = new BufferedReader(new InputStreamReader(
                        proc.getInputStream()));
                    String ln;
                    while ( (ln = br.readLine()) != null) {
                        if(ln.indexOf(System.getProperty("java.io.tmpdir").substring(0,System.getProperty("java.io.tmpdir").length() -1 ))!=-1) {
                            return;
                        }
                   }
                    fail("Error3");
                } catch (IOException e) {
                    e.printStackTrace();
                    fail("Error4");
                }
                fail("Error5");
/**/
    }
    
    /**
     *  
     */
    public void test_exec_Str_F2T1S2Z() {
        /**/System.out.println("test_exec_Str_F2T1S2Z");
        String line;
        if (System.getProperty("os.name").toLowerCase()
                .indexOf("windows") != -1) {
            String strarr[] = {"7", "Hello", "HELL", "Hello", "world",
                    "World hello", "vasa", "d:?*/\\ World", "hello"};
            try {
                String pathList = System.getProperty("java.library.path");
                String[] paths = pathList.split(File.pathSeparator);
                String cmnd = null;
                int ind1;
                for (ind1 = 0; ind1 < paths.length; ind1++) {
                    if (paths[ind1] == null) {
                        continue;
                    }
                    File asf = new java.io.File(paths[ind1] + File.separator
                            + "cmd.exe");
                    if (asf.exists()) {
                        cmnd = paths[ind1] + File.separator + "cmd.exe";
                        break;
                    }
                }
                if (cmnd == null) {
                    if (new java.io.File(
                            (cmnd = "C:\\WINNT\\system32\\cmd.exe")).exists()) { // XXX:IA64?
                    } else if (new java.io.File(
                            (cmnd = "C:\\WINDOWS\\system32\\cmd.exe")).exists()) { // XXX:IA64?
                    } else {
                        fail("check001: cmd.exe hasn't been found! " +
                                "Please, set the path to cmd.exe via " +
                                "java.library.path property.");
                    }
                }
                File f = new java.io.File("C:\\CygWin\\bin");
                Process p;
                if (f.exists()) {
                    p = Runtime.getRuntime().exec(new String[] {
                            cmnd, "/C", "sh", "-c", 
                            "echo $#; echo $0; echo $1; echo $2; echo $3; " +
                            "echo $4; echo $5; echo $6; echo $7",
                            "Hello", "HELL", "\"Hello\" \"world\"",
                            "World hello", "vas\"a d:?*/\\", "\"World hello\""},
                            new String[] {}, f);
                    p.waitFor();
                } else {
                    p = Runtime.getRuntime().exec(new String[] {
                            cmnd, "/C", "sh", "-c",
                            "echo $#; echo $0; echo $1; echo $2; echo $3; " +
                            "echo $4; echo $5; echo $6; echo $7",
                            "Hello", "HELL", "\"Hello\" \"world\"",
                            "World hello", "vas\"a d:?*/\\", "\"World hello\""});
                    if (p.waitFor() != 0) {
                        fail("check002: sh.exe seems to have not been found " +
                                "by default! Please, set the path to sh.exe" +
                                " via java.library.path property.");
                    }
                }
                BufferedReader input = new BufferedReader(
                        new InputStreamReader(p.getErrorStream()));
                boolean flg = false;
                while ((line = input.readLine()) != null) {
                    flg = true;
                    System.err.println("ErrorStream: " + line);
                }
                input.close();
                if (flg) {
                    fail("check003: ErrorStream should be empty!");
                }
                input = new BufferedReader(new InputStreamReader(p
                        .getInputStream()));
                int i = 0;
                while ((line = input.readLine()) != null) {
                    if (!line.equals(strarr[i])) {
                        flg = true;
                        System.out.println(line + " != " + strarr[i]);
                    }
                    i++;
                }
                input.close();
                if (flg) {
                    fail("An uncoincidence was found (see above)!");
                }
            } catch (Exception eeee) {
                fail("check004: Unexpected exception on " +
                        "exec(String[], String[], File)");
            }
        } else if (System.getProperty("os.name").toLowerCase()
                .indexOf("linux") != -1) {
            String strarr[] = {"6",System.getProperty("java.io.tmpdir")
                    + File.separator 
                    + "vasja", "Hello", "HELL", "\"Hello\" \"world\"",
                    "World hello", "vas\"a d:?*/\\" };
            java.io.File fff = null;
            java.io.PrintStream ps = null;
            try {
                fff = new java.io.File(System.getProperty("java.io.tmpdir")
                        + File.separator + "vasja");
                fff.createNewFile();
                ps = new java.io.PrintStream(new java.io.FileOutputStream(fff));
                ps.println("{ echo $#; echo $0; echo $1;  " +
                        "echo $2; echo $3; echo $4; echo $5; }");
            } catch (Throwable e) {
                System.err.println(e);
                System.err.println(System.getProperty("user.home")
                        + File.separator + "vasja");
                new Throwable().printStackTrace();
                fail("Preparing fails!");
            }
            try {
                String pathList = System.getProperty("java.library.path");
                String[] paths = pathList.split(File.pathSeparator);
                String cmnd = null;
                int ind1;
                for (ind1 = 0; ind1 < paths.length; ind1++) {
                    if (paths[ind1] == null) {
                        continue;
                    }
                    File asf = new java.io.File(paths[ind1] + File.separator
                            + "sh");
                    if (asf.exists()) {
                        cmnd = paths[ind1] + File.separator + "sh";
                        break;
                    }
                }
                if (cmnd == null) {
                    cmnd = "/bin/sh";
                }
                File f = new java.io.File("/bin");
                Process p;
                if (f.exists()) {
                    p = Runtime.getRuntime().exec(new String[] {
                            cmnd, System.getProperty("java.io.tmpdir")
                            + File.separator + "vasja", "Hello", "HELL", 
                            "\"Hello\" \"world\"", "World hello", 
                            "vas\"a d:?*/\\", "\"World hello\"" },
                            new String[] {}, f);
                    p.waitFor();
                } else {
                    p = Runtime.getRuntime().exec(new String[] {
                            cmnd, System.getProperty("java.io.tmpdir")
                            + File.separator + "vasja", "Hello", "HELL",
                            "\"Hello\" \"world\"", "World hello",
                            "vas\"a d:?*/\\", "\"World hello\"" });
                    if (p.waitFor() != 0) {
                        fail("check005: sh.exe seems to have not been found" +
                                " by default! Please, set the path to sh.exe" +
                                " via java.library.path property.");
                    }
                }
                BufferedReader input = new BufferedReader(
                        new InputStreamReader(p.getErrorStream()));
                boolean flg = false;
                while ((line = input.readLine()) != null) {
                    flg = true;
                    System.err.println("ErrorStream: " + line);
                }
                input.close();
                if (flg) {
                    fail("check006: ErrorStream should be empty!");
                }

                input = new BufferedReader(new InputStreamReader(p
                        .getInputStream()));
                int i = 0;
                while ((line = input.readLine()) != null) {
                    if (!line.equals(strarr[i])) {
                        flg = true;
                        System.out.println(line + " != " + strarr[i]);
                    }
                    i++;
                }
                input.close();
                if (flg) {
                    fail("check007: An uncoincidence was found (see above)!");
                }
            } catch (Exception eeee) {
                fail("check008: Unexpected exception on " +
                        "exec(String[], String[], File)");
            }
            try {
                fff.delete();
            } catch (Throwable _) {
            }
        } else {
            //UNKNOWN
        }
    }
}
