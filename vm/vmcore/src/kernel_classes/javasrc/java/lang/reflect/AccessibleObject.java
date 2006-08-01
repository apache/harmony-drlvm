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
 * @author Evgueni Brevnov
 * @version $Revision: 1.1.2.2.4.4 $
 */

package java.lang.reflect;

import org.apache.harmony.lang.reflect.ReflectPermissionCollection;
import org.apache.harmony.lang.reflect.Reflection;

/**
 * @com.intel.drl.spec_ref 
 */
public class AccessibleObject {

    /**
     * one dimensional array
     */
    private static final String DIMENSION_1 = "[]";
    
    /**
     * two dimensional array
     */
    private static final String DIMENSION_2 = "[][]";

    /**
     * three dimensional array
     */
    private static final String DIMENSION_3 = "[][][]";
    
    /**
     * used to exchange information with the java.lang package
     */
    static final ReflectExporter reflectExporter;
        
    /**
     * indicates whether this object is accessible or not
     */
    boolean isAccessible = false;

    static {
        reflectExporter = new ReflectExporter();
        Reflection.setReflectAccessor(reflectExporter);
    }

    /**
     * @com.intel.drl.spec_ref 
     */
    protected AccessibleObject() {
    }

    /**
     * @com.intel.drl.spec_ref 
     */
    public static void setAccessible(AccessibleObject[] objs, boolean flag)
        throws SecurityException {
        SecurityManager sc = System.getSecurityManager();
        if (sc != null) {
            sc.checkPermission(ReflectPermissionCollection.SUPPRESS_ACCESS_CHECKS_PERMISSION);
        }
        for (int i = 0; i < objs.length; i++) {
            objs[i].setAccessible0(flag);
        }
    }

    /**
     * @com.intel.drl.spec_ref 
     */
    public boolean isAccessible() {
        return isAccessible;
    }

    /**
     * @com.intel.drl.spec_ref 
     */
    public void setAccessible(boolean flag) throws SecurityException {
        SecurityManager sc = System.getSecurityManager();
        if (sc != null) {
            sc.checkPermission(ReflectPermissionCollection.SUPPRESS_ACCESS_CHECKS_PERMISSION);
        }
        setAccessible0(flag);
    }

    /*
     * NON API SECTION
     */

    /**
     * Checks obj argument for correctness.
     * 
     * @param declaringClass declaring class of the field/method member 
     * @param memberModifier field/method member modifier
     * @param obj object to check for correctness
     * @return null if accessing static member, otherwise obj 
     * @throws IllegalArgumentException if obj is not valid object
     * @throws NullPointerException if obj or declaringClass argument is null
     */
    Object checkObject(Class declaringClass, int memberModifier, Object obj)
        throws IllegalArgumentException {
        if (Modifier.isStatic(memberModifier)) {
            return null;
        } else if (!declaringClass.isInstance(obj)) {
            if (obj == null) {
                throw new NullPointerException(
                    "The specified object is null but the method is not static");
            }
            throw new IllegalArgumentException(
                "The specified object should be an instance of " + declaringClass);
        }
        return obj;
    }

    /**
     * Appends the specified class name to the buffer. The class may represent
     * a simple type, a reference type or an array type.
     * 
     * @param sb buffer
     * @param obj the class which name should be appended to the buffer
     * @throws NullPointerException if any of the arguments is null 
     */
    void appendArrayType(StringBuffer sb, Class obj) {
        Class simplified = obj.getComponentType();
        if (simplified == null) {
            sb.append(obj.getName());
            return;
        }
        int dimensions = 1;
        obj = simplified;
        while ((simplified = obj.getComponentType()) != null) {
            obj = simplified;
            dimensions++;
        }
        sb.append(obj.getName());
        switch (dimensions) {
        case 1:
            sb.append(DIMENSION_1);
            break;
        case 2:
            sb.append(DIMENSION_2);
            break;
        case 3:
            sb.append(DIMENSION_3);
            break;
        default:
            for (; dimensions > 0; dimensions--) {
                sb.append(DIMENSION_1);
            }
        }
    }

