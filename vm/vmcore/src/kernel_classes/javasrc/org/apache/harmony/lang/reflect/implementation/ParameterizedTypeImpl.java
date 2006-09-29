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

package org.apache.harmony.lang.reflect.implementation;

import java.lang.reflect.GenericArrayType;
import java.lang.reflect.ParameterizedType;
import java.lang.reflect.TypeVariable;
import java.lang.reflect.WildcardType;
import java.lang.reflect.Type;

/**
 * @author Serguei S. Zapreyev
 * @version $Revision: 1.1.2.2 $
 */
public final class ParameterizedTypeImpl implements ParameterizedType {
    private final Type[] args;
    private final Type rawType;
    private final Type typeOwner;

    public ParameterizedTypeImpl(Type[] args, Type rawType, Type typeOwner) {
        this.args = args;
        this.rawType = rawType;
        this.typeOwner = typeOwner;
    }
    
    public boolean equals(Object other) {
        Type[] arr;
        if (other == null || !(other instanceof ParameterizedType) || args.length != (arr = ((ParameterizedType)other).getActualTypeArguments()).length) {
            return false;
        }
        for (int i = 0; i < args.length; i++) {
            if (!args[i].equals(arr[i])) {
                return false;
            }
        }
        return rawType.equals(((ParameterizedType)other).getRawType()) && typeOwner.equals(((ParameterizedType)other).getOwnerType());
    }

    public Type[] getActualTypeArguments() {
        return (Type[])args.clone();
    }

    public Type getOwnerType() {
        return typeOwner;
    }

    public Type getRawType() {
        return rawType;
    }

    public int hashCode() {
        //return super.hashCode();
        int ah = 0; 
        for(int i = 0; i < args.length; i++) {
            ah += args[i].hashCode();
        }
        return ah ^ rawType.hashCode() ^ typeOwner.hashCode();
    }
    
    public String toString() {
        // TODO: this body should be reimplemented effectively.
        StringBuffer sb = new StringBuffer();
        if (typeOwner!=null) {
            sb.append((typeOwner instanceof Class ? ((Class)typeOwner).getName() : typeOwner.toString())+"."+((Class)getRawType()).getSimpleName());
        } else {
            sb.append(((Class)getRawType()).getName());
        }
        if (args.length > 0) {
            sb.append("<");
            for (int i = 0; i < args.length; i++) {
                if (i != 0) {
                    sb.append(", ");
                }
                if (args[i] instanceof Class) {
                    sb.append(((Class)args[i]).getName());
                } else if (args[i] instanceof ParameterizedType) {
                    sb.append(args[i].toString());
                } else if (args[i] instanceof TypeVariable) {
                    sb.append(args[i].toString());
                } else if (args[i] instanceof WildcardType) {
                    sb.append(args[i].toString());
                } else if (args[i] instanceof GenericArrayType) {
                    sb.append(args[i].toString());
                }
            }
            sb.append(">");
        }
        return sb.toString();
    }
}