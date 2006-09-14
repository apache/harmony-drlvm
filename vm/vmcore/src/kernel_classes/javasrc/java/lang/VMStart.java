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
/*
 * @author Artem Aliev
 * @version $Revision: 1.1.2.2.4.4 $
 */

package java.lang;

import java.lang.reflect.Method;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Modifier;
import java.security.AccessController;
import java.security.PrivilegedAction;
/**
 * This class is a VM start point.
 * It does following steps:<ul>
 * <li> starts all helper threads as Finalizer and Execution Manager threads.
 * <li> parse system properties and configure VM environment. 
 * <li> load main class and start main method in new thread.
 * <li> catch Throwable.
 * <li> call System.exit(), to execute shutdown hooks
 * </ul> 
 */
class VMStart {
	static int exitCode = 0;
    public static void initialize() {
        //start helper threads such as Finalizer
        startHelperThreads();
        // add default shutdown hooks, to stop helper threads.
        Runtime.getRuntime().addShutdownHook(new DefaultShutDownHook());
        // do additional tasks specified in system properties       
        parseSystemProperties();
    }

    public static void start (String mainClassName, String args[]) {
        try {
            // create main thread in new thread group
            MainThread mainThread = new MainThread(mainClassName, args);
            mainThread.start();
            //startHelperThreads();
            //System.out.println("Join Group " + Thread.currentThread().getThreadGroup());
        } catch (Throwable e) {
            e.printStackTrace(System.err);
            System.exit(-1);
        } 
    }

    public static void shutdown() {
        try {
            joinAllNonDaemonThreads();
            //System.out.println("Joined all ");
            System.exit(exitCode);
        } catch (Throwable e) {
            e.printStackTrace(System.err);
            System.exit(-1);
        } 
    }
    
    public static void parseSystemProperties() {
    }
    
    public static void startHelperThreads() {
        try {
            // start helper threads.
            FinalizerThread.initialize();
            EMThreadSupport.initialize();
        } catch (Throwable e) {
            System.err.println("Internal error");
            e.printStackTrace(System.err);
            Runtime.getRuntime().halt(-1);
        }
    }
    // should shutdown helper threads
    static class DefaultShutDownHook extends Thread {
        public void run() {
            FinalizerThread.shutdown();
            EMThreadSupport.shutdown();
        }
    }
    
    // main thread
    static class MainThread extends Thread {
        String mainClass; 
        String args[];
        public boolean started = false;
        
        MainThread (String mainClass, String args[]) {
            super(new ThreadGroup("main"), "main");
            this.mainClass = mainClass;
            this.args = args;
            
        }
        public void run() {
        }

        void runImpl() {
            // prevent access from user classes to run() method
            if(started) {
                return;
            }
            started = true;
            
            try {
                // load and start main class
                ClassLoader loader = ClassLoader.getSystemClassLoader();
                Class cl = Class.forName(mainClass, true, loader);
                final Method mainMethod = cl.getMethod("main", 
                        new Class[]{String[].class});
                int expectedModifiers = (Modifier.PUBLIC | Modifier.STATIC);
                if ((mainMethod.getModifiers() & expectedModifiers) != expectedModifiers 
                    || mainMethod.getReturnType() != Void.TYPE) {
                    throw new NoSuchMethodError(
                        "The method main must be declared public, static, and void.");
                }
                // the class itself may be non-public
                AccessController.doPrivileged(new PrivilegedAction<Object>() {
                    public Object run() {
                        mainMethod.setAccessible(true);
                        return null;
                    }
                });

                mainMethod.invoke(null, new Object[] {args});
            } catch (InvocationTargetException userException) {
            	exitCode = 1;
                userException.getCause().printStackTrace();
            } catch (Throwable e) {
            	exitCode = 1;
                e.printStackTrace(System.err);
            }  finally {
                group.remove(this);
                synchronized(lock) {
                    this.isAlive = false;
                    lock.notifyAll();
                }
           }
       }
    }
    
    static void mainThreadInit() {
        Thread theFirstThread = new Thread(true);
    }
    
    
    native static void joinAllNonDaemonThreads();
}
