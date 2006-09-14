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
 * @author Evgueni V. Brevnov, Roman S. Bushmanov
 * @version $Revision$
 */
package java.lang;

import junit.framework.TestCase;

/**
 * tested class: java.lang.Class
 * tested method: isPrimitive
 */
public class ClassTestIsPrimitive extends TestCase {

    /**
     * The Float.TYPE class represents the primitive type.
     */
    public void test1() {
        assertTrue(Float.TYPE.isPrimitive());
    }
    
    /**
     * The void class represents the primitive type.
     */
    public void test2() {
        assertTrue(void.class.isPrimitive());
    }

    /**
     * checks that the Integer class does not represent a primitive type.  
     */
    public void test3() {
        assertFalse(Integer.class.isPrimitive());
    }

    /**
     * array of primitive types is not the primitive type.
     */
    public void test4() { 
        assertFalse(new int[0].getClass().isPrimitive());
    }
}
