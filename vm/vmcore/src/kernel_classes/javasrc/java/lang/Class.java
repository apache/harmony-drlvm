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

import java.io.InputStream;
import java.io.Serializable;
import java.lang.reflect.Constructor;
import java.lang.reflect.Field;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Member;
import java.lang.reflect.Method;
import java.lang.reflect.Modifier;

import java.lang.reflect.AnnotatedElement;
import java.lang.reflect.GenericDeclaration;
import java.lang.reflect.Type;
import java.lang.reflect.TypeVariable;
import java.lang.annotation.Annotation;

import java.net.URL;
import java.security.AccessController;
import java.security.AllPermission;
import java.security.Permissions;
import java.security.PrivilegedAction;
import java.security.ProtectionDomain;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashSet;
import java.util.LinkedHashSet;

import org.apache.harmony.lang.RuntimePermissionCollection;
import org.apache.harmony.lang.reflect.Reflection;
import org.apache.harmony.vm.VMStack;

/**
 * @com.intel.drl.spec_ref
 *
 * @author Evgueni Brevnov
 * @version $Revision: 1.1.2.2.4.5 $
 */
//public final class Class implements Serializable {

public final class Class<T> implements Serializable, AnnotatedElement, GenericDeclaration, Type {

    private static final long serialVersionUID = 3206093459760846163L;

    static ProtectionDomain systemDomain;

    private transient ProtectionDomain domain;

    // TODO make it soft reference
    transient ReflectionData reflectionData;

    /**
     * Only VM can instantiate this class.
     */
    private Class() {
    }

    /**
     * @com.intel.drl.spec_ref
     */
    public static Class forName(String name) throws ClassNotFoundException {
        return forName(name, true, VMClassRegistry.getClassLoader(VMStack
            .getCallerClass(0)));
    }

    /**
     * @com.intel.drl.spec_ref
     */
    public static Class forName(String name, boolean initialize,
            ClassLoader classLoader) throws ClassNotFoundException {
        Class clazz = null;
        int i = 0;
        try {
            // First, if the name is array name convert it to simple reference
            // name. For example, the name "[[Ljava.lang.Object;" will be
            // converted to "java.lang.Object".
            while (name.charAt(i) == '[') {
                i++;
            }
            if (i > 0) {
                switch (name.charAt(i)) {
                case 'L':
                    if (!name.endsWith(";")) {
                        throw new ClassNotFoundException(name);
                    }
                    name = name.substring(i + 1, name.length() - 1);
                    break;
                case 'Z':
                    clazz = Boolean.TYPE;
                    break;
                case 'B':
                    clazz = Byte.TYPE;
                    break;
                case 'C':
                    clazz = Character.TYPE;
                    break;
                case 'D':
                    clazz = Double.TYPE;
                    break;
                case 'F':
                    clazz = Float.TYPE;
                    break;
                case 'I':
                    clazz = Integer.TYPE;
                    break;
                case 'J':
                    clazz = Long.TYPE;
                    break;
                case 'S':
                    clazz = Short.TYPE;
                    break;
                }
                if (clazz != null) {
                    //To reject superfluous symbols
                    if (name.length() > i + 1) {
                        throw new ClassNotFoundException(name);
                    }
                    clazz = VMClassRegistry.loadArray(clazz, i);
                    if (initialize) {
                        VMClassRegistry.initializeClass(clazz);
                    } else {
                        VMClassRegistry.linkClass(clazz);
                    }
                    return clazz;
                }
            }
        } catch (RuntimeException e) {
            throw new ClassNotFoundException(name);
        }

        if (classLoader == null) {
            SecurityManager sc = System.getSecurityManager();
            if (sc != null &&
                VMClassRegistry.getClassLoader(VMStack.getCallerClass(0)) != null) {
                sc.checkPermission(RuntimePermissionCollection.GET_CLASS_LOADER_PERMISSION);
            }
            clazz = VMClassRegistry.findLoadedClass(name, null);
            if (clazz == null) {
                throw new ClassNotFoundException(name);
            }
        } else {
            clazz = classLoader.loadClass(name);
            if (clazz == null) {
                throw new ClassNotFoundException(name);
            }
        }
        if (i > 0) {
            clazz = VMClassRegistry.loadArray(clazz, i);
        }
        if (initialize) {
            VMClassRegistry.initializeClass(clazz);
        } else {
            VMClassRegistry.linkClass(clazz);
        }
        return clazz;
    }

