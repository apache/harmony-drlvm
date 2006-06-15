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

import org.apache.harmony.vm.VMStack;

/**
 * @com.intel.drl.spec_ref 
 */
public final class Field extends AccessibleObject implements Member {

    /**
     * cache of the field data
     */
    private final FieldData data;

    /**
     * Copy constructor
     * 
     * @param f original field
     */
    Field(Field f) {
        data = f.data;
        isAccessible = f.isAccessible;
    }

    /**
     * Only VM should call this constructor.
     * 
     * @param obj field handler
     * @api2vm
     */
    Field(Object obj) {
        data = new FieldData(obj);
    }

    /**
     *  TODO : fix gmj
     */
    public boolean isSynthetic() {
        return false;
    }
    
    /**
     * Called by VM to obtain this field's handle.
     * 
     * @return handle for this field
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
            // Sole comparison by id is not enought because there are fields created 
            // through JNI. That fields have different ids. 
            return data.id == ((Field)obj).data.id
                || (getDeclaringClass().equals( ((Field)obj).getDeclaringClass()) 
                    && getName().equals( ((Field)obj).getName()));
        } catch (RuntimeException e) {
        }
        return false;
    }

    /**
     * @com.intel.drl.spec_ref 
     */
    public Object get(Object obj) throws IllegalArgumentException,
        IllegalAccessException {
        obj = checkGet(VMStack.getCallerClass(0), obj);
        return VMReflection.getFieldValue(obj, data.id);
    }

    /**
     * @com.intel.drl.spec_ref 
     */
    public boolean getBoolean(Object obj) throws IllegalArgumentException,
        IllegalAccessException {
        obj = checkGet(VMStack.getCallerClass(0), obj);
        if (getType() == Boolean.TYPE) {
            return ((Boolean)VMReflection.getFieldValue(obj, data.id)).booleanValue();
        }
        throw getIllegalArgumentException(Boolean.TYPE);
    }

    /**
     * @com.intel.drl.spec_ref 
     */
    public byte getByte(Object obj) throws IllegalArgumentException,
        IllegalAccessException {
        obj = checkGet(VMStack.getCallerClass(0), obj);
        if (getType() == Byte.TYPE) {
            return ((Number)VMReflection.getFieldValue(obj, data.id)).byteValue();
        }
        throw getIllegalArgumentException(Byte.TYPE);
    }

