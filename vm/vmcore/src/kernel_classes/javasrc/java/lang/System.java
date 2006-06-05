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
package java.lang;

import java.io.BufferedInputStream;
import java.io.FileDescriptor;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.FilterOutputStream;
import java.io.InputStream;
import java.io.PrintStream;
import java.security.SecurityPermission;
import java.util.Collections;
import java.util.Hashtable;
import java.util.Map;
import java.util.Properties;
import java.util.PropertyPermission;

import org.apache.harmony.lang.RuntimePermissionCollection;
import org.apache.harmony.misc.SystemUtils;
import org.apache.harmony.vm.VMStack;

/**
 * @com.intel.drl.spec_ref 
 * 
 * @author Roman S. Bushmanov
 * @version $Revision: 1.1.2.2.4.3 $
 */
public final class System {

    static {
        initNanoTime();
    }

    /**
     * This class can not be instantiated.
     */
    private System() {
    }

    /**
     * @com.intel.drl.spec_ref
     */
    public static final PrintStream err = createErr();

    /**
     * @com.intel.drl.spec_ref
     */
    public static final InputStream in = createIn();

    /**
     * @com.intel.drl.spec_ref
     */
    public static final PrintStream out = createOut();

    /**
     * Current system security manager
     */
    private static SecurityManager securityManager = null;

    /**
     * Current system properties
     */
    private static Properties systemProperties = null;

    /**
     * @com.intel.drl.spec_ref
     */
    public static void arraycopy(Object src, int srcPos, Object dest,
                                 int destPos, int length) {
        VMMemoryManager.arrayCopy(src, srcPos, dest, destPos, length);
    }

    /**
     * @com.intel.drl.spec_ref
     */
    public static native long currentTimeMillis();

    /**
     * @com.intel.drl.spec_ref
     */
    public static void exit(int status) {
        Runtime.getRuntime().exit(status);
    }

    /**
     * @com.intel.drl.spec_ref
     */
    public static void gc() {
        Runtime.getRuntime().gc();
    }

    /**
     * @com.intel.drl.spec_ref
     */
    public static String getenv(String name) {
        if (name == null) {
            throw new NullPointerException();
        }
        SecurityManager sm = securityManager;
        if (sm != null) {
            sm.checkPermission(new RuntimePermission("getenv." + name));
        }
        return getenvUnsecure(name);
    }

    // FIXME: MODIFY THE SIGNATURE FOR 1.5
    /**
     * @com.intel.drl.spec_ref
     */
    public static Map getenv() {
        SecurityManager sm = securityManager;
        if (sm != null) {
            sm.checkPermission(RuntimePermissionCollection.GETENV_PERMISSION);
        }
        Map envMap = getenvUnsecure();
        if (envMap == null) {
            envMap = new Hashtable();
        }
        return Collections.unmodifiableMap(envMap);
    }

    /**
     * @com.intel.drl.spec_ref
     */
    public static Properties getProperties() {
        if (securityManager != null) {
            securityManager.checkPropertiesAccess();
        }
        return getPropertiesUnsecure();
    }

    /**
     * @com.intel.drl.spec_ref
     */
    public static String getProperty(String key) {
        return getProperty(key, null);
    }

    /**
     * @com.intel.drl.spec_ref
     */
    public static String getProperty(String key, String def) {
        SecurityManager sm = securityManager;
        if (sm != null) {
            sm.checkPropertyAccess(key);
        } else if (key.length() == 0) {
            throw new IllegalArgumentException("key is empty");
        }
        Properties props = getPropertiesUnsecure();
        return props.getProperty(key, def);
    }
    
    /**
     * @com.intel.drl.spec_ref
     */
    public static String clearProperty(String key){
        SecurityManager sm = securityManager;
        if (sm != null) {
            sm.checkPermission(new PropertyPermission(key, "write"));
        } else if (key.length() == 0) {
            throw new IllegalArgumentException("key is empty");
        }
        Properties props = getPropertiesUnsecure();
        return (String)props.remove(key);
    }