    /**
     * Appends names of the specified array classes to the buffer. The array
     * elements may represent a simple type, a reference type or an array type.
     * Output format: java.lang.Object[], java.io.File, void
     * 
     * @param sb buffer
     * @param objs array of classes to print the names
     * @throws NullPointerException if any of the arguments is null 
     */
    void appendArrayType(StringBuffer sb, Class[] objs) {
        if (objs.length > 0) {
            appendArrayType(sb, objs[0]);
            for (int i = 1; i < objs.length; i++) {
                sb.append(',');
                appendArrayType(sb, objs[i]);
            }
        }
    }

    /**
     * Appends names of the specified array classes to the buffer. The array
     * elements may represent a simple type, a reference type or an array type.
     * In case if the specified array element represents an array type its
     * internal will be appended to the buffer.   
     * Output format: [Ljava.lang.Object;, java.io.File, void
     * 
     * @param sb buffer
     * @param objs array of classes to print the names
     * @throws NullPointerException if any of the arguments is null 
     */
    void appendSimpleType(StringBuffer sb, Class[] objs) {
        if (objs.length > 0) {
            sb.append(objs[0].getName());
            for (int i = 1; i < objs.length; i++) {
                sb.append(',');
                sb.append(objs[i].getName());
            }
        }
    }

    /**
     * Changes accessibility to the specified.
     * 
     * @param flag accessible flag
     * @throws SecurityException if this object represents a constructor of the
     *         Class
     */
    private void setAccessible0(boolean flag) throws SecurityException {
        if (flag && this instanceof Constructor
            && ((Constructor)this).getDeclaringClass() == Class.class) {
            throw new SecurityException(
                "Can not make the java.lang.Class class constructor accessible");
        }
        isAccessible = flag;
    }

    /**
     * Returns the signature-alike name representation of the class or interface.
     * 
     * @param c class to request the signature
     * @return the signature-alike name of the class or interface 
     */
    static String getClassSignature(Class c) {
        StringBuffer buf = new StringBuffer();
        while (c.isArray()) {
            buf.append('[');
            c = c.getComponentType();
        }

        if (!c.isPrimitive())
            buf.append('L').append(c.getName().replace('.', '/')).append(';');
        else if (c == int.class)
            buf.append('I');
        else if (c == byte.class)
            buf.append('B');
        else if (c == long.class)
            buf.append('J');
        else if (c == float.class)
            buf.append('F');
        else if (c == double.class)
            buf.append('D');
        else if (c == short.class)
            buf.append('S');
        else if (c == char.class)
            buf.append('C');
        else if (c == boolean.class)
            buf.append('Z');
        else if (c == void.class)
            buf.append('V');
        return buf.toString();
    }
    
    /**
     * Ensures that actual parameters are compartible with types of
     * formal parameters. For reference types, argument can be either 
     * null or assignment-compatible with formal type. 
     * For primitive types, argument must be non-null wrapper instance. 
     * @param types formal parameter' types
     * @param args runtime arguments
     * @throws IllegalArgumentException if arguments are incompartible
     */
    static void checkInvokationArguments(Class[] types, Object[] args) {
        if ((args == null) ? types.length != 0
                : args.length != types.length) {
            throw new IllegalArgumentException(
                    "Invalid number of actual parameters");
        }
        for (int i = types.length - 1; i >= 0; i--) {
            if (types[i].isPrimitive()) {
                if (args[i] instanceof Number 
                    || args[i] instanceof Character
                    || args[i] instanceof Boolean) {
                    // more accurate conversion testing better done on VM side                    
                    continue;
                }
            } else if (args[i] == null || types[i].isInstance(args[i])) {
                continue;
            }
            throw new IllegalArgumentException("Actual parameter: "
                  + (args[i] == null ? "<null>" : args[i].getClass().getName())   
                  + " is incompatible with " + types[i].getName());
        }
    }
}