    /**
     * @com.intel.drl.spec_ref 
     */
    public char getChar(Object obj) throws IllegalArgumentException,
        IllegalAccessException {
        obj = checkGet(VMStack.getCallerClass(0), obj);
        if (getType() == Character.TYPE) {
            return ((Character)VMReflection.getFieldValue(obj, data.id)).charValue();
        }
        throw getIllegalArgumentException(Character.TYPE);
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
    public double getDouble(Object obj) throws IllegalArgumentException,
        IllegalAccessException {
        obj = checkGet(VMStack.getCallerClass(0), obj);
        Class type = getType();
        if (type.isPrimitive() && type != Boolean.TYPE) {
            if (type == Character.TYPE) {
                return ((Character)VMReflection.getFieldValue(obj, data.id)).charValue();
            }
            return ((Number)VMReflection.getFieldValue(obj, data.id)).doubleValue();
        }
        throw getIllegalArgumentException(Double.TYPE);
    }

    /**
     * @com.intel.drl.spec_ref 
     */
    public float getFloat(Object obj) throws IllegalArgumentException,
        IllegalAccessException {
        obj = checkGet(VMStack.getCallerClass(0), obj);
        Class type = getType();
        if (type.isPrimitive() && type != Double.TYPE && type != Boolean.TYPE) {
            if (type == Character.TYPE) {
                return ((Character)VMReflection.getFieldValue(obj, data.id)).charValue();
            }
            return ((Number)VMReflection.getFieldValue(obj, data.id)).floatValue();
        }
        throw getIllegalArgumentException(Float.TYPE);
    }

    /**
     * @com.intel.drl.spec_ref 
     */
    public int getInt(Object obj) throws IllegalArgumentException,
        IllegalAccessException {
        obj = checkGet(VMStack.getCallerClass(0), obj);
        Class type = getType();
        if (type == Integer.TYPE || type == Short.TYPE || type == Byte.TYPE) {
            return ((Number)VMReflection.getFieldValue(obj, data.id)).intValue();
        }
        if (type == Character.TYPE) {
            return ((Character)VMReflection.getFieldValue(obj, data.id)).charValue();
        }
        throw getIllegalArgumentException(Integer.TYPE);
    }

    /**
     * @com.intel.drl.spec_ref 
     */
    public long getLong(Object obj) throws IllegalArgumentException,
        IllegalAccessException {
        obj = checkGet(VMStack.getCallerClass(0), obj);
        Class type = getType();
        if (type == Long.TYPE || type == Integer.TYPE || type == Short.TYPE
            || type == Byte.TYPE) {
            return ((Number)VMReflection.getFieldValue(obj, data.id)).longValue();
        }
        if (type == Character.TYPE) {
            return ((Character)VMReflection.getFieldValue(obj, data.id)).charValue();
        }
        throw getIllegalArgumentException(Long.TYPE);
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
    public short getShort(Object obj) throws IllegalArgumentException,
        IllegalAccessException {
        obj = checkGet(VMStack.getCallerClass(0), obj);
        Class type = getType();
        if (type == Short.TYPE || type == Byte.TYPE) {
            return ((Number)VMReflection.getFieldValue(obj, data.id)).shortValue();
        }
        throw getIllegalArgumentException(Short.TYPE);
    }

    /**
     * @com.intel.drl.spec_ref 
     */
    public Class getType() {
        if (data.type == null) {
            data.initType();
        }
        return data.type;
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
    public void set(Object obj, Object value) throws IllegalArgumentException,
        IllegalAccessException {
        // check that input is valid
        obj = checkSet(VMStack.getCallerClass(0), obj);
        // is this field primitive?
        if (!getType().isPrimitive()) {
            //can this value be converted to the type of the field?
            //(The null reference can always be converted to any reference type.)
            if (value == null || getType().isInstance(value)) {
                VMReflection.setFieldValue(obj, data.id, value);
                return;
            }
        } else { // this field is primitive
            if (value instanceof Number) {
                if (value instanceof Integer) {
                    setIntNoObjectCheck(obj, ((Integer)value).intValue());
                    return;
                } else if (value instanceof Float) {
                    setFloatNoObjectCheck(obj, ((Float)value).floatValue());
                    return;
                } else if (value instanceof Double) {
                    setDoubleNoObjectCheck(obj, ((Double)value).doubleValue());
                    return;
                } else if (value instanceof Long) {
                    setLongNoObjectCheck(obj, ((Long)value).longValue());
                    return;
                } else if (value instanceof Short) {
                    setShortNoObjectCheck(obj, ((Short)value).shortValue());
                    return;
                } else if (value instanceof Byte) {
                    setByteNoObjectCheck(obj, ((Byte)value).byteValue());
                    return;
                }
            } else if (value instanceof Boolean) {
                setBooleanNoObjectCheck(obj, ((Boolean)value).booleanValue());
                return;
            } else if (value instanceof Character) {
                setCharNoObjectCheck(obj, ((Character)value).charValue());
                return;
            }
        }
        // the specified value doesn't correspond to any primitive type
        throw new IllegalArgumentException(
            "The specified value can not be converted to a "
                + getType().getName()
                + " type by an unwrapping, identity and widening conversions");
    }

    /**
     * @com.intel.drl.spec_ref 
     */
    public void setBoolean(Object obj, boolean value)
        throws IllegalArgumentException, IllegalAccessException {        
        obj = checkSet(VMStack.getCallerClass(0), obj);
        setBooleanNoObjectCheck(obj, value);
    }
    
    /**
     * @com.intel.drl.spec_ref 
     */
    public void setByte(Object obj, byte value)
        throws IllegalArgumentException, IllegalAccessException {
        obj = checkSet(VMStack.getCallerClass(0), obj);
        setByteNoObjectCheck(obj, value);
    }
    
    /**
     * @com.intel.drl.spec_ref 
     */
    public void setChar(Object obj, char value)
        throws IllegalArgumentException, IllegalAccessException {
        obj = checkSet(VMStack.getCallerClass(0), obj);
        setCharNoObjectCheck(obj, value);
    }

    /**
     * @com.intel.drl.spec_ref 
     */
    public void setDouble(Object obj, double value)
        throws IllegalArgumentException, IllegalAccessException {
        obj = checkSet(VMStack.getCallerClass(0), obj);
        setDoubleNoObjectCheck(obj, value);
    }

    /**
     * @com.intel.drl.spec_ref 
     */
    public void setFloat(Object obj, float value)
        throws IllegalArgumentException, IllegalAccessException {
        obj = checkSet(VMStack.getCallerClass(0), obj);
        setFloatNoObjectCheck(obj, value);
    }

    /**
     * @com.intel.drl.spec_ref 
     */
    public void setInt(Object obj, int value) throws IllegalArgumentException,
        IllegalAccessException {
        obj = checkSet(VMStack.getCallerClass(0), obj);
        setIntNoObjectCheck(obj, value);
    }

    /**
     * @com.intel.drl.spec_ref 
     */
    public void setLong(Object obj, long value)
        throws IllegalArgumentException, IllegalAccessException {
        obj = checkSet(VMStack.getCallerClass(0), obj);
        setLongNoObjectCheck(obj, value);
    }

    /**
     * @com.intel.drl.spec_ref 
     */
    public void setShort(Object obj, short value)
        throws IllegalArgumentException, IllegalAccessException {
        obj = checkSet(VMStack.getCallerClass(0), obj);
        setShortNoObjectCheck(obj, value);
    }

    /**
     * @com.intel.drl.spec_ref 
     */
    public String toString() {
        StringBuffer sb = new StringBuffer();
        // append modifiers if any
        int modifier = getModifiers();
        if (modifier != 0) {
            sb.append(Modifier.toString(modifier)).append(' ');
        }
        // append return type
        appendArrayType(sb, getType());
        sb.append(' ');
        // append full field name
        sb.append(getDeclaringClass().getName()).append('.').append(getName());
        return sb.toString();
    }

    /* NON API SECTION */

    /**
     * Checks that the specified obj is valid object for a getXXX operation.
     * 
     * @param callerClass caller class of a getXXX method
     * @param obj object to check
     * @return null if this field is static, otherwise obj one
     * @throws IllegalArgumentException if obj argument is not valid
     * @throws IllegalAccessException if caller doesn't have access permission
     */
    private Object checkGet(Class callerClass, Object obj)
        throws IllegalArgumentException, IllegalAccessException {
        obj = checkObject(getDeclaringClass(), getModifiers(), obj);
        if (!isAccessible) {
            reflectExporter.checkMemberAccess(callerClass, getDeclaringClass(),
                obj == null ? getDeclaringClass() : obj.getClass(), getModifiers());
        }
        return obj;
    }
    
    /**
     * Checks that the specified obj is valid object for a setXXX operation.
     * 
     * @param callerClass caller class of a setXXX method
     * @param obj object to check
     * @return null if this field is static, otherwise obj
     * @throws IllegalArgumentException if obj argument is not valid one
     * @throws IllegalAccessException if caller doesn't have access permission
     *         or this field is final
     */
    private Object checkSet(Class callerClass, Object obj)
        throws IllegalArgumentException, IllegalAccessException {
        obj = checkObject(getDeclaringClass(), getModifiers(), obj);
        if (Modifier.isFinal(getModifiers())) {
            // TODO perform this check for 1.5.0
            // && !(isAccessible && obj != null)) {
            throw new IllegalAccessException(
                "Can not assign new value to the field with final modifier");
        }
        if (!isAccessible) {
            reflectExporter.checkMemberAccess(callerClass, getDeclaringClass(),
                obj == null ? getDeclaringClass() : obj.getClass(), getModifiers());
        }
        return obj;
    }

    /**
     * Constructs IllegalArgumentException
     * 
     * @param targetType type to construct exception for
     * @return constructed exception
     */
    private IllegalArgumentException getIllegalArgumentException(Class targetType) {
        return new IllegalArgumentException(
            "The field value can not be converted to a \""
                + targetType.getName() + "\" type by a widening conversion");
    }

    /**
     * Sets boolean value with out obj checking 
     * 
     * @param obj object
     * @param value new value
     * @throws IllegalArgumentException if value is not valid
     */
    private void setBooleanNoObjectCheck(Object obj, boolean value)
        throws IllegalArgumentException {
        if (getType() == Boolean.TYPE) {
            VMReflection.setFieldValue(obj, data.id, Boolean.valueOf(value));
            return;
        }
        throw new IllegalArgumentException(
            "The specified value can not be assigned to the field of a \""
                + getType().getName() + "\" type");
    }

    /**
     * Sets byte value with out obj checking 
     * 
     * @param obj object
     * @param value new value
     * @throws IllegalArgumentException if value is not valid
     */
    private void setByteNoObjectCheck(Object obj, byte value)
        throws IllegalArgumentException {
        if (getType() == Byte.TYPE) {
            VMReflection.setFieldValue(obj, data.id, new Byte(value));
            return;
        }
        setShortNoObjectCheck(obj, value);
    }

    /**
     * Sets char value with out obj checking 
     * 
     * @param obj object
     * @param value new value
     * @throws IllegalArgumentException if value is not valid
     */
    private void setCharNoObjectCheck(Object obj, char value)
        throws IllegalArgumentException {
        if (getType() == Character.TYPE) {
            VMReflection.setFieldValue(obj, data.id, new Character(value));
            return;
        }
        setIntNoObjectCheck(obj, value);
    }

    /**
     * Sets double value with out obj checking 
     * 
     * @param obj object
     * @param value new value
     * @throws IllegalArgumentException if value is not valid
     */
    private void setDoubleNoObjectCheck(Object obj, double value)
        throws IllegalArgumentException {
        if (getType() == Double.TYPE) {
            VMReflection.setFieldValue(obj, data.id, new Double(value));
            return;
        }
        throw new IllegalArgumentException(
            "The specified value can not be converted to a \""
                + getType().getName()
                + "\" type by a primitive widening conversion");
    }

    /**
     * Sets float value with out obj checking 
     * 
     * @param obj object
     * @param value new value
     * @throws IllegalArgumentException if value is not valid
     */
    private void setFloatNoObjectCheck(Object obj, float value)
        throws IllegalArgumentException {
        if (getType() == Float.TYPE) {
            VMReflection.setFieldValue(obj, data.id, new Float(value));
            return;
        }
        setDoubleNoObjectCheck(obj, value);
    }

    /**
     * Sets int value with out obj checking 
     * 
     * @param obj object
     * @param value new value
     * @throws IllegalArgumentException if value is not valid
     */
    private void setIntNoObjectCheck(Object obj, int value)
        throws IllegalArgumentException {
        if (getType() == Integer.TYPE) {
            VMReflection.setFieldValue(obj, data.id, new Integer(value));
            return;
        }
        setLongNoObjectCheck(obj, value);
    }

    /**
     * Sets long value with out obj checking 
     * 
     * @param obj object
     * @param value new value
     * @throws IllegalArgumentException if value is not valid
     */
    private void setLongNoObjectCheck(Object obj, long value)
        throws IllegalArgumentException {
        if (getType() == Long.TYPE) {
            VMReflection.setFieldValue(obj, data.id, new Long(value));
            return;
        }
        setFloatNoObjectCheck(obj, value);
    }

    /**
     * Sets short value with out obj checking 
     * 
     * @param obj object
     * @param value new value
     * @throws IllegalArgumentException if value is not valid
     */
    private void setShortNoObjectCheck(Object obj, short value)
        throws IllegalArgumentException {
        if (getType() == Short.TYPE) {
            VMReflection.setFieldValue(obj, data.id, new Short(value));
            return;
        }
        setIntNoObjectCheck(obj, value);
    }

    /**
     * Reconstructs the signature of this field.
     * 
     * @return the signature of the field 
     */
    String getSignature() {
        //XXX: May it be more effective to realize this request via
        //API2VM interface, i.e. just to obtain the signature of the field descriptor
        //from the class file?
        if (data.type == null) {
            data.initType();
        }
        return getClassSignature(data.type);
    }

    /**
     * Keeps an information about this field
     */
    private class FieldData {

        /**
         * field handle which is used to retrieve all necessary information
         * about this field object
         */
        final Object id;

        /**
         * declaring class
         */
        Class declaringClass;

        /**
         * field modifiers
         */
        int modifiers = -1;

        /**
         * field name
         */
        String name;

        /**
         * field type
         */
        Class type;

        /**
|         * @param obj field handler
         */
        public FieldData(Object obj) {
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
         * initializes type
         */
        public synchronized void initType() {
            if (type == null) {
                type = VMReflection.getFieldType(id);
            }
        }
    }
}
