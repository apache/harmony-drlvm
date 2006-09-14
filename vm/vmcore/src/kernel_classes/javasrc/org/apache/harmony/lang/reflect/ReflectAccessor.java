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
package org.apache.harmony.lang.reflect;

import java.lang.reflect.Constructor;
import java.lang.reflect.Field;
import java.lang.reflect.Method;

/**
 * @author Evgueni Brevnov, Roman S. Bushmanov
 * @version $Revision: 1.1.6.4 $
 */
public interface ReflectAccessor {

    public <T> Constructor<T> copyConstructor(Constructor<T> c);

    public Field copyField(Field f);

    public Method copyMethod(Method m);

    public void checkMemberAccess(Class<?> callerClass, Class<?> declaringClass,
                                  Class<?> runtimeClass, int memberModifiers)
        throws IllegalAccessException;

    public Method[] mergePublicMethods(Method[] declared, 
            Method[] superPublic, Method[][] intf, int estimate);
}
