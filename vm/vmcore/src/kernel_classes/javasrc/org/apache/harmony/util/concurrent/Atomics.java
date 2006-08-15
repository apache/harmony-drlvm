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
 * @author Artem A. Aliev, Andrey Y. Chernyshev, Sergey V. Dmitriev
 * @version $Revision: 1.1.6.3 $
 */
package org.apache.harmony.util.concurrent;

import java.lang.reflect.Field;

/**
 * Allows to atomically update the contents of fields for the specific object. The primary purpose
 * of this class is to provide the low-level atomic field access operations useful for
 * implementing the classes from java.util.concurrent.atomic package.
 *
 * @see java.util.concurrent.atomic
 */
public final class Atomics {

    private Atomics() {};

     /**
      * Returns offset of the given field.
      * @param field the field for which offset is returned
      *
      * @return offset of the given field
      */
    public static native long getFieldOffset(Field field);

    /**
     * Atomically sets an integer field to x if it currently contains the expected value.
     * @param o object those integer field needs to be set
     * @param field the field to be set
     * @param expected expected field value
     * @param x value to set.
     *
     * @return true if the value was set.
     * False return indicates that the actual value was not equal to the expected value.
     */
    public static native boolean compareAndSetInt(Object o, long offset, int expected, int x);

    /**
     * Atomically sets a boolean field to x if it currently contains the expected value.
     * @param o object those boolean field needs to be set
     * @param field the field to be set
     * @param expected expected field value
     * @param x value to set.
     *
     * @return true if the value was set.
     * False return indicates that the actual value was not equal to the expected value.
     */
    public static native boolean compareAndSetBoolean(Object o, long offset, boolean expected, boolean x);


    /**
     * Atomically sets a long field to x if it currently contains the expected value.
     * @param o object those long field needs to be set
     * @param field the field to be set
     * @param expected expected field value
     * @param x value to set.
     *
     * @return true if the value was set.
     * False return indicates that the actual value was not equal to the expected value.
     */
    public static native boolean compareAndSetLong(Object o, long offset, long expected, long x);


    /**
     * Atomically sets a reference type field to x if it currently contains the expected value.
     * @param o object those reference type field needs to be set
     * @param field the field to be set
     * @param expected expected field value
     * @param x value to set.
     *
     * @return true if the value was set.
     * False return indicates that the actual value was not equal to the expected value.
     */
    public static native boolean compareAndSetObject(Object o, long offset, Object expected, Object x);


    /**
     * Atomically sets an element within array of integers to x if it currently contains the expected value.
     * @param arr array those integer element needs to be set
     * @param index an index within an array
     * @param expected expected field value
     * @param x value to set.
     *
     * @return true if the value was set.
     * False return indicates that the actual value was not equal to the expected value.
     */
    public static native boolean compareAndSetInt(int[] arr, int index, int expected, int x);


    /**
     * Atomically sets an element within array of booleans to x if it currently contains the expected value.
     * @param arr array those boolean element needs to be set
     * @param index an index within an array
     * @param expected expected field value
     * @param x value to set.
     *
     * @return true if the value was set.
     * False return indicates that the actual value was not equal to the expected value.
     */
    public static native boolean compareAndSetBoolean(boolean[] arr, int index, boolean expected, boolean x);


    /**
     * Atomically sets an element within array of longs to x if it currently contains the expected value.
     * @param arr array those long element needs to be set
     * @param index an index within an array
     * @param expected expected field value
     * @param x value to set.
     *
     * @return true if the value was set.
     * False return indicates that the actual value was not equal to the expected value.
     */
    public static native boolean compareAndSetLong(long[] arr, int index, long expected, long x);


    /**
     * Atomically sets an element within array of objects to x if it currently contains the expected value.
     * @param arr array those object element needs to be set
     * @param index an index within an array
     * @param expected expected field value
     * @param x value to set.
     *
     * @return true if the value was set.
     * False return indicates that the actual value was not equal to the expected value.
     */
    public static native boolean compareAndSetObject(Object[] arr, int index, Object expected, Object x);
}
