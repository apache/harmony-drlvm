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

package java.lang;

import static org.apache.harmony.vm.ClassFormat.ACC_ANNOTATION;
import static org.apache.harmony.vm.ClassFormat.ACC_ENUM;
import static org.apache.harmony.vm.ClassFormat.ACC_INTERFACE;
import static org.apache.harmony.vm.ClassFormat.ACC_SYNTHETIC;

import java.io.Externalizable;
import java.io.InputStream;
import java.io.Serializable;
import java.lang.annotation.Annotation;
import java.lang.annotation.Inherited;
import java.lang.ref.SoftReference;
import java.lang.reflect.AnnotatedElement;
import java.lang.reflect.Constructor;
import java.lang.reflect.Field;
import java.lang.reflect.GenericDeclaration;
import java.lang.reflect.GenericSignatureFormatError;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.MalformedParameterizedTypeException;
import java.lang.reflect.Member;
import java.lang.reflect.Method;
import java.lang.reflect.Modifier;
import java.lang.reflect.Type;
import java.lang.reflect.TypeVariable;
import java.net.URL;
import java.security.AccessController;
import java.security.AllPermission;
import java.security.Permissions;
import java.security.PrivilegedAction;
import java.security.ProtectionDomain;
import java.util.ArrayList;
import java.util.Collection;
import java.util.LinkedHashSet;
import java.util.Map;

import org.apache.harmony.lang.RuntimePermissionCollection;
import org.apache.harmony.lang.reflect.Reflection;
import org.apache.harmony.lang.reflect.parser.Parser;
import org.apache.harmony.vm.VMGenericsAndAnnotations;
import org.apache.harmony.vm.VMStack;

/**
 * @com.intel.drl.spec_ref
 *
 * @author Evgueni Brevnov, Serguei S. Zapreyev, Alexey V. Varlamov
 * @version $Revision: 1.1.2.2.4.5 $
 */
//public final class Class implements Serializable {

public final class Class<T> implements Serializable, AnnotatedElement, GenericDeclaration, Type {

    private static final long serialVersionUID = 3206093459760846163L;

    static ProtectionDomain systemDomain;

    private transient ProtectionDomain domain;

    /** It is required for synchronization in newInstance method. */
    private boolean isDefaultConstructorInitialized;

    // TODO make it soft reference
    transient ReflectionData reflectionData;
    transient SoftReference<GACache> softCache;
    
    /** 
     * Indicates where the following properties have been calculated;
     * isSerializable, isExternalizable, isPrimitive
     * 
     * @see #resolveProperties() 
     */
    private transient volatile boolean arePropertiesResolved;
    
    private transient boolean isSerializable;
    private transient boolean isExternalizable;
    private transient boolean isPrimitive;
    
    private GACache getCache() {
        GACache cache = null;
        if (softCache != null) { 
            cache = softCache.get();
        }
        if (cache == null) {
            softCache = new SoftReference<GACache>(cache = new GACache());
        }
        return cache;
    }

    /**
     * Only VM can instantiate this class.
     */
    private Class() {
    }

    /**
     * @com.intel.drl.spec_ref
     */
    public static Class<?> forName(String name) throws ClassNotFoundException {
        return forName(name, true, VMClassRegistry.getClassLoader(VMStack
            .getCallerClass(0)));
    }

    /**
     * @com.intel.drl.spec_ref
     */
    public static Class<?> forName(String name, boolean initialize,
            ClassLoader classLoader) throws ClassNotFoundException {
        if (name == null) {
            throw new NullPointerException();
        }
        if(name.indexOf("/") != -1) {
            throw new ClassNotFoundException(name);
        }

        Class clazz = null;

        if (classLoader == null) {
            SecurityManager sc = System.getSecurityManager();
            if (sc != null &&
                VMClassRegistry.getClassLoader(VMStack.getCallerClass(0)) != null) {
                sc.checkPermission(RuntimePermissionCollection.GET_CLASS_LOADER_PERMISSION);
            }
            clazz = VMClassRegistry.loadBootstrapClass(name);
        } else {
            int dims = 0;
            int len = name.length();
            while (dims < len && name.charAt(dims) == '[') dims++;
            if (dims > 0 && len > dims + 1 
                    && name.charAt(dims) == 'L' && name.endsWith(";")) {
                /*
                 * an array of a reference type is requested.
                 * do not care of arrays of primitives as 
                 * they are perfectly loaded by bootstrap classloader. 
                 */
                try {
                    clazz = classLoader.loadClass(name.substring(dims + 1, len - 1));
                } catch (ClassNotFoundException ignore) {}
                if (clazz != null ) {
                    clazz = VMClassRegistry.loadArray(clazz, dims);
                }
            } else {
                clazz = classLoader.loadClass(name);
            }
        }
        if(clazz == null) {
            throw new ClassNotFoundException(name);
        }
        if(classLoader != null) {
            // Although class loader may have had a chance to register
            // itself as initiating for requested class, there may occur
            // a classloader which overloads loadClass method (though it is
            // not recommended by J2SE specification).
            // Try to register initiating loader for clazz from here again
            classLoader.registerInitiatedClass(clazz);
        }
        if (initialize) {
            VMClassRegistry.initializeClass(clazz);
        } else {
            VMClassRegistry.linkClass(clazz);
        }
        return clazz;
    }

