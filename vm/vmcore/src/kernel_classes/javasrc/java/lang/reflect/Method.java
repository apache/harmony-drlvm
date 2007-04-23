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
/**
 * @author Evgueni Brevnov, Serguei S. Zapreyev, Alexey V. Varlamov
 * @version $Revision: 1.1.2.2.4.4 $
 */

package java.lang.reflect;

import static org.apache.harmony.vm.ClassFormat.ACC_BRIDGE;
import static org.apache.harmony.vm.ClassFormat.ACC_SYNTHETIC;
import static org.apache.harmony.vm.ClassFormat.ACC_VARARGS;

import java.lang.annotation.Annotation;
import java.util.Arrays;

import org.apache.harmony.lang.reflect.parser.Parser;
import org.apache.harmony.lang.reflect.parser.Parser.SignatureKind;
import org.apache.harmony.lang.reflect.parser.InterimParameterizedType;
import org.apache.harmony.lang.reflect.parser.InterimTypeVariable;
import org.apache.harmony.lang.reflect.parser.InterimType;
import org.apache.harmony.lang.reflect.parser.InterimClassType;
import org.apache.harmony.lang.reflect.parser.InterimTypeParameter;
import org.apache.harmony.lang.reflect.parser.InterimGenericArrayType;
import org.apache.harmony.lang.reflect.parser.InterimMethodGenericDecl;

import org.apache.harmony.lang.reflect.repository.TypeVariableRepository;
import org.apache.harmony.lang.reflect.repository.ParameterizedTypeRepository;

import org.apache.harmony.lang.reflect.support.AuxiliaryFinder;
import org.apache.harmony.lang.reflect.support.AuxiliaryCreator;
import org.apache.harmony.lang.reflect.support.AuxiliaryChecker;
import org.apache.harmony.lang.reflect.support.AuxiliaryLoader;
import org.apache.harmony.lang.reflect.support.AuxiliaryUtil;

import org.apache.harmony.lang.reflect.implementation.TypeVariableImpl;
import org.apache.harmony.lang.reflect.implementation.ParameterizedTypeImpl;

import org.apache.harmony.vm.VMStack;
import org.apache.harmony.vm.VMGenericsAndAnnotations;

/**
* @com.intel.drl.spec_ref 
*/
public final class Method extends AccessibleObject implements Member, GenericDeclaration {

    /**
    *  @com.intel.drl.spec_ref
    */
    public boolean isBridge() {
        return (getModifiers() & ACC_BRIDGE) != 0;
    }

    /**
    *  @com.intel.drl.spec_ref
    */
    public boolean isVarArgs() {
        return (getModifiers() & ACC_VARARGS) != 0;
    }

    /**
    *  @com.intel.drl.spec_ref
    */
    public Annotation[][] getParameterAnnotations() {
        Annotation a[][] = data.getParameterAnnotations();
        Annotation aa[][] = new Annotation[a.length][]; 
        for (int i = 0; i < a.length; i++ ) {
            aa[i] = new Annotation[a[i].length];
            System.arraycopy(a[i], 0, aa[i], 0, a[i].length);
        }
        return aa;
    }

    /**
    *  @com.intel.drl.spec_ref
    */
    public Annotation[] getDeclaredAnnotations() {
        Annotation a[] = data.getDeclaredAnnotations();
        Annotation aa[] = new Annotation[a.length];
        System.arraycopy(a, 0, aa, 0, a.length);
        return aa;
    }

    /**
    *  @com.intel.drl.spec_ref
    */
    @SuppressWarnings("unchecked")
    public <A extends Annotation> A getAnnotation(Class<A> annotationClass) {
        if(annotationClass == null) {
            throw new NullPointerException();
        }
        for (Annotation aa : data.getDeclaredAnnotations()) {
            if(aa.annotationType() == annotationClass) {

                return (A) aa;
            }
        }
        return null;
    }

    /**
    *  @com.intel.drl.spec_ref
    */
    public Type[] getGenericExceptionTypes() throws GenericSignatureFormatError, TypeNotPresentException, MalformedParameterizedTypeException {
        if (data.genericExceptionTypes == null) {
            data.initGenericExceptionTypes();
        }

        return (Type[])data.genericExceptionTypes.clone();
    }

