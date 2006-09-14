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

import java.util.Arrays;
import java.util.HashSet;

import junit.framework.TestCase;

/**
 * tested class: java.lang.Class
 * tested method: getDeclaredClasses
 */
public class ClassTestGetDeclaresClasses extends TestCase {

    /**
     * The getDeclaredClasses() method must return all inner classes and
     * interfaces including protected, package private and private members.
     *  
     */
    public void test1() {
        Class[] cs = ClassTestGetDeclaresClasses.class.getDeclaredClasses();
        HashSet<Class> set = new HashSet<Class>(Arrays.asList(cs));
        assertTrue("Helper1 has not been found", set.contains(Helper1.class));
        assertTrue("Helper2 has not been found", set.contains(Helper2.class));
        assertTrue("Helper3 has not been found", set.contains(Helper3.class));
        assertTrue("Helper4 has not been found", set.contains(Helper4.class));
        assertFalse("Helper1.Inner1 has not been found",
                    set.contains(Helper1.Inner1.class));
    }

    /**
     * The members declared in the super class must not be reflected by
     * getDeclaredClasses() method.
     *  
     */
    public void test2() {
        Class[] cs = Helper3.class.getDeclaredClasses();
        assertNotNull("null expected", cs);
        assertEquals("array length:", 1, cs.length);
        assertSame("objects differ", Helper3.Inner2.class, cs[0]);
    }

    /**
     * An empty array must be returned for the classes that represent
     * primitive types.
     *  
     */
    public void test3() {
        Class[] cs = Void.TYPE.getDeclaredClasses();
        assertNotNull("null expected", cs);
        assertEquals("array length:", 0, cs.length);
    }

    /**
     * An empty array must be returned for classes that represent arrays.
     *  
     */
    public void test4() {
        Class[] cs = new Object[0].getClass().getDeclaredClasses();
        assertNotNull("null expected", cs);
        assertEquals("array length:", 0, cs.length);
    }

    public class Helper1 {
        class Inner1 {
        }
    }

    protected interface Helper2 {
    }

    class Helper3 extends Helper1 {
        class Inner2 {
        }
    }

    private class Helper4 {
    }
}