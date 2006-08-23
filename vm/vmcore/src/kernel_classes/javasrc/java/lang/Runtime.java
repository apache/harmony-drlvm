/*
 *  Copyright 2005-2006 The Apache Software Foundation or its licensors, as applicable.
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
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
 * @version $Revision: 1.1.2.2.4.5 $
 * 
 * This Core API Runtime class ("Software") is furnished under license and may
 * only be used or copied in accordance with the terms of that license.
 * 
 **/

/**
 * ###############################################################################
 * ###############################################################################
 * TODO LIST:
 * 1. Provide correct processing the case if process isn't started because of some 
 *    reason
 * 2. Clean and develop the native support
 * 3. Think of the default/undefault buffering
 * 3. Runtime.SubProcess.SubInputStream.read(b, off, len) and
 *    Runtime.SubProcess.SubErrorStream.read(b, off, len) should be effectively
 *    reimplemented on the native side.
 * ###############################################################################
 * ###############################################################################
 **/

package java.lang;

import java.util.StringTokenizer;
import java.io.File;
import java.io.InputStream;
import java.io.OutputStream;
import java.io.IOException;
import java.lang.UnsatisfiedLinkError;
import java.lang.VMExecutionEngine;
import java.lang.VMMemoryManager;

import java.util.Vector;

import org.apache.harmony.vm.VMStack;
//import org.apache.harmony.vm.VMStack.CallsTraceElement;

/**
 *
 *  @com.intel.drl.spec_ref
 *
 **/

public class Runtime {

    //--------------------------------------------------------------------------------
    //  Nested Runtime.ShutdownVM class:
    //-------------------------------------------------------------------------------- 

    static final class ShutdownVM {

        private static class Synchro {
        };

        private static Vector hooksList = new Vector();

        private static int VMState = 0; // 0 - normal work
        // 1 - being shutdown sequence running
        // 2 - being finalizing

        private static boolean finalizationOnExit = false;

        /**
         * 
         *  Support for runFinalizersOnExit(boolean) from Runtime class
         * 
         */

        static void runFinalizersOnExit(boolean value) {
            synchronized (Synchro.class) {
                FinalizerThread.setFinalizersOnExit(value);
                finalizationOnExit = value;
            }
        }

        /**
         * 
         *  Support for exit(int) from Runtime class
         * 
         */

        static void exit(int status) {
            // The virtual machine's shutdown sequence consists of two phases:

            // #1:
            //  In the first phase all registered shutdown hooks, if any, are started in some
            //  unspecified order and allowed to run concurrently until they finish.
            synchronized (Synchro.class) {
                try {
                if (VMState == 0) {
                    VMState = 1;
                    if (hooksList != null) {
                        for (int i = 0; i < hooksList.size(); i++) {
                            ((Thread)hooksList.elementAt(i)).start();
                        }

                        for (int i = 0; i < hooksList.size(); i++) {
                            while (true){
                                try {
                                    ((Thread)hooksList.elementAt(i)).join();
                                    break;
                                } catch (InterruptedException e) {
                                    continue;
                                }
                            }
                        }

                        hooksList.removeAllElements();
                    }
                }
                } catch (Throwable e) {} // skip any exceptions in shutdown sequence
            }

            // #2:
            //  In the second phase all uninvoked finalizers are run if finalization-on-exit has been 
            //  enabled. Once this is done the virtual machine halts."
            synchronized (Synchro.class) {
                VMState = 2;
                VMExecutionEngine.exit(status, finalizationOnExit); 
            }

        }

        /**
         * 
         * Support for addShutdownHook(Thread) from Runtime class
         * 
         */

        static void addShutdownHook(Thread hook) throws IllegalStateException, IllegalArgumentException {
            synchronized (hooksList) {
                if (hooksList.contains((Object) hook)) {
                    throw new IllegalArgumentException();
                }
            }
            synchronized (Synchro.class) {
                if (VMState > 0) {
                    throw new IllegalStateException();
                }
            }
            synchronized (hooksList) {
                hooksList.addElement((Object) hook);
            }
        }

        /**
         * 
         *   Support for removeShutdownHook(Thread) from Runtime class
         * 
         */

        static boolean removeShutdownHook(Thread hook) throws IllegalStateException {
            synchronized (Synchro.class) {
                if (VMState > 0) {
                    throw new IllegalStateException();
                }
            }
            synchronized (hooksList) {
                return hooksList == null ? false : hooksList.removeElement((Object) hook);
            }
        }