    /**
     * package private to access from the java.lang.ClassLoader class.
     */
    static volatile boolean disableAssertions = 
        VMExecutionEngine.getAssertionStatus(null, false, 0) <= 0;

    /**
     * @com.intel.drl.spec_ref 
     */
    public boolean desiredAssertionStatus() {
        if (disableAssertions) {
            return false;
        }
        
        ClassLoader loader = getClassLoaderImpl();
        if (loader == null) {
            // system class, status is controlled via cmdline only
            return VMExecutionEngine.getAssertionStatus(this, true, 0) > 0;
        } 

        // First check exact class name 
        String name = null;
        Map<String, Boolean> m = loader.classAssertionStatus;  
        if (m != null && m.size() != 0) 
        {
            name = getTopLevelClassName();
            Boolean status = m.get(name);
            if (status != null) {
                return status.booleanValue();
            }
        }
        if (!loader.clearAssertionStatus) {
            int systemStatus = VMExecutionEngine.getAssertionStatus(this, false, 0);
            if (systemStatus != 0) {
                return systemStatus > 0;
            }
        }
        
        // Next try (super)packages name(s) recursively
        m = loader.packageAssertionStatus;
        if (m != null && m.size() != 0) {
            if (name == null) {
                name = getName();
            }
            name = getParentName(name);
            // if this class is in the default package, 
            // it is checked in the 1st iteration
            do {
                Boolean status = m.get(name);
                if (status != null) {
                    return status.booleanValue();
                }
            } while ( (name = getParentName(name)).length() > 0);
        }
        if (!loader.clearAssertionStatus) {
            int systemStatus = VMExecutionEngine.getAssertionStatus(this, true, 
                    loader.defaultAssertionStatus);
            if (systemStatus != 0) {
                return systemStatus > 0;
            }
        }
        
        // Finally check the default status
        return loader.defaultAssertionStatus > 0;
    }

    /**
     * @com.intel.drl.spec_ref
     *
     * Note: We don't check member access permission for each super class.
     * Java 1.5 API specification doesn't require this check.
     */
    public Class[] getClasses() {
        if (reflectionData == null) {
            initReflectionData();
        }
        checkMemberAccess(Member.PUBLIC);
        Class clss = this;
        ArrayList<Class> classes = null;
        while (clss != null) {
            Class[] declared = VMClassRegistry.getDeclaredClasses(clss);
            if (declared.length != 0) {
                if (classes == null) {
                    classes = new ArrayList<Class>();
                }
                for (Class c : declared) {
                    if (Modifier.isPublic(c.getModifiers())) {
                        classes.add(c);
                    }
                }
            }
            clss = clss.getSuperclass();
        }
        if (classes == null) {
            return new Class[0];
        } else {
            return classes.toArray(new Class[classes.size()]);
        }
    }

    /**
     * @com.intel.drl.spec_ref
     */
    public ClassLoader getClassLoader() {
        ClassLoader loader = getClassLoaderImpl();
        SecurityManager sc = System.getSecurityManager();
        if (sc != null) {
            ClassLoader callerLoader = VMClassRegistry.getClassLoader(VMStack
                .getCallerClass(0));
            if (callerLoader != null && !callerLoader.isSameOrAncestor(loader)) {
                sc.checkPermission(RuntimePermissionCollection.GET_CLASS_LOADER_PERMISSION);
            }
        }
        return loader;
    }

    /**
     * @com.intel.drl.spec_ref
     */
    public Class<?> getComponentType() {
		if(!isArray()) return null;
        return VMClassRegistry.getComponentType(this);
    }

    /**
     * @com.intel.drl.spec_ref
     */
    public Constructor<T> getConstructor(Class... argumentTypes)
        throws NoSuchMethodException {
        if (reflectionData == null) {
            initReflectionData();
        }
        checkMemberAccess(Member.PUBLIC);
        Constructor<T> ctors[] = reflectionData.getPublicConstructors(); 
        for (int i = 0; i < ctors.length; i++) {
            Constructor<T> c = ctors[i];
            try {
                if (isTypeMatches(argumentTypes, c.getParameterTypes())) {
                    return Reflection.copyConstructor(c);
                }
            } catch (LinkageError ignore) {}
        }
        throw new NoSuchMethodException(getName()
            + printMethodSignature(argumentTypes));

    }

    /**
     * @com.intel.drl.spec_ref
     */
    public Constructor[] getConstructors() {
        if (reflectionData == null) {
            initReflectionData();
        }
        checkMemberAccess(Member.PUBLIC);
        return Reflection.copyConstructors(reflectionData.getPublicConstructors());
    }

    /**
     * @com.intel.drl.spec_ref
     */
    public Class[] getDeclaredClasses() {
        if (reflectionData == null) {
            initReflectionData();
        }
        checkMemberAccess(Member.DECLARED);
        return VMClassRegistry.getDeclaredClasses(this);
    }