    /**
    *  @com.intel.drl.spec_ref
    */
    public Type[] getGenericParameterTypes() throws GenericSignatureFormatError, TypeNotPresentException, MalformedParameterizedTypeException {
        if (data.genericParameterTypes == null) {
            data.initGenericParameterTypes();
        }

        return (Type[])data.genericParameterTypes.clone();
    }

    /**
    *  @com.intel.drl.spec_ref
    */
    public Type getGenericReturnType() throws GenericSignatureFormatError, TypeNotPresentException, MalformedParameterizedTypeException {
        if (data.genericReturnType == null) {
            Object startPoint = this; 
            if (data.methSignature == null) {
                data.methSignature = AuxiliaryUtil.toUTF8(VMGenericsAndAnnotations.getSignature(data.vm_member_id));
                if (data.methSignature == null) {
                    data.genericReturnType = (Type)getReturnType();
                    return data.genericReturnType;
                }
            }
            if (data.methGenDecl == null) {
                data.methGenDecl =  (InterimMethodGenericDecl) Parser.parseSignature(data.methSignature, SignatureKind.METHOD_SIGNATURE, (GenericDeclaration)startPoint);
            }
            InterimType mthdType = data.methGenDecl.returnValue;
            if (mthdType instanceof InterimTypeVariable) {
                String tvName = ((InterimTypeVariable) mthdType).typeVariableName;
                TypeVariable variable = TypeVariableRepository.findTypeVariable(tvName, startPoint);
                if (variable == null) {
                    variable =  AuxiliaryFinder.findTypeVariable(tvName, startPoint);
                    if (variable == null) {
                        return (Type) null; // compatible behaviour
                    }
                }
                data.genericReturnType = (Type) variable;
                return (Type) variable;
            } else if (mthdType instanceof InterimParameterizedType) {
                ParameterizedType pType = ParameterizedTypeRepository.findParameterizedType((InterimParameterizedType) mthdType, ((InterimParameterizedType) mthdType).signature, startPoint);
                if (pType == null) {
                    try {
                        AuxiliaryFinder.findGenericClassDeclarationForParameterizedType((InterimParameterizedType) mthdType, startPoint);
                    } catch(Throwable e) {
                        throw new TypeNotPresentException(((InterimParameterizedType) mthdType).rawType.classTypeName.substring(1).replace('/', '.'), e);
                    }
                    // check the correspondence of the formal parameter number and the actual argument number:
                    AuxiliaryChecker.checkArgsNumber((InterimParameterizedType) mthdType, startPoint); // the MalformedParameterizedTypeException may raise here
                    try {
                        pType = new ParameterizedTypeImpl(AuxiliaryCreator.createTypeArgs((InterimParameterizedType) mthdType, startPoint), AuxiliaryCreator.createRawType((InterimParameterizedType) mthdType, startPoint), AuxiliaryCreator.createOwnerType((InterimParameterizedType) mthdType, startPoint));
                    } catch(ClassNotFoundException e) {
                        throw new TypeNotPresentException(e.getMessage(), e);
                    }
                    ParameterizedTypeRepository.registerParameterizedType(pType, (InterimParameterizedType) mthdType, ((InterimParameterizedType) mthdType).signature, startPoint);
                }
                data.genericReturnType = (Type) pType;
                return (Type) pType; 
            } else if (mthdType instanceof InterimGenericArrayType) {
                return AuxiliaryCreator.createGenericArrayType((InterimGenericArrayType) mthdType, startPoint); 
            } else {
                return getReturnType();
            }            
        }
        return data.genericReturnType;
    }

    /**
    *  @com.intel.drl.spec_ref
    */
    public TypeVariable<Method>[] getTypeParameters() throws GenericSignatureFormatError {
        if (data.typeParameters == null) {
            data.initTypeParameters();
        }
        return (TypeVariable<Method>[])data.typeParameters.clone();
    }