        /**
         * 
         *   Support for halt(int) from Runtime class
         * 
         */

        static void halt(int status) {
            VMExecutionEngine.exit(status, false);
        }
    }

    //--------------------------------------------------------------------------------
    //  Nested protected Runtime.SubProcess class:
    //-------------------------------------------------------------------------------- 

    static final class SubProcess extends Process {


        final static class SubInputStream extends InputStream {

            long streamHandle;

            /**
             * Constructs a new SubInputStream instance. 
             **/
            SubInputStream() {
                this.streamHandle = -1;
            }

            /**
             *
             * Reads the next byte of data from the input stream....
             * 
             * See:
             * int read() from InputStream 
             * 
             **/
            private final native int readInputByte0(long handle) throws IOException;

            public final int read() throws IOException {
                return readInputByte0(this.streamHandle);
            }

            /**
             *
             * Returns the number of bytes that can be read (or skipped over) from
             * this input stream without blocking by the next caller
             * of a method for this input stream...
             * 
             * See:
             * int available() from InputStream 
             * 
             **/
            private final native int available0(long handle);

            public final int available() throws IOException {
                return available0(this.streamHandle);
            }

            /**
             *
             * Reads len bytes from input stream ...
             * 
             * See:
             *  void read(byte[], int, int) from InputStream 
             * 
             **/

            public int read(byte[] b, int off, int len) throws IOException {
                if (b == null) {
                    throw new NullPointerException();
                }

                if (off < 0 || len < 0 || off + len > b.length) {
                    throw new IndexOutOfBoundsException();
                }

                if (len == 0) {
                    return 0;
                }
                int c = read();
                if (c == -1) {
                    return -1;
                }
                b[off] = (byte) c;

                int i = 1;
                for(; i < len; i++) {
                    try {
                        if (available() != 0) {
                            int r = read();
                            if (r != -1) {
                                b[off + i] = (byte) r;
                                continue;
                            }
                            return i;
                        }
                    } catch(IOException e) {
                        break; //If any subsequent call to read() results in a IOException
                    }
                    break; //but a smaller number may be read, possibly zero.
                }
                return i;
            }

            /**
             *
             * Closes this input stream and releases any system resources associated
             *  with the stream.
             * 
             * See:
             * void close() from InputStream 
             * 
             **/
            private final native void close0(long handle) throws IOException;

            public final synchronized void close() throws IOException {
                if (streamHandle == -1) return;

                long handle = streamHandle;
                streamHandle = -1;
                close0(handle);
            }

            protected void finalize() throws Throwable {
                if (streamHandle != -1)
                    close0(this.streamHandle);
            }
        }

        //--------------------------------------------------------------------------------
        //  Nested Class Runtime.SubProcess.SubOutputStream :
        //-------------------------------------------------------------------------------- 

        /**
         *
         * Extends OutputStream class.
         *
         * 
         **/

        final static class SubOutputStream extends OutputStream {

            long streamHandle;

            /**
             * Constructs a new SubOutputStream instance. 
             **/
            SubOutputStream() {
                this.streamHandle = -1;
            }

            /**
             *
             * Writes the specified byte to this output stream ...
             * 
             * See:
             * void write(int) from OutputStream 
             * 
             **/

            private final native void writeOutputByte0(long handle, int bt);

            public final void write(int b) throws IOException {
                writeOutputByte0(this.streamHandle, b);
            }

            /**
             *
             * Writes len bytes from the specified byte array starting at 
             * offset off to this output stream ...
             * 
             * See:
             *  void write(byte[], int, int) from OutputStream 
             * 
             **/

            private final native void writeOutputBytes0(long handle, byte[] b, int off, int len);

            public final void write(byte[] b, int off, int len) throws IOException {
                if (b == null) {
                    throw new NullPointerException();
                }

                if (off < 0 || len < 0 || off + len > b.length) {
                    throw new IndexOutOfBoundsException();
                }

                writeOutputBytes0(this.streamHandle, b, off, len);
            }

            /**
             *
             * Writes b.length bytes from the specified byte array to this output stream...
             * 
             * See:
             * void write(byte[]) from OutputStream 
             * 
             **/

            public final void write(byte[] b) throws IOException {
                write(b, 0, b.length);
            }

            /**
             *
             * Flushes this output stream and forces any buffered output 
             * bytes to be written out ...
             * 
             * See:
             * void flush() from OutputStream 
             * 
             **/

            private final native void flush0(long handle);

            public final void flush() throws IOException {
                flush0(this.streamHandle);
            }

