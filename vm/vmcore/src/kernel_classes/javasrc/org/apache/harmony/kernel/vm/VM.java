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
 * @author Evgueni V. Brevnov
 * @version $Revision: 1.1.2.3.4.3 $
 */ 

package org.apache.harmony.kernel.vm;

import org.apache.harmony.luni.internal.net.www.protocol.jar.JarURLConnection;
import org.apache.harmony.luni.util.DeleteOnExit;

import org.apache.harmony.vm.VMStack;

public final class VM {

	private static boolean closeJars = false;
	private static boolean deleteOnExit = false;

    private VM() {
    }

    /* PUBLIC */

    /**
     * 1) Our implementation uses null for bootstrap class loader. So we return
     *    first non-null class loader.
     * 2) We expect this method will be removed since it's
     *    not safe to return class loader from the stack with out security checks.
     * @deprecated
     */
    public static ClassLoader getNonBootstrapClassLoader() {        
        
        for (int i = 0;;i++) {
            Class clazz = VMStack.getCallerClass(i);
            if (clazz == null) {
                return null;
            }
            ClassLoader loader = getClassLoader(clazz); 
            if (loader != null) {
                return loader;
            }
        }        
    }

    /**
     * Always return null for bootstrap class loader
     */
    public static ClassLoader bootCallerClassLoader() {
        return null;
    }

    /**
     * 1) We expect this method will be removed since it's
     *    not safe to return class loader from the stack with out security checks.
     * @deprecated
     */
    public static ClassLoader callerClassLoader() {
        return getClassLoader(VMStack.getCallerClass(1));
    }

    private static native ClassLoader getClassLoader(Class clazz);

    /* PACKAGE PRIVATE */

    public static final ClassLoader getStackClassLoader(int depth) {
        Class clazz = VMStack.getCallerClass(depth);
        return clazz != null ? getClassLoader(clazz) : null;
    }

    /* PRIVATE */

    /**
     * 1) This is temporary implementation
     * 2) We've proposed another approach to perform shutdown actions.
     */
    public static void closeJars() {
    	class CloseJarsHook implements Runnable {
    		public void run() {
    			JarURLConnection.closeCachedFiles();
    		}
    	}
    	if (!closeJars) {
        	closeJars = true;
        	Runtime.getRuntime().addShutdownHook(new Thread(new CloseJarsHook()));
    	}
    }

    /**
     * 1) This is temporary implementation
     * 2) We've proposed another approach to perform shutdown actions.
     */
    public static void deleteOnExit() {
    	class DeleteOnExitHook implements Runnable {
    		public void run() {
    			DeleteOnExit.deleteOnExit();
    		}
    	}
    	if (!deleteOnExit) {
        	deleteOnExit = true;
        	Runtime.getRuntime().addShutdownHook(new Thread(new DeleteOnExitHook()));
    	}
    }
    
    /**
     *  Returns an intern-ed representation of the 
     *  String
     *  
     *  @param s string to be interned
     *  @return String that has the same contents as 
     *    argument, but from internal pool
     */
    public static String intern(String s) {
        return intern0(s);
    }
    
    /**
     * Invokes native string interning service.
     */
    private static native String intern0(String s);
}
