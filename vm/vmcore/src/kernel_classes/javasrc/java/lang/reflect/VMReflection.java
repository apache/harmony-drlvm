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
 * @version $Revision: 1.1.2.1.4.4 $
 */

package java.lang.reflect;

/**
 * Provides the package private methods requirered for the
 * <code>java.lang.reflect</code> package implementation.
 * <p>
 * This class must be implemented according to the common policy for porting
 * interfaces - see the porting interface overview for more detailes.
 * <p>
 * <b>Note: </b> this class design is based on requirements for the
 * {@link Field}, {@link Method} and {@link Constructor} classes from the
 * <code>java.lang.reflect</code> package. Each class (Field, Method, Constructor)
 * should have a constructor that accepts an argument of the
 * {@link java.lang.Object} type. This argument serves as an identifier of
 * particular item. This identifier should be used later when one needs to 
 * operate with an item. For example, see {@link VMReflection#getFieldType(Object)
 * VMReflection.getFieldType(Object id)} method.
 * @api2vm
 */
 
final class VMReflection {

    /**
     * This class is not supposed to be instantiated.
     */
    private VMReflection() {
    }

    /**
     * This method satisfies the requirements of the specification for the
     * {@link Member#getDeclaringClass() Member.getDeclaringClass()} method. But
     * it takes one additional id parameter.
     * 
     * @param id an identifier of the caller class.
     * @api2vm
     */
    static native Class getDeclaringClass(Object id);

    /**
     * This method satisfies the requirements of the specification for the
     * {@link Method#getExceptionTypes() Method.getExceptionTypes()} method. But
     * it takes one additional id parameter.
     * 
     * @param id an identifier of the caller class.
     * @api2vm
     */
    static native Class[] getExceptionTypes(Object id);

    /**
     * This method satisfies the requirements of the specification for the
     * {@link Field#getType() Field.getType()} method. But it takes one
     * additional id parameter.
     * 
     * @param id an identifier of the caller class.
     * @api2vm
     */
    static native Class getFieldType(Object id);

    /**
     * Obtaines a value of the field with specified identifier. If the
     * <code>id</code> argument corresponds to a static field then the
     * <code>object</code> argument must be null. The value of a static field
     * will be returned in this case. If the <code>id</code> argument
     * corresponds to non-static field then object's field value will be
     * returned.
     * <p>
     * This method is used for the {@link Field#get(Object) Field.get(Object obj)}
     * method implementation.
     * <p>
     * <b>Note:</b> Under design yet. Subjected to change.
     * 
     * @param object the object to get a field value from.
     * @param id an identifier of the caller class.
     * @return a value of the object. The values of primitive type are wrapped
     *         by corresponding object from the <code>java.lang</code> package.
     * @throws ExceptionInInitializerError if initialization fails.
     * @api2vm
     */
    static native Object getFieldValue(Object object, Object id);

    /**
     * This method satisfies the requirements of the specification for the
     * {@link Method#getReturnType() Method.getReturnType()} method. But it
     * takes one additional id parameter.
     * 
     * @param id an identifier of the caller class.
     * @api2vm
     */
    static native Class getMethodReturnType(Object id);

    /**
     * This method satisfies the requirements of the specification for the
     * {@link Member#getModifiers() Member.getModifiers()} method. But it takes
     * one additional id parameter.
     * 
     * @param id an identifier of the caller class.
     * @api2vm
     */
    static native int getModifiers(Object id);

    /**
     * This method satisfies the requirements of the specification for the
     * {@link Member#getName() Member.getName()} method. But it takes one
     * additional id parameter.
     * 
     * @param id an identifier of the caller class.
     * @api2vm
     */
    static native String getName(Object id);

    /**
     * This method satisfies the requirements of the specification for the
     * {@link Method#getParameterTypes() Method.getParameterTypes()} method. But
     * it takes one additional id parameter.
     * 
     * @param id an identifier of the caller class.
     * @api2vm
     */
    static native Class[] getParameterTypes(Object id);

    /**
     * This method satisfies the requirements of the specification for the
     * {@link Method#invoke(java.lang.Object, java.lang.Object[])
     * Method.invoke(Object obj, Object[] args)} method. But it differs in
     * several ways.
     * <p>
     * First, it takes one additional <code>id</code> parameter. This parameter
     * is used as an identifier to invoke corresponding method.
     * <p>
     * Second, it doesn't perform access control so it doesn't throw an
     * <code>IllegalAccessException</code> exception. 
     * <p>
     * Third, it throws <code>IllegalArgumentException</code> only if the
     * <code>args</code> argument doesn't fit to the actual method parameters.  
     * <p>
     * Last, it doesn't throw an <code>NullPointerException</code> exception. If
     * the <code>id</code> argument corresponds to a static method then the
     * object argument must be null. An attempt to invoke a static method will
     * be made in this case. If the <code>id</code> argument corresponds to a
     * non-static method then corresponding object's method will be invoked.
     * <p>
     * <b>Note:</b> Under design yet. Subjected to change.
     * 
     * @param id the identifier of the method to be invoked.
     * @api2vm
     */
    static native Object invokeMethod(Object object, Object id, Object[] args)
        throws InvocationTargetException;

    /**
     * This method satisfies the requirements of the specification for the
     * {@link Array#newInstance(Class, int[])
     * Array.newInstance(Class componentType, int[] dimensions)} method. But it
     * differs in several ways.
     * <p>
     * First, it it doesn't throw an <code>NullPointerException</code> exception.
     * <p>
     * Second, it throws an <code>IllegalArgumentException</code> exception only
     * if the implementation doesn't support specified number of dimensions.
     * <p>
     * <b>Note:</b> Under design yet. Subjected to change.
     * @api2vm
     */
    static native Object newArrayInstance(Class type, int[] dimensions);

    /**
     * This method satisfies the requirements of the specification for the
     * {@link Constructor#newInstance(java.lang.Object[])
     * Constructor.newInstance(java.lang.Object[] initargs)} method. But it
     * differs in several ways.
     * <p>
     * First, it takes one additional <code>id</code> parameter. This parameter
     * is used as an identifier of the corresponding constructor.
     * <p>
     * Second, it doesn't perform access control so it doesn't throw an
     * <code>IllegalAccessException</code> exception.
     * <p>
     * Last, it doesn't throw an <code>InstantiationException</code> exception. 
     * The <code>id</code> argument must not correspond to an abstract class.
     * <p>
     * <b>Note:</b> Under design yet. Subjected to change.
     * 
     * @param id the identifier of the method to be invoked.
     * @api2vm
     */
    static native Object newClassInstance(Object id, Object[] args)
        throws InvocationTargetException;

    /**
     * Sets a value for the field with specified identifier. If the
     * <code>id</code> argument corresponds to a static field then the
     * <code>object</code> argument must be null. An attempt to set a new value
     * to a static field will be made in this case. If the <code>id</code>
     * argument corresponds to a non-static field then an attempt to assign new 
     * value to object's field will be made.
     * <p>
     * This method is used for the {@link Field#set(Object, Object)
     * Field.set(Object obj, Object value)} method implementation.
     * <p>
     * <b>Note:</b> Under design yet. Subjected to change.
     * 
     * @param object the object to set a field value in.
     * @param id an identifier of the caller class.
     * @param value a new field value. If the field has primitive type then the
     *        value argument should be unwrapped.
     * @throws ExceptionInInitializerError if initialization fails.
     * @api2vm
     */
    static native void setFieldValue(Object object, Object id, Object value);
}