    /**
     * @com.intel.drl.spec_ref
     */
    public Constructor<T> getDeclaredConstructor(Class... argumentTypes)
        throws NoSuchMethodException {
        if (reflectionData == null) {
            initReflectionData();
        }
        checkMemberAccess(Member.DECLARED);
        return Reflection
            .copyConstructor(getDeclaredConstructorInternal(argumentTypes));
    }

    /**
     * @com.intel.drl.spec_ref
     */
    public Constructor[] getDeclaredConstructors() {
        if (reflectionData == null) {
            initReflectionData();
        }
        checkMemberAccess(Member.DECLARED);
        if (reflectionData.declaredConstructors == null) {
            reflectionData.initDeclaredConstructors();
        }
        return Reflection.copyConstructors(reflectionData.declaredConstructors);
    }

    /**
     * @com.intel.drl.spec_ref
     */
    public Field getDeclaredField(String fieldName) throws NoSuchFieldException {
        if (reflectionData == null) {
            initReflectionData();
        }
        checkMemberAccess(Member.DECLARED);
        if (reflectionData.declaredFields == null) {
            reflectionData.initDeclaredFields();
        }
        for (int i = 0; i < reflectionData.declaredFields.length; i++) {
            Field f = reflectionData.declaredFields[i];
            if (fieldName.equals(f.getName())) {
                return Reflection.copyField(f);
            }
        }
        throw new NoSuchFieldException(fieldName.toString());
    }

    /**
     * @com.intel.drl.spec_ref
     */
    public Field[] getDeclaredFields() {
        if (reflectionData == null) {
            initReflectionData();
        }
        checkMemberAccess(Member.DECLARED);
        if (reflectionData.declaredFields == null) {
            reflectionData.initDeclaredFields();
        }
        return Reflection.copyFields(reflectionData.declaredFields);
    }

    /**
     * @com.intel.drl.spec_ref
     */
    public Method getDeclaredMethod(String methodName, Class... argumentTypes)
        throws NoSuchMethodException {
        if (reflectionData == null) {
            initReflectionData();
        }
        checkMemberAccess(Member.DECLARED);
        if (reflectionData.declaredMethods == null) {
            reflectionData.initDeclaredMethods();
        }
        return Reflection
            .copyMethod(findMatchingMethod(reflectionData.declaredMethods,
                                           methodName, argumentTypes));
    }

    /**
     * @com.intel.drl.spec_ref
     */
    public Method[] getDeclaredMethods() {
        if (reflectionData == null) {
            initReflectionData();
        }
        checkMemberAccess(Member.DECLARED);
        if (reflectionData.declaredMethods == null) {
            reflectionData.initDeclaredMethods();
        }
        return Reflection.copyMethods(reflectionData.declaredMethods);
    }

    /**
     * @com.intel.drl.spec_ref
     */
    public Class<?> getDeclaringClass() {
        return VMClassRegistry.getDeclaringClass(this);
    }

    /**
     * @com.intel.drl.spec_ref
     */
    public Field getField(String fieldName) throws NoSuchFieldException {
        if (reflectionData == null) {
            initReflectionData();
        }
        checkMemberAccess(Member.PUBLIC);
        Field[] ff = reflectionData.getPublicFields();
        for (Field f : ff) {
            if (fieldName.equals(f.getName())) {
                return Reflection.copyField(f);
            }
        }
        throw new NoSuchFieldException(fieldName.toString());
    }

    /**
     * @com.intel.drl.spec_ref
     */
    public Field[] getFields() {
        if (reflectionData == null) {
            initReflectionData();
        }
        checkMemberAccess(Member.PUBLIC);
        return Reflection.copyFields(reflectionData.getPublicFields());
    }

    /**
     * @com.intel.drl.spec_ref
     */
    public Class[] getInterfaces() {
        return VMClassRegistry.getInterfaces(this);
    }

    /**
     * @com.intel.drl.spec_ref
     */
    public Method getMethod(String methodName, Class... argumentTypes)
        throws NoSuchMethodException {
        if (reflectionData == null) {
            initReflectionData();
        }
        checkMemberAccess(Member.PUBLIC);
        return Reflection
            .copyMethod(findMatchingMethod(reflectionData.getPublicMethods(),
                                           methodName, argumentTypes));
    }

    /**
     * @com.intel.drl.spec_ref
     */
    public Method[] getMethods() {
        if (reflectionData == null) {
            initReflectionData();
        }
        checkMemberAccess(Member.PUBLIC);
        return Reflection.copyMethods(reflectionData.getPublicMethods());
    }

    /**
     * @com.intel.drl.spec_ref
     */
    public int getModifiers() {
        if (reflectionData == null) {
            initReflectionData();
        }
        int mods = reflectionData.modifiers;
        if (mods == -1) {
            mods = reflectionData.modifiers = VMClassRegistry.getModifiers(this); 
        }
        return mods;
    }

    /**
     * @com.intel.drl.spec_ref
     */
    public String getName() {
        if (reflectionData == null) {
            initReflectionData();
        }
        return reflectionData.name;
    }

    /**
     * @com.intel.drl.spec_ref
     */
    public Package getPackage() {
        ClassLoader classLoader = getClassLoaderImpl();
        return classLoader == null
            ? ClassLoader.BootstrapLoader.getPackage(getPackageName())
            : classLoader.getPackage(getPackageName());
    }