            /**
             *
             * Closes this output stream and releases any system resources 
             * associated with this stream ...
             * 
             * See:
             * void close() from OutputStream 
             * 
             **/

            private final native void close0(long handle);

            public final synchronized void close() throws IOException {
                if (streamHandle == -1) return;
                long handle = streamHandle;
                streamHandle = -1;
                close0(handle);
            }

            protected void finalize() throws Throwable {
                if (streamHandle != -1)
                    close0(this.streamHandle);
            }
        }

        ////////////////////////////////////////////////////////////////////////////////////////////////////////////
        /////////////////////////////////////     Runtime.SubProcess     BODY     //////////////////////////////////
        ////////////////////////////////////////////////////////////////////////////////////////////////////////////

        private int processHandle;
        private int processExitCode;
        private SubProcess.SubOutputStream os;
        private SubProcess.SubInputStream is;
        private SubProcess.SubInputStream es;

        protected SubProcess() { // An application cannot create its own instance of this class.
            this.processHandle = -1;
            this.processExitCode = 0;
            this.os = null;
            this.is = null;
            this.es = null;
        }

        private final native void close0(int handle);

        protected void finalize() throws Throwable {
            if (processHandle != -1)
                close0(this.processHandle);
        }

        /**
         *
         * See:
         * OutputStream getOutputStream() from Process 
         *
         **/

        public final OutputStream getOutputStream() {
            return os;
        }

        /**
         *
         * See:
         * InputStream getInputStream() from Process 
         * 
         **/

        public final InputStream getInputStream() {
            return is;
        }

        /**
         *
         * See:
         * InputStream getErrorStream() from Process 
         * 
         **/

        public final InputStream getErrorStream() {
            return es;
        }

        private final native boolean getState0(int thisProcessHandle);

        private final native void createProcess0(Object[] cmdarray, Object[] envp, String dir, long[] ia);

        protected final void execVM(String[] cmdarray, String[] envp, String dir) throws IOException {
            // Do all java heap allocation first, in order to throw OutOfMemory
            // exception early, before we have actually executed the process.
            // Otherwise we should do somewhat complicated cleanup.
            os = new SubProcess.SubOutputStream();
            is = new SubProcess.SubInputStream();
            es = new SubProcess.SubInputStream();

            long[] la = new long[4];
            createProcess0(cmdarray, envp, dir, la);
            if (la[0] == 0) {
                String cmd = null;
                for(int i = 0; i < cmdarray.length; i++) {
                    if (i == 0) {
                        cmd = "\"" + cmdarray[i] + "\"";
                    } else {
                        cmd = cmd + " " + cmdarray[i];
                    }
                }
                throw new IOException("The creation of the Process has just failed: " + cmd); 
            }
            this.processHandle = (int)la[0];
            os.streamHandle = la[1];
            is.streamHandle = la[2];
            es.streamHandle = la[3];
        }

        /**
         *
         * See:
         * int waitFor() from Process 
         * 
         **/

        public int waitFor() throws InterruptedException {
            while (true) {
                synchronized (this) {
                    if (getState0(processHandle)) break;
                }
                Thread.sleep(50);
            }

            return processExitCode;
        }

        /**
         *
         * See:
         * int exitValue() from Process 
         * 
         **/

        public synchronized int exitValue() throws IllegalThreadStateException {
            if (!getState0(processHandle)) {
                throw new IllegalThreadStateException("process has not exited");
            }

            return processExitCode;
        }

        /**
         *
         * See:
         * void destroy() from Process 
         * 
         **/

        private final native void destroy0(int thisProcessHandle);

        public synchronized final void destroy() {
            destroy0(processHandle);
        }

    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////
    //////////////////////////////////////////     RUNTIME     BODY     ////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////

    private static Runtime thisApplicationRuntime = new Runtime(); // "Every Java application has a single instance of class Runtime ..."

    private Runtime() { // "An application cannot create its own instance of this class."
    }

    /**
     * 
     * @com.intel.drl.spec_ref  
     * 
     */

    public static Runtime getRuntime() {
        return thisApplicationRuntime;
    }

    /**
     * 
     * @com.intel.drl.spec_ref  
     * 
     */

    public void exit(int status) throws SecurityException {
        SecurityManager currentSecurity = System.getSecurityManager();

        if (currentSecurity != null) {
            currentSecurity.checkExit(status);
        }
        ShutdownVM.exit(status);
    }

    /**
     * 
     * @com.intel.drl.spec_ref  
     * 
     */