    /**
     * @com.intel.drl.spec_ref
     */
    public boolean desiredAssertionStatus() {
        if (!ClassLoader.enableAssertions) {
            return false;
        }
        ClassLoader classLoader = VMClassRegistry.getClassLoader(this);
        String topLevelName = getTopLevelClassName();
        int systemStatus = 0;
        if (classLoader != null) {
            synchronized (classLoader.classAssertionStatus) {
                Object status = classLoader.classAssertionStatus
                    .get(topLevelName);
                if (status != null) {
                    return ((Boolean)status).booleanValue();
                }
                systemStatus = VMExecutionEngine
                    .getAssertionStatus(topLevelName);
                if (systemStatus != 0) {
                    return systemStatus > 0;
                }
                while ( (topLevelName = getParentName(topLevelName)).length() > 0) {
                    status = classLoader.packageAssertionStatus
                        .get(topLevelName);
                    if (status != null) {
                        return ((Boolean)status).booleanValue();
                    }
                }
                while ( (topLevelName = getParentName(topLevelName)).length() > 0) {
                    if ( (systemStatus = VMExecutionEngine
                        .getAssertionStatus(topLevelName)) != 0) {
                        return systemStatus > 0;
                    }
                }
                return classLoader.defaultAssertionStatus;
            }
        }
        // classLoader == null
        do {
            if ( (systemStatus = VMExecutionEngine
                .getAssertionStatus(topLevelName)) != 0) {
                return systemStatus > 0;
            }
        } while ( (topLevelName = getParentName(topLevelName)).length() > 0);
        return false;
    }

    /**
     * @com.intel.drl.spec_ref
     *
     * Note: We don't check member acces permission for each super class.
     * Java 1.5 API specification doesn't require this check.
     */
    public Class[] getClasses() {
        if (reflectionData == null) {
            initReflectionData();
        }
        checkMemberAccess(Member.PUBLIC);
        if (reflectionData.publicClasses == null) {
            reflectionData.initPublicClasses();
        }
        return (Class[])reflectionData.publicClasses.clone();
    }

