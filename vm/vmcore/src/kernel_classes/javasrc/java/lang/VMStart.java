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
 * This class does the following:
 * <li> starts Finalizer and Execution Manager helper threads.
 * <li> parses system properties and configure VM environment.
 * </ul> 
 */
class VMStart {

    public static void initialize() {
        //start helper threads such as Finalizer
        startHelperThreads();
        // add default shutdown hooks, to stop helper threads.
        Runtime.getRuntime().addShutdownHook(new DefaultShutDownHook());
        // do additional tasks specified in system properties       
        parseSystemProperties();
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
            Runtime.getRuntime().halt(1);
        }
    }
    // should shutdown helper threads
    static class DefaultShutDownHook extends Thread {
        
        public DefaultShutDownHook() {
            super("Thread-shutdown");
        }

        public void run() {
            EMThreadSupport.shutdown();
        }
    }
}