    /**
     * @com.intel.drl.spec_ref
     */
    public ProtectionDomain getProtectionDomain() {
        SecurityManager sc = System.getSecurityManager();
        if (sc != null) {
            sc.checkPermission(RuntimePermissionCollection.GET_PROTECTION_DOMAIN_PERMISSION);
        }
        if (domain == null) {
        	if (systemDomain == null) {
        	        Permissions allPermissions = new Permissions();
        	        allPermissions.add(new AllPermission());
        	        systemDomain = new ProtectionDomain(null, allPermissions);
        	}
        	return systemDomain;
        }
        return domain;
    }

    /**
     * @com.intel.drl.spec_ref
     */
    public URL getResource(String resource) {
        resource = getAbsoluteResource(resource);
        ClassLoader classLoader = getClassLoaderImpl();
        return classLoader == null
            ? ClassLoader.getSystemResource(resource)
            : classLoader.getResource(resource);
    }

    /**
     * @com.intel.drl.spec_ref
     */
    public InputStream getResourceAsStream(String resource) {
        resource = getAbsoluteResource(resource);
        ClassLoader classLoader = getClassLoaderImpl();
        return classLoader == null
            ? ClassLoader.getSystemResourceAsStream(resource)
            : classLoader.getResourceAsStream(resource);
    }

    /**
     * @com.intel.drl.spec_ref
     */
    public Object[] getSigners() {
        try {
            Object[] signers = (Object[])getClassLoaderImpl().classSigners.get(getName());
            return (Object[])signers.clone();
       } catch (NullPointerException e) {
        }
        try {
            return (Object[])domain.getCodeSource().getCertificates().clone();
        } catch (NullPointerException e) {
        }
        return null;
    }

    /**
     * @com.intel.drl.spec_ref
     */
    public Class<? super T> getSuperclass() {
        return VMClassRegistry.getSuperclass(this);
    }

    /**
     * @com.intel.drl.spec_ref
     */
    public boolean isArray() {
        if (reflectionData == null) {
            initReflectionData();
        }
        return reflectionData.isArray;
    }

    private void resolveProperties() {
        if (arePropertiesResolved) {
        	return;
        }
        isExternalizable = VMClassRegistry.isAssignableFrom(Externalizable.class, this);
        isSerializable = VMClassRegistry.isAssignableFrom(Serializable.class, this);
        isPrimitive = VMClassRegistry.isPrimitive(this);
        arePropertiesResolved = true;
    }
    
    /**
     * @com.intel.drl.spec_ref
     */
    public boolean isAssignableFrom(Class<?> clazz) {
        
        if (Serializable.class.equals(this)) {
            // assure that props have been resolved
        	clazz.resolveProperties();
            return clazz.isSerializable;
        }
        
        if (Externalizable.class.equals(this)) {
            // assure that props have been resolved
        	clazz.resolveProperties();
            return clazz.isExternalizable;
        }
        
        return VMClassRegistry.isAssignableFrom(this, clazz);
    }

    /**
     * @com.intel.drl.spec_ref
     */
    public boolean isInstance(Object obj) {
        return VMClassRegistry.isInstance(this, obj);
    }

    /**
     * @com.intel.drl.spec_ref
     */
    public boolean isInterface() {
        return (getModifiers() & ACC_INTERFACE) != 0;
    }

    /**
     * @com.intel.drl.spec_ref
     */
    public boolean isPrimitive() {
    	// assure that props have been resolved
        resolveProperties();
        return isPrimitive;
    }

    /**
     * @com.intel.drl.spec_ref
     */
    public T newInstance() throws InstantiationException,
        IllegalAccessException {
        T newInstance = null;
        if (reflectionData == null) {
            initReflectionData();
        }
        SecurityManager sc = System.getSecurityManager();
        if (sc != null) {
            sc.checkMemberAccess(this, Member.PUBLIC);
            sc.checkPackageAccess(reflectionData.packageName);
        }

        /*
         * HARMONY-1930: The synchronization issue is possible here.
         *
         * The issues is caused by fact that:
         * - first thread starts defaultConstructor initialization, including
         *   setting "isAccessible" flag to "true" for Constrcutor object
         * - another thread bypasses initialization and calls "newInstance" 
         *   for defaultConstructor (while isAccessible is "false" yet)
         * - so, for this "another" thread the Constructor.newInstance checks
         *   the access rights by mistake and IllegalAccessException happens
         */
        while (!isDefaultConstructorInitialized) {
            synchronized (reflectionData) {
                if (isDefaultConstructorInitialized) {
                    break; // non-first threads can be here - nothing to do
                }

                // only first thread can reach this point & do initialization
                try {
                    reflectionData.initDefaultConstructor();
                } catch (NoSuchMethodException e) {
                    throw new InstantiationException(e.getMessage()
                            + " method not found");
                }
                final Constructor<T> c = reflectionData.defaultConstructor;

                try {
                    AccessController.doPrivileged(new PrivilegedAction<Object>() {
                            public Object run() {
                            c.setAccessible(true);
                            return null;
                            }
                            });
                } catch (SecurityException e) {
                    // can't change accessibilty of the default constructor
                    IllegalAccessException ex = new IllegalAccessException();
                    ex.initCause(e);
                    throw ex;
                }

                // default constructor is initialized, access flag is set
                isDefaultConstructorInitialized = true;
                break;
            }
        }

        // initialization is done, threads may work from here in any order
        Reflection.checkMemberAccess(
                VMStack.getCallerClass(0),
                reflectionData.defaultConstructor.getDeclaringClass(),
                reflectionData.defaultConstructor.getDeclaringClass(),
                reflectionData.defaultConstructor.getModifiers()
            );

        try {
            newInstance = reflectionData.defaultConstructor.newInstance();
        } catch (InvocationTargetException e) {
            System.rethrow(e.getCause());
        }
        return newInstance;
    }

