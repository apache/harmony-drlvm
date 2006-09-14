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
package org.apache.harmony.lang.annotation;

import java.lang.annotation.Annotation;
import java.lang.reflect.Constructor;
import java.lang.reflect.Field;
import java.lang.reflect.Method;

import org.apache.harmony.test.TestResources;

import junit.framework.TestCase;

/**
 * Test verifies that correct classloader is used to reflect
 * annotations.
 * 
 * @author Alexey V. Varlamov
 * @version $Revision$
 */
public class AnnotationLoaderTest extends TestCase {

    public static void main(String[] args) {
        junit.textui.TestRunner.run(AnnotationLoaderTest.class);
    }
    
    protected ClassLoader ld;
    protected Class<?> test;
    
    @Override
    protected void setUp() throws Exception {
        ld = TestResources.getLoader();
        test = ld.loadClass("org.apache.harmony.lang.test.resource.AnnotatedMembers");
    }
    
    /**
     * Tests that the defining classloader is used to lookup class annotations.  
     */
    public void testClass() throws Throwable {
        Annotation[] an = test.getAnnotations();
        assertNotNull(an);
        assertEquals("annotations num", 1, an.length);
        assertEquals("the class annotation", "AnotherAntn", an[0].annotationType().getSimpleName());
    }

    /**
     * Tests that the defining classloader is used to lookup package annotations.  
     */
    public void testPackage() throws Throwable {
        Package p = test.getPackage();
        assertNotNull("package", p);
        Annotation[] an = p.getAnnotations();
        assertNotNull(an);
        assertEquals("annotations num", 1, an.length);
        assertEquals("the package annotation", "AnotherAntn", an[0].annotationType().getSimpleName());
    }
    
    /**
     * Tests that the defining classloader is used to lookup annotations 
     * of fields of a class.  
     */
    public void testField() throws Throwable {
        Field f = test.getField("foo");
        assertNotNull("field", f);
        Annotation[] an = f.getAnnotations();
        assertNotNull("annotations", an);
        assertEquals("annotations num", 1, an.length);
        assertEquals("the class annotation", "AnotherAntn", an[0].annotationType().getSimpleName());
    }

    /**
     * Tests that the defining classloader is used to lookup annotations 
     * of methods of a class.  
     */
    public void testMethod() throws Throwable {
        Method m = test.getMethod("bar");
        assertNotNull("method", m);
        Annotation[] an = m.getAnnotations();
        assertNotNull("annotations", an);
        assertEquals("annotations num", 1, an.length);
        assertEquals("the class annotation", "AnotherAntn", an[0].annotationType().getSimpleName());
    }
    
    /**
     * Tests that the defining classloader is used to lookup annotations 
     * of constructors of a class.  
     */
    public void testCtor() throws Throwable {
        Constructor ctor = test.getConstructor();
        assertNotNull("ctor", ctor);
        Annotation[] an = ctor.getAnnotations();
        assertNotNull("annotations", an);
        assertEquals("annotations num", 1, an.length);
        assertEquals("the class annotation", "AnotherAntn", an[0].annotationType().getSimpleName());
    }

    /**
     * Tests that the defining classloader is used to lookup parameter annotations 
     * of class's methods.  
     */
    public void testParam() throws Throwable {
        Method m = test.getMethod("buz", String.class);
        assertNotNull("method", m);
        Annotation[][] an = m.getParameterAnnotations();
        assertNotNull("annotations", an);
        assertEquals("param num", 1, an.length);
        assertEquals("annotations num", 1, an[0].length);
        assertEquals("the class annotation", "AnotherAntn", an[0][0].annotationType().getSimpleName());
    }
}