    /**
    * @com.intel.drl.spec_ref 
    */
    public String toGenericString() {
        StringBuilder sb = new StringBuilder(80);
        // data initialization
        if (data.genericParameterTypes == null) {
            data.initGenericParameterTypes();
        }
        if (data.genericExceptionTypes == null) {
            data.initGenericExceptionTypes();
        }
        // append modifiers if any
        int modifier = getModifiers();
        if (modifier != 0) {
            sb.append(Modifier.toString(modifier & ~(ACC_BRIDGE + ACC_VARARGS))).append(' ');
        }
        // append type parameters
        if (data.typeParameters != null && data.typeParameters.length > 0) {
            sb.append('<');
            for (int i = 0; i < data.typeParameters.length; i++) {
                appendGenericType(sb, data.typeParameters[i]);
                if (i < data.typeParameters.length - 1) {
                    sb.append(", ");
                }
            }
            sb.append("> ");
        }
        // append return type
        appendGenericType(sb, getGenericReturnType());
        sb.append(' ');
        // append method name
        appendArrayType(sb, getDeclaringClass());
        sb.append("."+getName());
        // append parameters
        sb.append('(');
        appendArrayGenericType(sb, data.genericParameterTypes);
        sb.append(')');
        // append exeptions if any
        if (data.genericExceptionTypes.length > 0) {
            sb.append(" throws ");
            appendArrayGenericType(sb, data.genericExceptionTypes);
        }
        return sb.toString();
    }

    /**
    * @com.intel.drl.spec_ref 
    */
    public boolean isSynthetic() {
        return (getModifiers() & ACC_SYNTHETIC) != 0;
    }   

    /**
    * @com.intel.drl.spec_ref 
    */
    public Object getDefaultValue() {
        return VMGenericsAndAnnotations.getDefaultValue(data.vm_member_id);
    }   

    /**
     * cache of the method data
     */
    private final MethodData data;

    /**
     * Copy constructor
     * 
     * @param m original method
     */
    Method(Method m) {
        data = m.data;
        isAccessible = m.isAccessible;
    }
    
    /**
     * Only VM should call this constructor.
     * String parameters must be interned.
     * @api2vm
     */
    Method(long id, Class clss, String name, String desc, int m) {
        data = new MethodData(id, clss, name, desc, m);
    }

    /**
     * Called by VM to obtain this method's handle.
     * 
     * @return handle for this method
     * @api2vm
     */
    long getId() {
        return data.vm_member_id;
    }

    /**
     * @com.intel.drl.spec_ref 
     */
    public boolean equals(Object obj) {
        if (obj instanceof Method) {
            Method another = (Method)obj;
            if (data.vm_member_id == another.data.vm_member_id){
                assert getDeclaringClass() == another.getDeclaringClass()
                && getName() == another.getName()
                && getReturnType() == another.getReturnType()
                && Arrays.equals(getParameterTypes(), another.getParameterTypes());
                return true;
            }
        }
        return false;
    }

    /**
     * @com.intel.drl.spec_ref 
     */
    public Class<?> getDeclaringClass() {
        return data.declaringClass;
    }

    /**
     * @com.intel.drl.spec_ref 
     */
    public Class<?>[] getExceptionTypes() {
        return (Class[])data.getExceptionTypes().clone();
    }

    /**
     * @com.intel.drl.spec_ref 
     */
    public int getModifiers() {
        return data.modifiers;
    }

    /**
     * @com.intel.drl.spec_ref 
     */
    public String getName() {
        return data.name;
    }

    /**
     * @com.intel.drl.spec_ref 
     */
    public Class<?>[] getParameterTypes() {
        return (Class[])data.getParameterTypes().clone();
    }