    /**
     * @com.intel.drl.spec_ref
     */
    public String toString() {
        return isPrimitive() ? getName()
            : (isInterface() ? "interface " : "class ") + getName();
    }

    /*
     * NON API SECTION
     */

    /**
     *  Answers whether the arrays are equal
     */
    static boolean isTypeMatches(Class[] t1, Class[] t2) {
        if (t1 == null) {
            return t2 == null || t2.length == 0;
        }
        if (t2 == null) {
            return t1 == null || t1.length == 0;
        }
        if (t1.length != t2.length) {
            return false;
        }
        for (int i = 0; i < t2.length; i++) {
            if (t1[i] != t2[i]) {
                return false;
            }
        }
        return true;
    }

    /**
     * This is not time consume operation so we can do syncrhronization on this
     * class object. Note, this method has package private visibility in order
     * to increase performance.
     */
    synchronized void initReflectionData() {
        if (reflectionData == null) {
            reflectionData = new ReflectionData();
        }
    }


    String getPackageName() {
        if (reflectionData == null) {
            initReflectionData();
        }
        return reflectionData.packageName;
    }

    void setProtectionDomain(ProtectionDomain protectionDomain) {
        domain = protectionDomain;
    }

    static private Method findMatchingMethod(Method[] methods, String methodName,
                                      Class[] argumentTypes)
        throws NoSuchMethodException {
        Method matcher = null;
        for (int i = 0; i < methods.length; i++) {
            Method m = methods[i];
            if (matcher != null && matcher.getDeclaringClass() != m
                .getDeclaringClass()) {
                return matcher;
            }
            try {
                if (methodName.equals(m.getName())
                    && isTypeMatches(argumentTypes, m.getParameterTypes())
                    && (matcher == null || matcher.getReturnType()
                        .isAssignableFrom(m.getReturnType()))) {
                    matcher = m;
                }
            } catch (LinkageError ignore) {}
        }
        if (matcher == null) {
            throw new NoSuchMethodException(methodName.toString()
                + printMethodSignature(argumentTypes));
        }
        return matcher;
    }

    static private String getParentName(String name) {
        int dotPosition = name.lastIndexOf('.');
        return dotPosition == -1 ? "" : name.substring(0, dotPosition);
    }

    static private String printMethodSignature(Class[] types) {
        StringBuffer sb = new StringBuffer("(");
        if (types != null && types.length > 0) {
            sb.append(types[0] != null ? types[0].getName() : "null");
            for (int i = 1; i < types.length; i++) {
                sb.append(", ");
                sb.append(types[i] != null ? types[i].getName() : "null");
            }
        }
        sb.append(")");
        return sb.toString();
    }

    private void checkMemberAccess(int accessType) {
        SecurityManager sc = System.getSecurityManager();
        if (sc != null) {
            sc.checkMemberAccess(this, accessType);
            sc.checkPackageAccess(reflectionData.packageName);
        }
    }

    private String getAbsoluteResource(String resource) {
        if (resource.startsWith("/")) {
            return resource.substring(1);
        }
        String pkgName = getPackageName();
        if (pkgName.length() > 0) {
            resource = pkgName.replace('.', '/') + '/' + resource;
        }
        return  resource;
    }

    private Constructor<T> getDeclaredConstructorInternal(Class[] argumentTypes)
        throws NoSuchMethodException {
        if (reflectionData.declaredConstructors == null) {
            reflectionData.initDeclaredConstructors();
        }
        for (int i = 0; i < reflectionData.declaredConstructors.length; i++) {
            Constructor<T> c = reflectionData.declaredConstructors[i];
            if (isTypeMatches(argumentTypes, c.getParameterTypes())) {
                return c;
            }
        }
        throw new NoSuchMethodException(getName()
            + printMethodSignature(argumentTypes));
    }

    private String getTopLevelClassName() {
        Class declaringClass = getDeclaringClass();
        return declaringClass == null
            ? getName() : declaringClass.getTopLevelClassName();
    }


    /* VMI SPECIFIC PART*/

    final ClassLoader getClassLoaderImpl() {
        assert(VMClassRegistry.getClassLoader0(this) == definingLoader);
        return definingLoader;
    }

    static final Class[] getStackClasses(int maxDepth, 
            boolean stopAtPrivileged) {
        return VMStack.getClasses(maxDepth, stopAtPrivileged);
    }