    /**
     * @com.intel.drl.spec_ref
     */
    public static SecurityManager getSecurityManager() {
        return securityManager;
    }

    /**
     * @com.intel.drl.spec_ref
     */
    public static int identityHashCode(Object object) {
        return VMMemoryManager.getIdentityHashCode(object);
    }

/*
//	#########################################################################################
//	############################### 1.5 extention start #####################################
//	#########################################################################################

//import java.nio.channels.spi.SelectorProvider; //move this above later
//import java.nio.channels.Channel;              //move this above later
//import java.io.IOException;                    //move this above later

    / **
     * @com.intel.drl.spec_ref
     * /
    public static Channel inheritedChannel() throws IOException{
    	//XXX:does it mean the permission of the "access to the channel"?
    	//If YES then this checkPermission must be removed because it should be presented into java.nio.channels.spi.SelectorProvider.inheritedChannel()
    	//If NO  then some other permission name (which one?) should be used here
    	//and the corresponding constant should be placed within org.apache.harmony.lang.RuntimePermission class: 
        if (securityManager != null) {
        	securityManager.checkPermission(new RuntimePermission("inheritedChannel")); //see java.nio.channels.spi.SelectorProvider.inheritedChannel() spec
        }
        
        return SelectorProvider.provider().inheritedChannel();
    }

//	#########################################################################################
//	############################### 1.5 extention end #######################################
//	#########################################################################################
*/

    /**
     * @com.intel.drl.spec_ref
     */
    public static void load(String filename) {
        Runtime.getRuntime().load0(
                                   filename,
                                   VMClassRegistry.getClassLoader(VMStack
                                       .getCallerClass(0)), true);
    }

    public static void loadLibrary(String libname) {
        Runtime.getRuntime().loadLibrary0(
                                          libname,
                                          VMClassRegistry
                                              .getClassLoader(VMStack
                                                  .getCallerClass(0)), true);
    }

    /**
     * @com.intel.drl.spec_ref Note: Only Windows and Linux operating systems
     *                         are supported by current version
     */
    public static String mapLibraryName(String libname) {
        switch (SystemUtils.getOS()) {
        case SystemUtils.OS_WINDOWS:
            return libname + ".dll";
        case SystemUtils.OS_LINUX:
            return "lib" + libname + ".so";
        default:
            throw new UnsupportedOperationException(
                "The operation is supported for Windows and Linux operating systems only");
        }
    }
  
    /**
     * @com.intel.drl.spec_ref
     */
    public static native long nanoTime();

    /**
     * @com.intel.drl.spec_ref
     */
    public static void runFinalization() {
        Runtime.getRuntime().runFinalization();
    }

    /**
     * @com.intel.drl.spec_ref
     */
    public static void runFinalizersOnExit(boolean value) {
        Runtime.runFinalizersOnExit(value);
    }

    /**
     * @com.intel.drl.spec_ref
     */
    public static void setErr(PrintStream err) {
        SecurityManager sm = securityManager;
        if (sm != null) {
            sm.checkPermission(RuntimePermissionCollection.SET_IO_PERMISSION);
        }
        setErrUnsecure(err);
    }

    /**
     * @com.intel.drl.spec_ref
     */
    public static void setIn(InputStream in) {
        SecurityManager sm = securityManager;
        if (sm != null) {
            sm.checkPermission(RuntimePermissionCollection.SET_IO_PERMISSION);
        }
        setInUnsecure(in);
    }

    /**
     * @com.intel.drl.spec_ref
     */
    public static void setOut(PrintStream out) {
        SecurityManager sm = securityManager;
        if (sm != null) {
            sm.checkPermission(RuntimePermissionCollection.SET_IO_PERMISSION);
        }
        setOutUnsecure(out);
    }