    public void addShutdownHook(Thread hook) {
        SecurityManager currentSecurity = System.getSecurityManager();

        if (currentSecurity != null) {
            currentSecurity.checkPermission(new RuntimePermission("shutdownHooks"));
        }

        ShutdownVM.addShutdownHook(hook);
    }

    /**
     * 
     * @com.intel.drl.spec_ref  
     * 
     */

    public boolean removeShutdownHook(Thread hook) {
        SecurityManager currentSecurity = System.getSecurityManager();

        if (currentSecurity != null) {
            currentSecurity.checkPermission(new RuntimePermission("shutdownHooks"));
        }

        return ShutdownVM.removeShutdownHook(hook);
    }

    /**
     * 
     * @com.intel.drl.spec_ref  
     * 
     */

    public void halt(int status) {
        SecurityManager currentSecurity = System.getSecurityManager();

        if (currentSecurity != null) {
            currentSecurity.checkExit(status);

        }

        ShutdownVM.halt(status);
    }

    /**
     * 
     * @com.intel.drl.spec_ref  
     * 
     */

    public static void runFinalizersOnExit(boolean value) {
        SecurityManager currentSecurity = System.getSecurityManager();

        if (currentSecurity != null) {
            currentSecurity.checkExit(0);
        }

        ShutdownVM.runFinalizersOnExit(value);
    }

    /**
     * 
     * @com.intel.drl.spec_ref  
     * 
     */

    public Process exec(String command) throws IOException {
        return exec(command, null, null);
    }

    /**
     * 
     * @com.intel.drl.spec_ref  
     * 
     */

    public Process exec(String cmd, String[] envp) throws IOException {
        return exec(cmd, envp, null);
    }

    /**
     * 
     * @com.intel.drl.spec_ref  
     * 
     */

    public Process exec(String command, String[] envp, File dir) throws IOException {
        if (command == null) {
            throw new NullPointerException();
        }
        if (command.length() == 0) {
            throw new IllegalArgumentException();
        }

        StringTokenizer st = new StringTokenizer(command);
        String[] cmdarray = new String[st.countTokens()];
        int i = 0;

        while (st.hasMoreTokens()) {
            cmdarray[i++] = st.nextToken();

        }

        return exec(cmdarray, envp, dir);

    }

    /**
     * 
     * @com.intel.drl.spec_ref  
     * 
     */

    public Process exec(String[] cmdarray) throws IOException {
        return exec(cmdarray, null, null);

    }

    /**
     * 
     * @com.intel.drl.spec_ref  
     * 
     */

    public Process exec(String[] cmdarray, String[] envp) throws IOException, NullPointerException, IndexOutOfBoundsException, SecurityException {
        return exec(cmdarray, envp, null);

    }

    /**
     * 
     * @com.intel.drl.spec_ref  
     * 
     */

    public Process exec(String[] cmdarray, String[] envp, File dir) throws IOException {
        SecurityManager currentSecurity = System.getSecurityManager();

        if (currentSecurity != null) {
            currentSecurity.checkExec(cmdarray[0]);
        }

        if (cmdarray == null) {
            throw new NullPointerException("Command argument shouldn't be empty.");
        }
        if (cmdarray.length == 0) {
            throw new IndexOutOfBoundsException();
        }
        //XXX:
        //#IN004# Should we check cmdarray's direction values: cmdarray[i] != null ?
        //#IN004# Should we check: envp != null ?
        //#IN004# Should we check envp's direction values: envp[i] != null ?

        String dirPathName = (dir != null ? dir.getPath() : null);

        SubProcess sp = new SubProcess();

        sp.execVM(cmdarray, envp, dirPathName);

        return sp;

    }

    /**
     * 
     * @com.intel.drl.spec_ref  
     * 
     */

    public int availableProcessors() {
        return VMExecutionEngine.getAvailableProcessors();
    }

    /**
     * 
     * @com.intel.drl.spec_ref  
     * 
     */

    public long freeMemory() {
        return VMMemoryManager.getFreeMemory();
    }

    /**
     * 
     * @com.intel.drl.spec_ref  
     * 
     */

    public long totalMemory() {
        return VMMemoryManager.getTotalMemory();
    }

    /**
     * 
     * @com.intel.drl.spec_ref  
     * 
     */

    public long maxMemory() {
        return VMMemoryManager.getMaxMemory();
    }

    /**
     * 
     * @com.intel.drl.spec_ref  
     * 
     */

    public void gc() {
        VMMemoryManager.runGC();
    }

    /**
     * 
     * @com.intel.drl.spec_ref  
     * 
     */