    /* END OF VMI SPECIFIC PART */

    /**
     *
     *  @com.intel.drl.spec_ref
     * 
     **/
    public Annotation[] getDeclaredAnnotations() {
        Annotation[] declared = getCache().getDeclaredAnnotations();  
        Annotation aa[] = new Annotation[declared.length];
        System.arraycopy(declared, 0, aa, 0, declared.length);
        return aa;
    }

    /**
     *
     *  @com.intel.drl.spec_ref
     * 
     **/
    public Annotation[] getAnnotations() {
        Annotation[] all = getCache().getAllAnnotations();
        Annotation aa[] = new Annotation[all.length];
        System.arraycopy(all, 0, aa, 0, all.length);
        return aa;
    }

    /**
     *
     *  @com.intel.drl.spec_ref
     * 
     **/
    @SuppressWarnings("unchecked")
    public <A extends Annotation> A getAnnotation(Class<A> annotationClass) {
        if(annotationClass == null) {
            throw new NullPointerException();
        }
        for (Annotation aa : getCache().getAllAnnotations()) {
            if(annotationClass == aa.annotationType()) {
                return (A)aa;
            }
        }
        return null;
    }

    /**
     *
     *  @com.intel.drl.spec_ref
     * 
     **/
    public boolean isAnnotationPresent(Class<? extends Annotation> annotationClass) {
        if(annotationClass == null) {
            throw new NullPointerException();
        }
        for (Annotation aa : getCache().getAllAnnotations()) {
            if(annotationClass == aa.annotationType()) {
                return true;
            }
        }
        return false;
    }

    /**
     *
     *  @com.intel.drl.spec_ref
     * 
     **/
    @SuppressWarnings("unchecked")
    public T[] getEnumConstants() {
        if (isEnum()) {
            try {
                final Method values = getMethod("values");
                AccessController.doPrivileged(new PrivilegedAction() {
                    public Object run() {
                        values.setAccessible(true);
                        return null;
                    }
                });
                return (T[]) values.invoke(null);
            } catch (Exception ignore) {}
        }
        return null;
    }

    /**
     *
     *  @com.intel.drl.spec_ref
     * 
     **/
    public boolean isEnum() {
        // check for superclass is needed for compatibility
        // otherwise there are false positives on anonymous element classes
        return ((getModifiers() & ACC_ENUM) != 0 && getSuperclass() == Enum.class);
    }

    /**
     *
     *  @com.intel.drl.spec_ref
     * 
     **/
    public boolean isAnnotation() {
        return (getModifiers() & ACC_ANNOTATION) != 0;
    }

    /**
     *
     *  @com.intel.drl.spec_ref
     * 
     **/
    @SuppressWarnings("unchecked")
    public <U> Class<? extends U> asSubclass(Class<U> clazz) throws ClassCastException {
        if (!VMClassRegistry.isAssignableFrom(clazz, this)) {
            throw new ClassCastException(toString());
        }

        return (Class<? extends U>)this;
    }

    /**
     *
     *  @com.intel.drl.spec_ref
     * 
     **/
    @SuppressWarnings("unchecked")
    public T cast(Object obj) throws ClassCastException {
        if (obj != null && !VMClassRegistry.isInstance(this, obj)) {
            throw new ClassCastException(obj.getClass().toString());
        }
        return (T) obj;
    }

    /**
     *
     *  @com.intel.drl.spec_ref
     * 
     **/
    public TypeVariable<Class<T>>[] getTypeParameters() throws GenericSignatureFormatError {
        return (TypeVariable<Class<T>>[])getCache().getTypeParameters().clone();
    }

    /**
     *
     *  @com.intel.drl.spec_ref
     * 
     **/
    public Method getEnclosingMethod() {
        Member m = VMClassRegistry.getEnclosingMember(this); // see VMClassRegistry.getEnclosingMember() spec
        return m instanceof Method? (Method)m : null;
    }

    /**
     *
     *  @com.intel.drl.spec_ref
     * 
     **/
    public Constructor<?> getEnclosingConstructor() {
        Member m = VMClassRegistry.getEnclosingMember(this); // see VMClassRegistry.getEnclosingMember() spec
        return m instanceof Constructor ? (Constructor<?>)m : null;
    }

    /**
     *
     *  @com.intel.drl.spec_ref
     * 
     **/
    public Type[] getGenericInterfaces() throws GenericSignatureFormatError, TypeNotPresentException, MalformedParameterizedTypeException {
        if (isArray()) {
            return new Type[]{Cloneable.class, Serializable.class};
        }
        if (isPrimitive()) {
            return new Type[0];
        }
        
        return (Type[])getCache().getGenericInterfaces().clone();
    }

    /**
     *
     *  @com.intel.drl.spec_ref
     * 
     **/
    public Type getGenericSuperclass() throws GenericSignatureFormatError, TypeNotPresentException, MalformedParameterizedTypeException  {
        String tmp;
        if (isInterface() || ((tmp = getCanonicalName()) != null && tmp.equals("java.lang.Object")) || isPrimitive()) {
            return null;
        }
        if (isArray()) {
            return (Type) Object.class;
        }
        
        Class clazz = getSuperclass();
        if (clazz.getTypeParameters().length == 0) {
            return (Type) clazz;
        }
        
        return getCache().getGenericSuperclass();
    }

