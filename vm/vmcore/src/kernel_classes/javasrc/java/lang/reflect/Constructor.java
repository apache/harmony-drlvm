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
public final class Constructor extends AccessibleObject implements Member {

    /**
     * cache of the constructor data
     */
    private final ConstructorData data;

    /**
     * Copy constructor
     * 
     * @param c original constructor
     */
    Constructor(Constructor c) {
        data = c.data;
        isAccessible = c.isAccessible;
    }

    /**
     * Only VM should call this constructor
     * 
     * @param obj constructor handler
     * @api2vm
     */
    Constructor(Object obj) {
        data = new ConstructorData(obj);
    }
    
    /**
     * Called by VM to obtain this constructor's handle.
     * 
     * @return handle for this constructor
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
            // Sole comparison by id is not enough because there are constructors
            // created through JNI. That constructors have different ids.
            Constructor another = (Constructor)obj;
            if (data.id == another.data.id){
                return true;
            }
            return getDeclaringClass().equals(another.getDeclaringClass())
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
    public int hashCode() {
        return getDeclaringClass().getName().hashCode();
    }

    /**
     * @com.intel.drl.spec_ref 
     */
    public Object newInstance(Object[] args) throws InstantiationException,
        IllegalAccessException, IllegalArgumentException,
        InvocationTargetException {
        if (Modifier.isAbstract(getDeclaringClass().getModifiers())) {
            throw new InstantiationException("Can not instantiate abstract "
                + getDeclaringClass());
        }
        
        // check parameter validity
        if (data.parameterTypes == null) {
            data.initParameterTypes();
        }
        checkInvokationArguments(data.parameterTypes, args);
        
        if (!isAccessible) {
            reflectExporter.checkMemberAccess(VMStack.getCallerClass(0),
                                              getDeclaringClass(),
                                              getDeclaringClass(),
                                              getModifiers());
        }
        return VMReflection.newClassInstance(data.id, args);
    }

    /**
     * @com.intel.drl.spec_ref 
     */
    public String toString() {
        StringBuffer sb = new StringBuffer();
        // data initialization
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
        // append constructor name
        appendArrayType(sb, getDeclaringClass());
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
     * Reconstructs the signature of this constructor.
     * 
     * @return the signature of the constructor 
     */
    String getSignature() {
        //XXX: seems, it may be more effective to realize this request via 
        //API2VM interface, i.e. just to obtain the signature of the constructor descriptor
        //from the class file.
        StringBuffer buf = new StringBuffer().append('(');
        if (data.parameterTypes == null) {
            data.initParameterTypes();
        }
        for (int i = 0; i < data.parameterTypes.length; i++)
            buf.append(getClassSignature(data.parameterTypes[i]));
        buf.append(")V");
        return buf.toString().replace('/','.');
    }

    /**
     * Keeps an information about this constructor
     */
    private class ConstructorData {

        /**
         * constructor handle which is used to retrieve all necessary
         * information about this constructor object
         */
        final Object id;

        /**
         * declaring class
         */
        Class declaringClass;

        /**
         * constructor exceptions
         */
        Class[] exceptionTypes;

        /**
         * constructor modifiers
         */
        int modifiers = -1;

        /**
         * constructor name
         */
        String name;

        /**
         * constructor parameters
         */
        Class[] parameterTypes;

        /**
         * @param obj constructor handler 
         */
        public ConstructorData(Object obj) {
            id = obj;
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
    }
}