    /**
     * @com.intel.drl.spec_ref
     */
    public static synchronized void setProperties(Properties props) {
        if (securityManager != null) {
            securityManager.checkPropertiesAccess();
        }
        systemProperties = props;
    }

    /**
     * @com.intel.drl.spec_ref
     */
    public static String setProperty(String key, String value) {
        if (key.length() == 0) {
            throw new IllegalArgumentException("key is empty");
        }
        SecurityManager sm = securityManager;
        if (sm != null) {
            sm.checkPermission(new PropertyPermission(key, "write"));
        }
        Properties props = getPropertiesUnsecure();
        return (String)props.setProperty(key, value);
    }

    /**
     * @com.intel.drl.spec_ref
     */
    public static synchronized void setSecurityManager(SecurityManager sm) {
        if (securityManager != null) {
            securityManager
                .checkPermission(RuntimePermissionCollection.SET_SECURITY_MANAGER_PERMISSION);
        }

        if (sm != null) {
            // before the new manager assumed office, make a pass through 
            // the common operations and let it load needed classes (if any),
            // to avoid infinite recursion later on 
            try {
                sm.checkPermission(new SecurityPermission("getProperty.package.access")); 
            } catch (Exception ignore) {}
            try {
                sm.checkPackageAccess("java.lang"); 
            } catch (Exception ignore) {}
        }

        securityManager = sm;
    }

    /**
     * Constructs a system <code>err</code> stream. This method is used only
     * for initialization of <code>err</code> field
     */
    private static PrintStream createErr() {
        //return new PrintStream(new BufferedOutputStream(new FileOutputStream(
        //    FileDescriptor.err)), true);
        //FIXME: WORKAROUND FOR BUG
        // We can't use here simple FileOutputStream cuz such code causes our VM
        // crash while initialization of main Thread object
        //
        // Correct fix of the problem is closing out and err print streams on VM
        // exit for example, in Runtime.exit(). Another bug prevents us of doing
        // such thing
        return new PrintStream(new FilterOutputStream(new FileOutputStream(
            FileDescriptor.err)), true);
    }

    /**
     * Constructs a system <code>in</code> stream. This method is used only
     * for initialization of <code>in</code> field
     */
    private static InputStream createIn() {
        return new BufferedInputStream(new FileInputStream(FileDescriptor.in));
    }

    /**
     * Constructs a system <code>out</code> stream. This method is used only
     * for initialization of <code>out</code> field
     */
    private static PrintStream createOut() {
        //return new PrintStream(new BufferedOutputStream(new FileOutputStream(
        //    FileDescriptor.out)), true);
        //FIXME: WORKAROUND FOR BUG
        return new PrintStream(new FilterOutputStream(new FileOutputStream(
            FileDescriptor.out)), true);
    }

    /**
     * Returns system properties without security checks. Initializes the system
     * properties if it isn't done yet.
     */
    private static synchronized Properties getPropertiesUnsecure() {
        if (systemProperties == null) {
            systemProperties = VMExecutionEngine.getProperties();
        }
        return systemProperties;
    }

    /**
     * Sets the value of <code>err</code> field without any security checks
     */
    private static native void setErrUnsecure(PrintStream err);

    /**
     * Sets the value of <code>in</code> field without any security checks
     */
    private static native void setInUnsecure(InputStream in);

    /**
     * Sets the value of <code>out</code> field without any security checks
     */
    private static native void setOutUnsecure(PrintStream out);

    // FIXME: MODIFY THE SIGNATURE FOR 1.5
    /**
     * Returns enviromnent variables to values map without any security checks
     */
    private static native Map getenvUnsecure();

    /**
     * Returns the value of the environment variable specified by
     * <code>name</code> argument or <code>null</code> if it is not set
     */
    private static native String getenvUnsecure(String name);

    /**
     * Initializes nanosecond system timer
     */
    private static native void initNanoTime();

    /**
     *  To rethrow without mentioning within throws clause.
     */
    native static void rethrow(Throwable tr);
}