    /**
     *
     *  @com.intel.drl.spec_ref
     * 
     **/
    public Class<?> getEnclosingClass() {
        return VMClassRegistry.getEnclosingClass(this); // see VMClassRegistry.getEnclosingClass() spec
    }

    /**
     *
     *  @com.intel.drl.spec_ref
     * 
     **/
    public boolean isMemberClass() {
        return getDeclaringClass() != null; // see Class.getDeclaringClass() spec
    }

    /**
     *
     *  @com.intel.drl.spec_ref
     * 
     **/
    public boolean isLocalClass() {
        return VMClassRegistry.getEnclosingMember(this) != null && !isAnonymousClass(); // see CFF spec, #4.8.6, first paragraph and VMClassRegistry.getEnclosingMember() spec
    }
    
    /**
     *
     *  @com.intel.drl.spec_ref
     * 
     **/
    public boolean isAnonymousClass() {
        return getSimpleName().length() == 0;
    }
    
    /**
     *
     *  @com.intel.drl.spec_ref
     * 
     **/
    public boolean isSynthetic() {
        return (getModifiers() & ACC_SYNTHETIC) != 0;
    }

    /**
     *
     *  @com.intel.drl.spec_ref
     * 
     **/
    public String getCanonicalName() {
        if (isLocalClass() || isAnonymousClass()) {
            return null;
        }
        if (isArray()) {
            String res = getComponentType().getCanonicalName();
            return res != null ? res + "[]" : null;
        }
        
        StringBuffer sb = new StringBuffer(getPackageName());
        ArrayList<String> sympleNames = new ArrayList<String>();
        Class clss = this;
        while ((clss = clss.getDeclaringClass()) != null) {
            if (clss.isLocalClass() || clss.isAnonymousClass()) {
                return null;
            }
            sympleNames.add(clss.getSimpleName());
        }
        if (sb.length() > 0) {
            sb.append(".");
        }
        for (int i = sympleNames.size() - 1; i > -1 ; i--) {
            sb.append(sympleNames.get(i)).append(".");
        }
        sb.append(getSimpleName());

        return sb.toString();
    }

    /**
     *
     *  @com.intel.drl.spec_ref
     * 
     **/
    public String getSimpleName() {
//      TODO: the method result should be reusible
        return VMClassRegistry.getSimpleName(this);
    }

    /**
     * Provides strong referencing between the classloader 
     * and it's defined classes. Intended for class unloading implementation.
     * @see java.lang.ClassLoader#loadedClasses
     */
    ClassLoader definingLoader;

    private final class ReflectionData {
        
        String name;
        
        int modifiers = -1;
        
        boolean isArray;

        Constructor<T>[] declaredConstructors;

        Field[] declaredFields;

        Method[] declaredMethods;

        Constructor<T> defaultConstructor;
        
        String packageName;

        Constructor<T>[] publicConstructors;

        Field[] publicFields;

        Method[] publicMethods;
        
        public ReflectionData() {
            name = VMClassRegistry.getName(Class.this);
            isArray = VMClassRegistry.isArray(Class.this);
            packageName = Class.getParentName(name);
        }
        
        public void initDeclaredConstructors() {
            if (declaredConstructors == null) {
                declaredConstructors = VMClassRegistry
                    .getDeclaredConstructors(Class.this);
            }
        }

        public void initDeclaredFields() {
            if (declaredFields == null) {
                declaredFields = VMClassRegistry
                    .getDeclaredFields(Class.this);
            }
        }

        public void initDeclaredMethods() {
            if (declaredMethods == null) {
                declaredMethods = VMClassRegistry
                    .getDeclaredMethods(Class.this);
            }
        }

        public void initDefaultConstructor()
            throws NoSuchMethodException {
            if (defaultConstructor == null) {
                defaultConstructor = Class.this
                    .getDeclaredConstructorInternal(null);
            }
        }

        @SuppressWarnings("unchecked")
        public synchronized Constructor<T>[] getPublicConstructors() {
            if (publicConstructors != null) {
                return publicConstructors;
            }
            if (declaredConstructors == null) {
                initDeclaredConstructors();
            }
            ArrayList<Constructor<T>> constructors = 
                new ArrayList<Constructor<T>>(declaredConstructors.length);
            for (int i = 0; i < declaredConstructors.length; i++) {
                Constructor<T> c = declaredConstructors[i];
                if (Modifier.isPublic(c.getModifiers())) {
                    constructors.add(c);
                }
            }
            return publicConstructors = constructors.toArray(
                    new Constructor[constructors.size()]);
        }