    /**
     * @com.intel.drl.spec_ref
     */
    public ClassLoader getClassLoader() {
        ClassLoader loader = VMClassRegistry.getClassLoader(this);
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
    public Class getComponentType() {
        return VMClassRegistry.getComponentType(this);
    }

    /**
     * @com.intel.drl.spec_ref
     */
    public Constructor getConstructor(Class[] argumentTypes)
        throws NoSuchMethodException {
        if (reflectionData == null) {
            initReflectionData();
        }
        checkMemberAccess(Member.PUBLIC);
        if (reflectionData.publicConstructors == null) {
            reflectionData.initPublicConstructors();
        }
        for (int i = 0; i < reflectionData.publicConstructors.length; i++) {
            Constructor c = reflectionData.publicConstructors[i];
            if (isTypeMatches(argumentTypes, c.getParameterTypes())) {
                return Reflection.copyConstructor(c);
            }
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
        if (reflectionData.publicConstructors == null) {
            reflectionData.initPublicConstructors();
        }
        return Reflection.copyConstructors(reflectionData.publicConstructors);
    }

    /**
     * @com.intel.drl.spec_ref
     */
    public Class[] getDeclaredClasses() {
        if (reflectionData == null) {
            initReflectionData();
        }
        checkMemberAccess(Member.DECLARED);
        if (reflectionData.declaredClasses == null) {
            reflectionData.initDeclaredClasses();
        }
        return (Class[])reflectionData.declaredClasses.clone();
    }

    /**
     * @com.intel.drl.spec_ref
     */
    public Constructor getDeclaredConstructor(Class[] argumentTypes)
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
    public Method getDeclaredMethod(String methodName, Class[] argumentTypes)
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
    public Class getDeclaringClass() {
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
        if (reflectionData.publicFields == null) {
            reflectionData.initPublicFields();
        }
        for (int i = 0; i < reflectionData.publicFields.length; i++) {
            Field f = reflectionData.publicFields[i];
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
        if (reflectionData.publicFields == null) {
            reflectionData.initPublicFields();
        }
        return Reflection.copyFields(reflectionData.publicFields);
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
    public Method getMethod(String methodName, Class[] argumentTypes)
        throws NoSuchMethodException {
        if (reflectionData == null) {
            initReflectionData();
        }
        checkMemberAccess(Member.PUBLIC);
        if (reflectionData.publicMethods == null) {
            reflectionData.initPublicMethods();
        }
        return Reflection
            .copyMethod(findMatchingMethod(reflectionData.publicMethods,
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
        if (reflectionData.publicMethods == null) {
            reflectionData.initPublicMethods();
        }
        return Reflection.copyMethods(reflectionData.publicMethods);
    }

    /**
     * @com.intel.drl.spec_ref
     */
    public int getModifiers() {
        return VMClassRegistry.getModifiers(this);
    }

    /**
     * @com.intel.drl.spec_ref
     */
    public String getName() {
        return VMClassRegistry.getName(this);
    }

    /**
     * @com.intel.drl.spec_ref
     */
    public Package getPackage() {
        ClassLoader classLoader = VMClassRegistry.getClassLoader(this);
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
        if (resource == null) {
            return null;
        }
        resource = getAbsoluteResource(resource);
        ClassLoader classLoader = VMClassRegistry.getClassLoader(this);
        return classLoader == null
            ? ClassLoader.getSystemResource(resource)
            : classLoader.getResource(resource);
    }

    /**
     * @com.intel.drl.spec_ref
     */
    public InputStream getResourceAsStream(String resource) {
        if (resource == null) {
            return null;
        }
        resource = getAbsoluteResource(resource);
        ClassLoader classLoader = VMClassRegistry.getClassLoader(this);
        return classLoader == null
            ? ClassLoader.getSystemResourceAsStream(resource)
            : classLoader.getResourceAsStream(resource);
    }

    /**
     * @com.intel.drl.spec_ref
     */
    public Object[] getSigners() {
        try {
            Object[] signers = (Object[])VMClassRegistry.getClassLoader(this).classSigners.get(getName());
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
    public Class getSuperclass() {
        return VMClassRegistry.getSuperclass(this);
    }

    /**
     * @com.intel.drl.spec_ref
     */
    public boolean isArray() {
        return VMClassRegistry.isArray(this);
    }

    /**
     * @com.intel.drl.spec_ref
     */
    public boolean isAssignableFrom(Class clazz) {
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
        return VMClassRegistry.isInterface(this);
    }

    /**
     * @com.intel.drl.spec_ref
     */
    public boolean isPrimitive() {
        return VMClassRegistry.isPrimitive(this);
    }

    /**
     * @com.intel.drl.spec_ref
     */
    public Object newInstance() throws InstantiationException,
        IllegalAccessException {
        Object newInstance = null;
        if (reflectionData == null) {
            initReflectionData();
        }
        SecurityManager sc = System.getSecurityManager();
        if (sc != null) {
            sc.checkMemberAccess(this, Member.PUBLIC);
            sc.checkPackageAccess(reflectionData.packageName);
        }
        try {
            if (reflectionData.defaultConstructor == null) {
                reflectionData.initDefaultConstructor();
                final Constructor c = reflectionData.defaultConstructor;
                try {
                    AccessController.doPrivileged(new PrivilegedAction() {

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
            }
            Reflection.checkMemberAccess(
                VMStack.getCallerClass(0),
                reflectionData.defaultConstructor.getDeclaringClass(),
                reflectionData.defaultConstructor.getDeclaringClass(),
                reflectionData.defaultConstructor.getModifiers()
            );
            newInstance = reflectionData.defaultConstructor.newInstance(null);
        } catch (NoSuchMethodException e) {
            throw new InstantiationException(e.getMessage()
                + " method not found");
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
        if (reflectionData != null) {
            return reflectionData.packageName;
        }
        String className = getName();
        int dotPosition = className.lastIndexOf('.');
        return dotPosition == -1 ? "" : className.substring(0, dotPosition);
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
            if (methodName.equals(m.getName())
                && isTypeMatches(argumentTypes, m.getParameterTypes())
                && (matcher == null || matcher.getReturnType()
                    .isAssignableFrom(m.getReturnType()))) {
                matcher = m;
            }
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

    private Constructor getDeclaredConstructorInternal(Class[] argumentTypes)
        throws NoSuchMethodException {
        if (reflectionData.declaredConstructors == null) {
            reflectionData.initDeclaredConstructors();
        }
        for (int i = 0; i < reflectionData.declaredConstructors.length; i++) {
            Constructor c = reflectionData.declaredConstructors[i];
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


    /* IBM SPECIFIC PART*/

    final ClassLoader getClassLoaderImpl() {
        return VMClassRegistry.getClassLoader(this);
    }

    static final native Class[] getStackClasses(int maxDepth, boolean stopAtPrivileged);

    /**
     *  TODO - provide real implementation
     * @param annotationType
     * @return
     */
    public boolean isAnnotationPresent(Class<? extends Annotation> annotationType) {
        return false;
    }

    /**
     *  TODO - provide real implementation
     * @param annotationType
     * @return
     */
    public <T extends Annotation> T getAnnotation(Class<T> annotationType) {
        return null;
    }

    /**
     *  TODO - provide real implementatoin
     * @return
     */
    public Annotation[] getAnnotations() {
        return new Annotation[0];
    }

    /**
     *  TODO - provide real implementatoin
     * @return
     */
    public Annotation[] getDeclaredAnnotations() {
        return new Annotation[0];
    }

    /**
     *  TODO - provide real implementatoin
     * @return
     */
    public TypeVariable<?>[] getTypeParameters() {
        return new TypeVariable<?>[0];
    }

    /* END OF IBM SPECIFIC PART */

    private final class ReflectionData {

        Class[] declaredClasses;

        Constructor[] declaredConstructors;

        Field[] declaredFields;

        Method[] declaredMethods;

        Constructor defaultConstructor;

        String packageName;

        Class[] publicClasses;

        Constructor[] publicConstructors;

        Field[] publicFields;

        Method[] publicMethods;

        public ReflectionData() {
            packageName = Class.getParentName(Class.this.getName());
        }

        public synchronized void initDeclaredClasses() {
            if (declaredClasses == null) {
                declaredClasses = VMClassRegistry
                    .getDeclaredClasses(Class.this);
            }
        }

        public synchronized void initDeclaredConstructors() {
            if (declaredConstructors == null) {
                declaredConstructors = VMClassRegistry
                    .getDeclaredConstructors(Class.this);
            }
        }

        public synchronized void initDeclaredFields() {
            if (declaredFields == null) {
                declaredFields = VMClassRegistry
                    .getDeclaredFields(Class.this);
            }
        }

        public synchronized void initDeclaredMethods() {
            if (declaredMethods == null) {
                declaredMethods = VMClassRegistry
                    .getDeclaredMethods(Class.this);
            }
        }

        public synchronized void initDefaultConstructor()
            throws NoSuchMethodException {
            if (defaultConstructor == null) {
                defaultConstructor = Class.this
                    .getDeclaredConstructorInternal(null);
            }
        }

        public synchronized void initPublicClasses() {
            if (publicClasses != null) {
                return;
            }
            if (declaredClasses == null) {
                initDeclaredClasses();
            }

            int size = declaredClasses.length;
            Class superClass = Class.this.getSuperclass();
            if (superClass != null) {
                if (superClass.reflectionData == null) {
                    superClass.initReflectionData();
                }
                if (superClass.reflectionData.publicClasses == null) {
                    superClass.reflectionData.initPublicClasses();
                }
                // most likely this size is enough
                size += superClass.reflectionData.publicClasses.length;
            }

            ArrayList classes = new ArrayList(size);
            for (int i = 0; i < declaredClasses.length; i++) {
                Class c = declaredClasses[i];
                if (Modifier.isPublic(c.getModifiers())) {
                    classes.add(c);
                }
            }

            if (superClass != null) {
                classes.addAll(Arrays
                    .asList(superClass.reflectionData.publicClasses));
            }

            publicClasses = (Class[])classes.toArray(new Class[classes.size()]);
        }

        public synchronized void initPublicConstructors() {
            if (publicConstructors != null) {
                return;
            }
            if (declaredConstructors == null) {
                initDeclaredConstructors();
            }
            ArrayList constructors = new ArrayList(declaredConstructors.length);
            for (int i = 0; i < declaredConstructors.length; i++) {
                Constructor c = declaredConstructors[i];
                if (Modifier.isPublic(c.getModifiers())) {
                    constructors.add(c);
                }
            }
            publicConstructors = (Constructor[])constructors
                .toArray(new Constructor[constructors.size()]);
        }

        /**
         * Stores public fields in order they should be searched by
         * getField(name) method.
         */
        public synchronized void initPublicFields() {
            if (publicFields != null) {
                return;
            }
            if (declaredFields == null) {
                initDeclaredFields();
            }

            // initialize public fields of the super class
            int size = declaredFields.length;
            Class superClass = Class.this.getSuperclass();
            if (superClass != null) {
                if (superClass.reflectionData == null) {
                    superClass.initReflectionData();
                }
                if (superClass.reflectionData.publicFields == null) {
                    superClass.reflectionData.initPublicFields();
                }
                size += superClass.reflectionData.publicFields.length;
            }

            // add public fields of this class
            LinkedHashSet fields = new LinkedHashSet(size);
            if (Class.this.isInterface()) {
                fields.addAll(Arrays.asList(declaredFields));
            } else {
                for (int i = 0; i < declaredFields.length; i++) {
                    Field f = declaredFields[i];
                    if (Modifier.isPublic(f.getModifiers())) {
                        fields.add(f);
                    }
                }
            }

            // initialize and add fields of the super interfaces
            Class[] interfaces = Class.this.getInterfaces();
            for (int j = 0; j < interfaces.length; j++) {
                Class interf = interfaces[j];
                if (interf.reflectionData == null) {
                    interf.initReflectionData();
                }
                if (interf.reflectionData.publicFields == null) {
                    interf.reflectionData.initPublicFields();
                }
                fields.addAll(Arrays.asList(interf.reflectionData.publicFields));
            }

            // add public fields of the super class
            if (superClass != null) {
                fields.addAll(Arrays
                    .asList(superClass.reflectionData.publicFields));
            }

            publicFields = (Field[])fields.toArray(new Field[fields
                .size()]);
        }

        public synchronized void initPublicMethods() {
            if (publicMethods != null) {
                return;
            }
            if (declaredMethods == null) {
                initDeclaredMethods();
            }

            // initialize public methods of the super class
            int size = declaredMethods.length;
            Class superClass = Class.this.getSuperclass();
            if (superClass != null) {
                if (superClass.reflectionData == null) {
                    superClass.initReflectionData();
                }
                if (superClass.reflectionData.publicMethods == null) {
                    superClass.reflectionData.initPublicMethods();
                }
                // most likely this size is enough
                size += superClass.reflectionData.publicMethods.length;
            }

            // the storage for public methods
            ArrayList methods = new ArrayList(size);
            // the set of names of public methods
            HashSet methodNames = new HashSet(size);

            // add public methods of this class
            if (Class.this.isInterface()) {
                // every interface method is public
                for (int i = 0; i < declaredMethods.length; i++) {
                    methods.add(declaredMethods[i]);
                    methodNames.add(declaredMethods[i].getName());
                }
            } else {
                for (int i = 0; i < declaredMethods.length; i++) {
                    Method m = declaredMethods[i];
                    if (Modifier.isPublic(m.getModifiers())) {
                        methods.add(m);
                        methodNames.add(m.getName());
                    }
                }
            }

            if (superClass != null) {
                // add public methods of the super class
                mergeMethods(methodNames, methods,
                             superClass.reflectionData.publicMethods);
            }

            // add methods of the super interfaces
            Class[] interfaces = Class.this.getInterfaces();
            for (int j = 0; j < interfaces.length; j++) {
                Class interf = interfaces[j];
                if (interf.reflectionData == null) {
                    interf.initReflectionData();
                }
                if (interf.reflectionData.publicMethods == null) {
                    interf.reflectionData.initPublicMethods();
                }
                mergeMethods(methodNames, methods,
                             interf.reflectionData.publicMethods);
            }

            publicMethods = (Method[])methods
                .toArray(new Method[methods.size()]);
        }

        /**
         *  Checks if two methods match by name and parameter types.
         *  Ignored return type
         *
         * @param m1 one method to check
         * @param m2 the other method to check
         * @return true if they match, false otherwise
         */
        private boolean isMethodMatches(Method m1, Method m2) {
            return m1.getName().equals(m2.getName())
                && isTypeMatches(m1.getParameterTypes(), m2.getParameterTypes());
        }

        private void mergeMethods(HashSet names, ArrayList thisMethods,
                                  Method[] superMethods) {
            for (int i = 0; i < superMethods.length; i++) {
                Method superMethod = superMethods[i];
                if (names.contains(superMethod.getName())) {
                    int j;
                    for (j = 0; j < thisMethods.size(); j++) {
                        Method thisMethod = (Method)thisMethods.get(j);
                        if (isMethodMatches(thisMethod, superMethod)) {
                            break;
                        }
                    }
                    if (j >= thisMethods.size()) {
                        thisMethods.add(superMethod);
                        names.add(superMethod.getName());
                    }
                } else {
                    thisMethods.add(superMethod);
                    names.add(superMethod.getName());
                }
            }
        }
    }
}