    /**
     * @com.intel.drl.spec_ref 
     */
    public Class<?> getReturnType() {
        return data.getReturnType();
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
    public Object invoke(Object obj, Object... args)
        throws IllegalAccessException, IllegalArgumentException,
        InvocationTargetException {
    	
        obj = checkObject(getDeclaringClass(), getModifiers(), obj);
        
        // check parameter validity
        checkInvokationArguments(data.getParameterTypes(), args);
        
        if (!isAccessible) {
            reflectExporter.checkMemberAccess(
                VMStack.getCallerClass(0), getDeclaringClass(),
                obj == null ? getDeclaringClass() : obj.getClass(),
                getModifiers()
            );
        }
        return VMReflection.invokeMethod(data.vm_member_id, obj, args);
    }

    /**
     * @com.intel.drl.spec_ref 
     */
    public String toString() {
        StringBuilder sb = new StringBuilder();
        // append modifiers if any
        int modifier = getModifiers();
        if (modifier != 0) {
            // BRIDGE & VARARGS recognized incorrectly
            final int MASK = ~(ACC_BRIDGE + ACC_VARARGS);
            sb.append(Modifier.toString(modifier & MASK)).append(' ');            
        }
        // append return type
        appendArrayType(sb, getReturnType());
        sb.append(' ');
        // append full method name
        sb.append(getDeclaringClass().getName()).append('.').append(getName());
        // append parameters
        sb.append('(');
        appendArrayType(sb, data.getParameterTypes());
        sb.append(')');
        // append exeptions if any
        Class[] exn = data.getExceptionTypes(); 
        if (exn.length > 0) {
            sb.append(" throws ");
            appendSimpleType(sb, exn);
        }
        return sb.toString();
    }

    /* NON API SECTION */

    /**
     * This method is required by serialization mechanism.
     * 
     * @return the signature of the method 
     */
    String getSignature() {
        return data.descriptor;
    }

    /**
     * Keeps an information about this method
     */
    private class MethodData {

        /**
         * method handle which is used to retrieve all necessary information
         * about this method object
         */
        final long vm_member_id;

        Annotation[] declaredAnnotations;

        final Class<?> declaringClass;

        private Class<?>[] exceptionTypes;

        Type[] genericExceptionTypes;

        Type[] genericParameterTypes;

        Type genericReturnType;

        String methSignature;

        /**
         * method generic declaration
         */
        InterimMethodGenericDecl methGenDecl;

        final int modifiers;

        final String name;
        
        final String descriptor;

        /**
         * declared method annotations
         */
        Annotation[][] parameterAnnotations;

        /**
         * method parameters
         */
        Class<?>[] parameterTypes;

        /** 
         * method return type
         */
        private Class<?> returnType;

        /**
         * method type parameters
         */
        TypeVariable<Method>[] typeParameters;

        /**
         * @param obj method handler
         */
        public MethodData(long vm_id, Class clss, String name, String desc, int mods) {
            vm_member_id = vm_id;
            declaringClass = clss;
            this.name = name;
            modifiers = mods;
            descriptor = desc;
        }
        
        public Annotation[] getDeclaredAnnotations() {
            if (declaredAnnotations == null) {
                declaredAnnotations = VMGenericsAndAnnotations
                    .getDeclaredAnnotations(vm_member_id);
            }
            return declaredAnnotations;
        }
        
        /**
         * initializes exeptions
         */
        public Class<?>[] getExceptionTypes() {
            if (exceptionTypes == null) {
                exceptionTypes = VMReflection.getExceptionTypes(vm_member_id);
            }
            return exceptionTypes;
        }

        /**
         * initializes generalized exeptions
         */
        public synchronized void initGenericExceptionTypes() {
            // So, here it can be ParameterizedType or TypeVariable or ordinary reference class type elements.
            if (genericExceptionTypes == null) {
                Object startPoint = Method.this;
                if (methSignature == null) {
                    methSignature = AuxiliaryUtil.toUTF8(VMGenericsAndAnnotations.getSignature(vm_member_id)); // getting this method signature
                    if (methSignature == null) {
                        genericExceptionTypes = getExceptionTypes();
                        return;
                    }
                }
                if (methGenDecl == null) {
                    methGenDecl =  (InterimMethodGenericDecl) Parser.parseSignature(methSignature, SignatureKind.METHOD_SIGNATURE, (GenericDeclaration)startPoint); // GenericSignatureFormatError can be thrown here
                }
                InterimType[] throwns = methGenDecl.throwns;
                if (throwns == null) {
                    genericExceptionTypes = getExceptionTypes();
                    return;
                }
                int l = throwns.length;
                genericExceptionTypes = new Type[l];
                for (int i = 0; i < l; i++) {
                    if (throwns[i] instanceof InterimParameterizedType) {
                        ParameterizedType pType = ParameterizedTypeRepository.findParameterizedType((InterimParameterizedType) throwns[i], ((InterimParameterizedType) throwns[i]).signature, startPoint);
                        if (pType == null) {
                            try {
                                AuxiliaryFinder.findGenericClassDeclarationForParameterizedType((InterimParameterizedType) throwns[i], startPoint);
                            } catch(Throwable e) {
                                throw new TypeNotPresentException(((InterimParameterizedType) throwns[i]).rawType.classTypeName.substring(1).replace('/', '.'), e);
                            }
                            // check the correspondence of the formal parameter number and the actual argument number:
                            AuxiliaryChecker.checkArgsNumber((InterimParameterizedType) throwns[i], startPoint); // the MalformedParameterizedTypeException may raise here
                            try {
                                pType = new ParameterizedTypeImpl(AuxiliaryCreator.createTypeArgs((InterimParameterizedType) throwns[i], startPoint), AuxiliaryCreator.createRawType((InterimParameterizedType) throwns[i], startPoint), AuxiliaryCreator.createOwnerType((InterimParameterizedType) throwns[i], startPoint));
                            } catch(ClassNotFoundException e) {
                                throw new TypeNotPresentException(e.getMessage(), e);
                            }
                            ParameterizedTypeRepository.registerParameterizedType(pType, (InterimParameterizedType) throwns[i], ((InterimParameterizedType) throwns[i]).signature, startPoint);
                        }
                        genericExceptionTypes[i] = (Type) pType; 
                    } else if (throwns[i] instanceof InterimClassType) {
                        try {
                            genericExceptionTypes[i] = (Type) AuxiliaryLoader.ersatzLoader.findClass(((InterimClassType)throwns[i]).classTypeName.substring((((InterimClassType)throwns[i]).classTypeName.charAt(0)=='L'? 1 : 0)).replace('/', '.')); // XXX: should we propagate the class loader of initial user's request (Field.getGenericType()) or use this one?
                        } catch (ClassNotFoundException e) {
                            throw new TypeNotPresentException(((InterimClassType)throwns[i]).classTypeName.substring((((InterimClassType)throwns[i]).classTypeName.charAt(0)=='L'? 1 : 0)).replace('/', '.'), e);
                        } catch (ExceptionInInitializerError e) {
                        } catch (LinkageError e) {
                        }
                    } else if (throwns[i] instanceof InterimTypeVariable) {
                        String tvName = ((InterimTypeVariable) throwns[i]).typeVariableName;
                        TypeVariable variable = TypeVariableRepository.findTypeVariable(tvName, startPoint);
                        if (variable == null) {
                            variable =  AuxiliaryFinder.findTypeVariable(tvName, startPoint);
                            if (variable == null) {
                                genericExceptionTypes[i] = (Type) null;
                                break;
                            }
                        }
                        genericExceptionTypes[i] = (Type) variable;
                    } else {
                        // Internal Error
                    }
                }
            }
        }

        /**
         * initializes generalized parameters
         */
        public synchronized void initGenericParameterTypes() {
            // So, here it can be ParameterizedType or TypeVariable or ordinary reference class type elements.
            if (genericParameterTypes == null) {
                Object startPoint = Method.this;
                if (methSignature == null) {
                    methSignature = AuxiliaryUtil.toUTF8(VMGenericsAndAnnotations.getSignature(vm_member_id)); // getting this method signature
                    if (methSignature == null) {
                        genericParameterTypes = getParameterTypes();
                        return;
                    }
                }
                if (methGenDecl == null) {
                    methGenDecl =  (InterimMethodGenericDecl) Parser.parseSignature(methSignature, SignatureKind.METHOD_SIGNATURE, (GenericDeclaration)startPoint); // GenericSignatureFormatError can be thrown here
                }
                InterimType[] methodParameters = methGenDecl.methodParameters;
                if (methodParameters == null) {
                    genericParameterTypes = new Type[0];
                    return;
                }
                int l = methodParameters.length;
                genericParameterTypes = new Type[l];
                for (int i = 0; i < l; i++) {
                    if (methodParameters[i] instanceof InterimParameterizedType) {
                        ParameterizedType pType = ParameterizedTypeRepository.findParameterizedType((InterimParameterizedType) methodParameters[i], ((InterimParameterizedType) methodParameters[i]).signature, startPoint);
                        if (pType == null) {
                            try {
                                AuxiliaryFinder.findGenericClassDeclarationForParameterizedType((InterimParameterizedType) methodParameters[i], startPoint);
                            } catch(Throwable e) {
                                throw new TypeNotPresentException(((InterimParameterizedType) methodParameters[i]).rawType.classTypeName.substring(1).replace('/', '.'), e);
                            }
                            // check the correspondence of the formal parameter number and the actual argument number:
                            AuxiliaryChecker.checkArgsNumber((InterimParameterizedType) methodParameters[i], startPoint); // the MalformedParameterizedTypeException may raise here
                            try {
                                pType = new ParameterizedTypeImpl(AuxiliaryCreator.createTypeArgs((InterimParameterizedType) methodParameters[i], startPoint), AuxiliaryCreator.createRawType((InterimParameterizedType) methodParameters[i], startPoint), AuxiliaryCreator.createOwnerType((InterimParameterizedType) methodParameters[i], startPoint));
                            } catch(ClassNotFoundException e) {
                                throw new TypeNotPresentException(e.getMessage(), e);
                            }
                            ParameterizedTypeRepository.registerParameterizedType(pType, (InterimParameterizedType) methodParameters[i], ((InterimParameterizedType) methodParameters[i]).signature, startPoint);
                        }
                        genericParameterTypes[i] = (Type) pType; 
                    } else if (methodParameters[i] instanceof InterimClassType) {
                        try {
                            genericParameterTypes[i] = (Type) AuxiliaryLoader.ersatzLoader.findClass(((InterimClassType)methodParameters[i]).classTypeName.substring((((InterimClassType)methodParameters[i]).classTypeName.charAt(0)=='L'? 1 : 0)).replace('/', '.')); // XXX: should we propagate the class loader of initial user's request (Field.getGenericType()) or use this one?
                        } catch (ClassNotFoundException e) {
                            throw new TypeNotPresentException(((InterimClassType)methodParameters[i]).classTypeName.substring((((InterimClassType)methodParameters[i]).classTypeName.charAt(0)=='L'? 1 : 0)).replace('/', '.'), e);
                        } catch (ExceptionInInitializerError e) {
                        } catch (LinkageError e) {
                        }
                    } else if (methodParameters[i] instanceof InterimTypeVariable) {
                        String tvName = ((InterimTypeVariable) methodParameters[i]).typeVariableName;
                        TypeVariable variable = TypeVariableRepository.findTypeVariable(tvName, startPoint);
                        if (variable == null) {
                            variable =  AuxiliaryFinder.findTypeVariable(tvName, startPoint);
                            if (variable == null) {
                                genericParameterTypes[i] = (Type) null;
                                continue;
                            }
                        }
                        genericParameterTypes[i] = (Type) variable;
                    } else if (methodParameters[i] instanceof InterimGenericArrayType) {
                        genericParameterTypes[i] = AuxiliaryCreator.createGenericArrayType((InterimGenericArrayType) methodParameters[i], startPoint); 
                    } else {
                        // Internal Error
                    }
                }
            }
        }


        public Annotation[][] getParameterAnnotations() {
            if (parameterAnnotations == null) {
                parameterAnnotations = VMGenericsAndAnnotations
                    .getParameterAnnotations(vm_member_id);
            }
            return parameterAnnotations;
        }

        /**
         * initializes parameters
         */
        public Class[] getParameterTypes() {
            if (parameterTypes == null) {
                parameterTypes = VMReflection.getParameterTypes(vm_member_id);
            }
            return parameterTypes;
        }

        /**
         * initializes return type
         */
        public Class<?> getReturnType() {
            if (returnType == null) {
                returnType = VMReflection.getMethodReturnType(vm_member_id);
            }
            return returnType;
        }
        
        /**
         * initializes type parameters
         */
        @SuppressWarnings("unchecked")
        public synchronized void initTypeParameters() {
            // So, here it can be only TypeVariable elements.
            if (typeParameters == null) {
                Object startPoint = Method.this;
                if (methSignature == null) {
                    methSignature = AuxiliaryUtil.toUTF8(VMGenericsAndAnnotations.getSignature(vm_member_id)); // getting this method signature
                    if (methSignature == null) {
                        typeParameters =  new TypeVariable[0];
                        return;
                    }
                }
                if (methGenDecl == null) {
                    methGenDecl =  (InterimMethodGenericDecl) Parser.parseSignature(methSignature, SignatureKind.METHOD_SIGNATURE, (GenericDeclaration)startPoint); // GenericSignatureFormatError can be thrown here
                }
                InterimTypeParameter[] pTypeParameters = methGenDecl.typeParameters;
                if (pTypeParameters == null) {
                    typeParameters =  new TypeVariable[0];
                    return;
                }
                int l = pTypeParameters.length;
                typeParameters = new TypeVariable[l];
                for (int i = 0; i < l; i++) {
                    String tvName = pTypeParameters[i].typeParameterName;
                    TypeVariable variable = new TypeVariableImpl((GenericDeclaration)Method.this, tvName, methGenDecl.typeParameters[i]);
                    TypeVariableRepository.registerTypeVariable(variable, tvName, startPoint);
                    typeParameters[i] = variable;                
                }
            }
        }
    }
}