        /**
         * Stores public fields in order they should be searched by
         * getField(name) method.
         */
        public synchronized Field[] getPublicFields() {
            if (publicFields != null) {
                return publicFields;
            }
            if (declaredFields == null) {
                initDeclaredFields();
            }

            // initialize public fields of the super class
            int size = declaredFields.length;
            Class superClass = Class.this.getSuperclass();
            Field[] superFields = null;
            if (superClass != null) {
                if (superClass.reflectionData == null) {
                    superClass.initReflectionData();
                }
                superFields = superClass.reflectionData.getPublicFields();
                size += superFields.length;
            }

            // add public fields of this class 
            Collection<Field> fields = new LinkedHashSet<Field>(size);
            for (Field f : declaredFields) {
                if (Modifier.isPublic(f.getModifiers())) {
                    fields.add(f);
                }
            }
            
            // initialize and add fields of the super interfaces
            Class[] interfaces = Class.this.getInterfaces();
            for (Class ci : interfaces) {
                if (ci.reflectionData == null) {
                    ci.initReflectionData();
                }
                Field[] fi = ci.reflectionData.getPublicFields();
                for (Field f : fi) {
                    fields.add(f);
                }
            }        
            
            // add public fields of the super class
            if (superFields != null) {
                for (Field f : superFields) {
                    if (Modifier.isPublic(f.getModifiers())) {
                        fields.add(f);
                    }
                }
            }
            
            // maybe publicFields better be set atomically 
            // instead of access synchronization? 
            return publicFields = fields.toArray(new Field[fields.size()]);
        }

        public synchronized Method[] getPublicMethods() {
            if (publicMethods != null) {
                return publicMethods;
            }
            if (declaredMethods == null) {
                initDeclaredMethods();
            }
            
            // initialize public methods of the super class
            int size = declaredMethods.length;
            Class superClass = Class.this.getSuperclass();
            Method[] superPublic = null;
            if (superClass != null) {
                if (superClass.reflectionData == null) {
                    superClass.initReflectionData();
                }
                superPublic = superClass.reflectionData.getPublicMethods(); 
                size += superPublic.length;
            }

            // add methods of the super interfaces
            Class[] interfaces = Class.this.getInterfaces();
            Method[][] intf = null;
            if (interfaces.length != 0) {
                intf = new Method[interfaces.length][];
                for (int i = 0; i < interfaces.length; i++) {
                    Class ci = interfaces[i];
                    if (ci.reflectionData == null) {
                        ci.initReflectionData();
                    }
                    intf[i] = ci.reflectionData.getPublicMethods();
                    size += intf[i].length; 
                }
            }
            // maybe publicMethods better be set atomically 
            // instead of access synchronization? 
            return publicMethods = Reflection.mergePublicMethods(declaredMethods, superPublic, intf, size);
        }        
    }

    private final class GACache {
    
        private Annotation[] allAnnotations;
        private Annotation[] declaredAnnotations;
        private Type[] genericInterfaces;
        private Type genericSuperclass;
        private TypeVariable<Class<T>>[] typeParameters;
        
        public synchronized Annotation[] getAllAnnotations() {
            if (allAnnotations != null) {
                return allAnnotations;
            }
            if (declaredAnnotations == null) {
                declaredAnnotations = VMGenericsAndAnnotations
                .getDeclaredAnnotations(Class.this);
            }
            
            // look for inherited annotations
            Class superClass = Class.this.getSuperclass();
            if (superClass != null) {
                Annotation[] sa = superClass.getCache().getAllAnnotations();
                if (sa.length != 0) {
                    final int size = declaredAnnotations.length;
                    Annotation[] all = new Annotation[size + sa.length];
                    System.arraycopy(declaredAnnotations, 0, all, 0, size);
                    int pos = size;
                    next: for (Annotation s : sa) {
                        if (s.annotationType().isAnnotationPresent(Inherited.class)) {
                            for (int i = 0; i < size; i++) {
                                if (all[i].annotationType() == s.annotationType()) {
                                    // overriden by declared annotation
                                    continue next;
                                }
                            }
                            all[pos++] = s;
                        }
                    }
                    allAnnotations = new Annotation[pos];
                    System.arraycopy(all, 0, allAnnotations, 0, pos);
                    return allAnnotations;
                }
            }
            return allAnnotations = declaredAnnotations;
        }

        public Annotation[] getDeclaredAnnotations() {
            if (declaredAnnotations == null) {
                  declaredAnnotations = VMGenericsAndAnnotations
                      .getDeclaredAnnotations(Class.this);
            }
            return declaredAnnotations;
        }
        
        public synchronized Type[] getGenericInterfaces() {
            if (genericInterfaces == null) {
                genericInterfaces = Parser.getGenericInterfaces(Class.this, VMGenericsAndAnnotations.getSignature(Class.this));
            }
            return genericInterfaces;
        }

        public Type getGenericSuperclass() {
            //So, here it can be only ParameterizedType or ordinary reference class type
            if (genericSuperclass == null) {
                genericSuperclass = Parser.getGenericSuperClass(Class.this, VMGenericsAndAnnotations.getSignature(Class.this));
            }
            return genericSuperclass;
        }

        @SuppressWarnings("unchecked")
        public synchronized TypeVariable<Class<T>>[] getTypeParameters() {
            if(typeParameters == null){
                typeParameters = Parser.getTypeParameters(Class.this,
                        VMGenericsAndAnnotations.getSignature(Class.this));
            }
            return typeParameters;
        }
    }
}