    public void runFinalization() {
        //VMMemoryManager.runFinalzation(); 
        VMMemoryManager.runFinalization();
    }

    /**
     * 
     * @com.intel.drl.spec_ref  
     * 
     */

    public void traceInstructions(boolean on) {
        VMExecutionEngine.traceInstructions(on);
    }

    /**
     * 
     * @com.intel.drl.spec_ref  
     * 
     */

    public void traceMethodCalls(boolean on) {
        VMExecutionEngine.traceMethodCalls(on);
    }

    /**
     * 
     * @com.intel.drl.spec_ref  
     * 
     */

    public void load(String filename) throws SecurityException, UnsatisfiedLinkError {
        load0(filename, VMClassRegistry.getClassLoader(VMStack.getCallerClass(0)), true);
    }

    void load0(String filename, ClassLoader cL, boolean check) throws SecurityException, UnsatisfiedLinkError {
        if (check) {
            if (filename == null) {
                throw new NullPointerException();
            }

            SecurityManager currentSecurity = System.getSecurityManager();

            if (currentSecurity != null) {
                currentSecurity.checkLink(filename);
            }
        }
        VMClassRegistry.loadLibrary(filename, cL); // Should throw UnsatisfiedLinkError if needs.
    }

    /**
     * 
     * @com.intel.drl.spec_ref  
     * 
     */

    public void loadLibrary(String libname) throws SecurityException, UnsatisfiedLinkError {
        loadLibrary0(libname, VMClassRegistry.getClassLoader(VMStack.getCallerClass(0)), true);
    }

    void loadLibrary0(String libname, ClassLoader cL, boolean check) throws SecurityException, UnsatisfiedLinkError {
        if (check) {
            if (libname == null) {
                throw new NullPointerException();
            }

            SecurityManager currentSecurity = System.getSecurityManager();

            if (currentSecurity != null) {
                currentSecurity.checkLink(libname);
            }
        }

        String libFullName = null;

        if (cL!=null) {
            libFullName = cL.findLibrary(libname);
        }
        if (libFullName == null) {
            String allPaths = null;

            //XXX: should we think hard about security policy for this block?:
            String jlp = System.getProperty("java.library.path");
            String vblp = System.getProperty("vm.boot.library.path");
            String udp = System.getProperty("user.dir");
            String pathSeparator = System.getProperty("path.separator");
            String fileSeparator = System.getProperty("file.separator");
            allPaths = (jlp!=null?jlp:"")+(vblp!=null?pathSeparator+vblp:"")+(udp!=null?pathSeparator+udp:"");

            if (allPaths.length()==0) {
                throw new UnsatisfiedLinkError("Can not find the library: " +
                        libname);
            }

            //String[] paths = allPaths.split(pathSeparator);
            String[] paths;
            {
                java.util.ArrayList res = new java.util.ArrayList();
                int curPos = 0;
                int l = pathSeparator.length();
                int i = allPaths.indexOf(pathSeparator);
                int in = 0;
                while (i != -1) {
                    String s = allPaths.substring(curPos, i); 
                    res.add(s);
                    in++;
                    curPos = i + l;
                    i = allPaths.indexOf(pathSeparator, curPos);
                }

                if (curPos <= allPaths.length()) {
                    String s = allPaths.substring(curPos, allPaths.length()); 
                    in++;
                    res.add(s);
                }

                paths = (String[]) res.toArray(new String[in]);
            }

            libname = System.mapLibraryName(libname);
            for (int i=0; i<paths.length; i++) {
                if (paths[i]==null) {
                    continue;
                }
                libFullName = paths[i] + fileSeparator + libname;
                try {
                    this.load0(libFullName, cL, false);
                    return;
                } catch (UnsatisfiedLinkError e) {
                }
            }
        } else {
            this.load0(libFullName, cL, false);
            return;
        }
        throw new UnsatisfiedLinkError("Can not find the library: " +
                libname);
    }

    /**
     * 
     * @com.intel.drl.spec_ref  
     * 
     */

    public InputStream getLocalizedInputStream(InputStream in) {
        //XXX: return new BufferedInputStream( (InputStream) (Object) new InputStreamReader( in ) );
        return in;
    }

    /**
     * 
     * @com.intel.drl.spec_ref  
     * 
     */

    public OutputStream getLocalizedOutputStream(OutputStream out) {
        //XXX: return new BufferedOutputStream( (OutputStream) (Object) new OutputStreamWriter( out ) );
        return out;
    }

}
