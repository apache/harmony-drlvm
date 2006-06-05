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

import java.util.Arrays;

import org.apache.harmony.vm.VMStack;

/**
 * @com.intel.drl.spec_ref 
 */
public final class Method extends AccessibleObject implements Member {

    /**
     * cache of the method data
     */
    private final MethodData data;

    /**
     * Copy constructor
     * 
     * @param m original method
     */
    Method(Method m) {
        data = m.data;
        isAccessible = m.isAccessible;
    }
    
    /**
     * Only VM should call this constructor
     * 
     * @param obj method handle
     * @api2vm
     */
    Method(Object obj) {
        data = new MethodData(obj);
    }
    
    /**
     * Called by VM to obtain this method's handle.
     * 
     * @return handle for this method
     * @api2vm
     */
    Object getHandle() {
        return data.id;
    }

    /**
     * @com.intel.drl.spec_ref 
     */
    public boolean equals(Object obj) {
        try {
            // Sole comparison by method id is not enough because there are methods
            // created through JNI. That methods have different ids.
            Method another = (Method)obj;
            if (data.id == another.data.id){
                return true;
            }
            return getDeclaringClass().equals(another.getDeclaringClass())
                && getName().equals(another.getName()) 
                && getReturnType().equals(another.getReturnType())
                && Arrays.equals(getParameterTypes(), another.getParameterTypes());
        } catch (RuntimeException e) {
        }
        return false;
    }

    /**
     * @com.intel.drl.spec_ref 
     */
    public Class getDeclaringClass() {
        if (data.declaringClass == null) {
            data.initDeclaringClass();
        }
        return data.declaringClass;
    }

    /**
     * @com.intel.drl.spec_ref 
     */
    public Class[] getExceptionTypes() {
        if (data.exceptionTypes == null) {
            data.initExceptionTypes();
        }
        return (Class[])data.exceptionTypes.clone();
    }

    /**
     * @com.intel.drl.spec_ref 
     */
    public int getModifiers() {
        if (data.modifiers == -1) {
            data.initModifiers();
        }
        return data.modifiers;
    }

    /**
     * @com.intel.drl.spec_ref 
     */
    public String getName() {
        if (data.name == null) {
            data.initName();
        }
        return data.name;
    }

    /**
     * @com.intel.drl.spec_ref 
     */
    public Class[] getParameterTypes() {
        if (data.parameterTypes == null) {
            data.initParameterTypes();
        }
        return (Class[])data.parameterTypes.clone();
    }

    /**
     * @com.intel.drl.spec_ref 
     */
    public Class getReturnType() {
        if (data.returnType == null) {
            data.initReturnType();
        }
        return data.returnType;
    }

    /**
     * @com.intel.drl.spec_ref 
     */
    public int hashCode() {
        return getDeclaringClass().getName().hashCode() ^ getName().hashCode();
    }

    /**
     * @com.intel.drl.spec_ref 
     */
    public Object invoke(Object obj, Object[] args)
        throws IllegalAccessException, IllegalArgumentException,
        InvocationTargetException {
    	
        obj = checkObject(getDeclaringClass(), getModifiers(), obj);
        
        // check parameter validity
        if (data.parameterTypes == null) {
            data.initParameterTypes();
        }
        checkInvokationArguments(data.parameterTypes, args);
        
        if (!isAccessible) {
            reflectExporter.checkMemberAccess(
                VMStack.getCallerClass(0), getDeclaringClass(),
                obj == null ? getDeclaringClass() : obj.getClass(),
                getModifiers()
            );
        }
        return VMReflection.invokeMethod(obj, data.id, args);
    }

    /**
     * @com.intel.drl.spec_ref 
     */
    public String toString() {
        StringBuffer sb = new StringBuffer();
        // initialize data
        if (data.parameterTypes == null) {
            data.initParameterTypes();
        }
        if (data.exceptionTypes == null) {
            data.initExceptionTypes();
        }
        // append modifiers if any
        int modifier = getModifiers();
        if (modifier != 0) {
            sb.append(Modifier.toString(modifier)).append(' ');            
        }
        // append return type
        appendArrayType(sb, getReturnType());
        sb.append(' ');
        // append full method name
        sb.append(getDeclaringClass().getName()).append('.').append(getName());
        // append parameters
        sb.append('(');
        appendArrayType(sb, data.parameterTypes);
        sb.append(')');
        // append exeptions if any
        if (data.exceptionTypes.length > 0) {
            sb.append(" throws ");
            appendSimpleType(sb, data.exceptionTypes);
        }
        return sb.toString();
    }

    /* NON API SECTION */

    /**
     * Reconstructs the signature of this method.
     * 
     * @return the signature of the method 
     */
    String getSignature() {
        //XXX: seems, it may be more effective to realize this request via 
        //API2VM interface, i.e. just to obtain the signature of the method descriptor
        //from the class file.
        StringBuffer buf = new StringBuffer().append('(');
        if (data.parameterTypes == null) {
            data.initParameterTypes();
        }
        for (int i = 0; i < data.parameterTypes.length; i++)
            buf.append(getClassSignature(data.parameterTypes[i]));
        buf.append(')');
        if (data.returnType == null) {
            data.initReturnType();
        }
        buf.append(getClassSignature(data.returnType));
        return buf.toString().replace('/','.');
    }

    /**
     * Keeps an information about this method
     */
    private class MethodData {

        /**
         * method handle which is used to retrieve all necessary information
         * about this method object
         */
        final Object id;

        /**
         * declaring class
         */
        Class declaringClass;

        /**
         * method exeptions
         */
        Class[] exceptionTypes;

        /**
         * method modifiers
         */
        int modifiers = -1;

        /**
         * method name
         */
        String name;

        /**
         * method parameters
         */
        Class[] parameterTypes;

        /** 
         * method return type
         */
        Class returnType;

        /**
         * @param obj method handler
         */
        public MethodData(Object obj) {
            id  = obj;
        }
        
        /**
         * initializes declaring class
         */
        public synchronized void initDeclaringClass() {
            if (declaringClass == null) {
                declaringClass = VMReflection.getDeclaringClass(id);
            }
        }

        /**
         * initializes exeptions
         */
        public synchronized void initExceptionTypes() {
            if (exceptionTypes == null) {
                exceptionTypes = VMReflection.getExceptionTypes(id);
            }
        }

        /**
         * initializes modifiers
         */
        public synchronized void initModifiers() {
            if (modifiers == -1) {
                modifiers = VMReflection.getModifiers(id);
            }
        }

        /**
         * initializes name 
         */
        public synchronized void initName() {
            if (name == null) {
                name = VMReflection.getName(id);
            }
        }

        /**
         * initializes parameters
         */
        public synchronized void initParameterTypes() {
            if (parameterTypes == null) {
                parameterTypes = VMReflection.getParameterTypes(id);
            }
        }

        /**
         * initializes return type
         */
        public synchronized void initReturnType() {
            if (returnType == null) {
                returnType = VMReflection.getMethodReturnType(id);
            }
        }
    }
}
