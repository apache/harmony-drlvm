/*
 *  Licensed to the Apache Software Foundation (ASF) under one or more
 *  contributor license agreements.  See the NOTICE file distributed with
 *  this work for additional information regarding copyright ownership.
 *  The ASF licenses this file to You under the Apache License, Version 2.0
 *  (the "License"); you may not use this file except in compliance with
 *  the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */
package org.apache.harmony.lang.reflect.support;

import java.security.AccessController;

/**
 * @author Serguei S. Zapreyev
 * @version $Revision: 1.1.2.1 $
 */

/**
 * Loader provides access to some of finding.
 * 
 * (This should be considered as a temporary decision. A correct approach
 * in using loader facilities should be implemented later.)
 */
public final class AuxiliaryLoader extends ClassLoader {
    public static final AuxiliaryLoader ersatzLoader = new AuxiliaryLoader();

    public Class<?> findClass(final String classTypeName)
            throws ClassNotFoundException {
        if (classTypeName.equals("byte")) {
            return byte.class;
        } else if (classTypeName.equals("char")) {
            return char.class;
        } else if (classTypeName.equals("double")) {
            return double.class;
        } else if (classTypeName.equals("float")) {
            return float.class;
        } else if (classTypeName.equals("int")) {
            return int.class;
        } else if (classTypeName.equals("long")) {
            return long.class;
        } else if (classTypeName.equals("short")) {
            return short.class;
        } else if (classTypeName.equals("boolean")) {
            return boolean.class;
        } else if (classTypeName.equals("void")) {
            return void.class;
        }
        ClassLoader cl = this.getClass().getClassLoader();
        if (cl == null) {
            cl = ClassLoader.getSystemClassLoader();
        }
        try {
            return cl.loadClass(classTypeName);
        } catch (Throwable _) {
            Class c = (Class) AccessController
                    .doPrivileged(new java.security.PrivilegedAction<Object>() {
                        public Object run() {
                            // based on an empiric knowledge
                            ClassLoader cl = ClassLoader.getSystemClassLoader();
                            try {
                                java.lang.reflect.Method[] ms = cl.getClass()
                                        .getDeclaredMethods();
                                int i = 0;
                                for (; i < ms.length; i++) {
                                    if (ms[i].getName().equals("loadClass")
                                            && ms[i].getParameterTypes().length == 2
                                            && ms[i].getParameterTypes()[0]
                                                    .getName().equals(
                                                            "java.lang.String")
                                            && ms[i].getParameterTypes()[1]
                                                    .getName()
                                                    .equals("boolean")) {
                                        break;
                                    }
                                }
                                ms[i].setAccessible(true);
                                return (Object) ms[i]
                                        .invoke(
                                                (Object) cl,
                                                new Object[] {
                                                        (Object) AuxiliaryFinder
                                                                .transform(classTypeName),
                                                        new Boolean(false) });
                            } catch (java.lang.IllegalAccessException e) {
                                System.err
                                        .println("Error: AuxiliaryLoader.findClass("
                                                + classTypeName
                                                + "): "
                                                + e.toString());
                                e.printStackTrace();
                            } catch (java.lang.reflect.InvocationTargetException e) {
                                System.err
                                        .println("Error: AuxiliaryLoader.findClass("
                                                + classTypeName
                                                + "): "
                                                + e.getTargetException()
                                                        .toString());
                                e.getTargetException().printStackTrace();
                            } catch (Exception e) {
                                System.err
                                        .println("Error: AuxiliaryLoader.findClass("
                                                + classTypeName
                                                + "): "
                                                + e.toString());
                                e.printStackTrace();
                            }
                            return null;
                        }
                    });
            if (c == null)
                throw new ClassNotFoundException(classTypeName);
            return c;
        }
    }

    public void resolve(final Class c) {
        AccessController.doPrivileged(new java.security.PrivilegedAction<Object>() {
            public Object run() {
                ClassLoader cl = AuxiliaryLoader.this.getClass().getClassLoader();
                if (cl == null) {
                    cl = ClassLoader.getSystemClassLoader();
                }
                try {
                    java.lang.reflect.Method[] ms = cl.getClass()
                            .getDeclaredMethods();
                    int i = 0;
                    for (; i < ms.length; i++) {
                        if (ms[i].getName().equals("loadClass")) {
                            break;
                        }
                    }
                    ms[i].setAccessible(true);
                    ms[i].invoke((Object) cl, new Object[] {
                            (Object) c.getCanonicalName(), (Object) true });
                } catch (java.lang.IllegalAccessException _) {
                } catch (java.lang.reflect.InvocationTargetException _) {
                }
                return null;
            }
        });
    }
}
