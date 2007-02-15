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
 * @author Intel, Mikhail Y. Fursov
 * @version $Revision: 1.36.8.4.4.4 $
 */

#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <assert.h>

#include "DrlVMInterface.h"
#include "CompilationContext.h"
#include "Log.h"
#include "JITInstanceContext.h"
#include "jit_intf.h"
#include "open/hythread_ext.h"


namespace Jitrino {

Mutex g_compileLock;

//////////////////////////////////////////////////////////////////////////////
//                          Utilities
//////////////////////////////////////////////////////////////////////////////

Boolean mtd_vars_is_managed_pointer(Method_Handle mh, unsigned idx)
{
    return false;
}; 

Boolean mtd_ret_type_is_managed_pointer(Method_Signature_Handle msh)
{
    return false;
}; 

Boolean mtd_args_is_managed_pointer(Method_Signature_Handle msh, unsigned idx)
{
    return false;
}; 

// The JIT info block is laid out as:
//    header
//    stack info
//    GC info

Byte*
methodGetStacknGCInfoBlock(Method_Handle method, JIT_Handle jit)
{
    Byte*   addr = method_get_info_block_jit(method, jit);
    addr += sizeof(void *);    // skip the header 
    return addr;
}


uint32
methodGetStacknGCInfoBlockSize(Method_Handle method, JIT_Handle jit)
{
    uint32  size = method_get_info_block_size_jit(method, jit);
    return (size - sizeof(void *));     // skip the header
}

// TODO: move both methods below to the base VMInterface level
// TODO: free TLS key on JIT deinitilization
uint32
flagTLSSuspendRequestOffset(){
    return hythread_tls_get_suspend_request_offset();
}

uint32
flagTLSThreadStateOffset() {
    static hythread_tls_key_t key = 0;
    static size_t offset = 0;
    if (key == 0) {
        hythread_tls_alloc(&key);
        offset = hythread_tls_get_offset(key);
    }
    return offset;
}

//////////////////////////////////////////////////////////////////////////////
///////////////////////// DrlVMTypeManager /////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
//
// VM specific type manager
//
void*
DrlVMTypeManager::getBuiltinValueTypeVMTypeHandle(Type::Tag type) {
    switch (type) {
    case Type::Void:    return class_get_class_of_primitive_type(VM_DATA_TYPE_VOID);
    case Type::Boolean: return class_get_class_of_primitive_type(VM_DATA_TYPE_BOOLEAN);
    case Type::Char:    return class_get_class_of_primitive_type(VM_DATA_TYPE_CHAR);
    case Type::Int8:    return class_get_class_of_primitive_type(VM_DATA_TYPE_INT8);
    case Type::Int16:   return class_get_class_of_primitive_type(VM_DATA_TYPE_INT16);
    case Type::Int32:   return class_get_class_of_primitive_type(VM_DATA_TYPE_INT32);
    case Type::Int64:   return class_get_class_of_primitive_type(VM_DATA_TYPE_INT64);
    case Type::IntPtr:  return class_get_class_of_primitive_type(VM_DATA_TYPE_INTPTR);
    case Type::UIntPtr: return class_get_class_of_primitive_type(VM_DATA_TYPE_UINTPTR);
    case Type::UInt8:   return class_get_class_of_primitive_type(VM_DATA_TYPE_UINT8);
    case Type::UInt16:  return class_get_class_of_primitive_type(VM_DATA_TYPE_UINT16);
    case Type::UInt32:  return class_get_class_of_primitive_type(VM_DATA_TYPE_UINT32);
    case Type::UInt64:  return class_get_class_of_primitive_type(VM_DATA_TYPE_UINT64);
    case Type::Single:  return class_get_class_of_primitive_type(VM_DATA_TYPE_F4);
    case Type::Double:  return class_get_class_of_primitive_type(VM_DATA_TYPE_F8);
    case Type::Float:   return NULL;
    case Type::TypedReference: assert(0);
    default:  break;
    }
    return NULL;
}

void*
DrlVMTypeManager::getSystemObjectVMTypeHandle() {
    return get_system_object_class();
}

void*
DrlVMTypeManager::getSystemClassVMTypeHandle() {
    return get_system_class_class();
}

void*
DrlVMTypeManager::getSystemStringVMTypeHandle() {
    return get_system_string_class();
}

void*
DrlVMTypeManager::getArrayVMTypeHandle(void* elemVMTypeHandle,bool isUnboxed) {
    if (isUnboxed)
        return class_get_array_of_unboxed((Class_Handle) elemVMTypeHandle);
    return class_get_array_of_class((Class_Handle) elemVMTypeHandle);
}

const char* 
DrlVMTypeManager::getTypeNameQualifier(void* vmTypeHandle) {
    return class_get_package_name((Class_Handle) vmTypeHandle);
}

void*
DrlVMTypeManager::getArrayElemVMTypeHandle(void* vmTypeHandle) {
    return class_get_array_element_class((Class_Handle) vmTypeHandle);
}

const char* DrlVMTypeManager::getTypeName(void* vmTypeHandle) {
    return class_get_name((Class_Handle) vmTypeHandle);
}

bool
DrlVMTypeManager::isArrayOfPrimitiveElements(void* vmClassHandle) {
    return type_info_is_primitive(class_get_element_type_info((Class_Handle) vmClassHandle))?true:false;
}

bool
DrlVMTypeManager::isEnumType(void* vmTypeHandle) {
    return false;
}

bool
DrlVMTypeManager::isValueType(void* vmTypeHandle) {
    return class_is_primitive((Class_Handle) vmTypeHandle);
}

bool
DrlVMTypeManager::isLikelyExceptionType(void* vmTypeHandle) {
    return class_hint_is_exceptiontype((Class_Handle) vmTypeHandle)?true:false;
}

bool
DrlVMTypeManager::isVariableSizeType(void* vmTypeHandle) {
    return isArrayType(vmTypeHandle);
}

const char*
DrlVMTypeManager::getMethodName(MethodDesc* methodDesc) {
    return methodDesc->getName();
}


bool
DrlVMTypeManager::isSystemStringType(void* vmTypeHandle) {
    // We should also be looking at namespace
    if (vmTypeHandle == systemStringVMTypeHandle)
        return true;
    const char* name = getTypeName(vmTypeHandle);
    if (systemStringVMTypeHandle == NULL && strcmp(name,"String") == 0) {
        // Built-in System.String type
        systemStringVMTypeHandle = vmTypeHandle;
        return true;
    }
    return false;
}

bool
DrlVMTypeManager::isSystemObjectType(void* vmTypeHandle) {
    // We should also be looking at namespace
    if (vmTypeHandle == systemObjectVMTypeHandle)
        return true;
    const char* name = getTypeName(vmTypeHandle);
    if (systemObjectVMTypeHandle == NULL && strcmp(name,"Object") == 0) {
        // Built-in System.Object type
        systemObjectVMTypeHandle = vmTypeHandle;
        return true;
    }
    return false;
}

bool
DrlVMTypeManager::isSystemClassType(void* vmTypeHandle) {
    // We should also be looking at namespace
    if (vmTypeHandle == systemClassVMTypeHandle)
        return true;
    const char* name = getTypeName(vmTypeHandle);
    if (systemClassVMTypeHandle == NULL && strcmp(name,"Class") == 0) {
        // Built-in System.Class type
        systemClassVMTypeHandle = vmTypeHandle;
        return true;
    }
    return false;
}

bool
DrlVMTypeManager::isBeforeFieldInit(void* vmTypeHandle) {
    return class_is_before_field_init((Class_Handle) vmTypeHandle)?true:false;
}

bool        
DrlVMTypeManager::getClassFastInstanceOfFlag(void* vmTypeHandle) {
    return class_get_fast_instanceof_flag((Class_Handle) vmTypeHandle)?true:false;
}

int 
DrlVMTypeManager::getClassDepth(void* vmTypeHandle) {
    return class_get_depth((Class_Handle) vmTypeHandle);
}

uint32
DrlVMTypeManager::getArrayLengthOffset() {
    return vector_length_offset();
}

uint32
DrlVMTypeManager::getArrayElemOffset(void* vmElemTypeHandle,bool isUnboxed) {
    if (isUnboxed)
        return vector_first_element_offset_unboxed((Class_Handle) vmElemTypeHandle);
    return vector_first_element_offset_class_handle((Class_Handle) vmElemTypeHandle);
}

bool
DrlVMTypeManager::isSubClassOf(void* vmTypeHandle1,void* vmTypeHandle2) {
    return class_is_instanceof((Class_Handle) vmTypeHandle1,(Class_Handle) vmTypeHandle2)?true:false;
}    

uint32
DrlVMTypeManager::getUnboxedOffset(void* vmTypeHandle) {
    assert(false); return 0;
}

uint32
DrlVMTypeManager::getBoxedSize(void * vmTypeHandle) {
    return class_get_boxed_data_size((Class_Handle) vmTypeHandle);
}

uint32
DrlVMTypeManager::getUnboxedSize(void* vmTypeHandle) {
    assert(false); return 0;
}

uint32
DrlVMTypeManager::getUnboxedAlignment(void* vmTypeHandle) {
    return class_get_alignment((Class_Handle) vmTypeHandle);
}

uint32
DrlVMTypeManager::getUnboxedNumFields(void* vmTypeHandle) {
    assert(0);
    return 0;
}    

FieldDesc*
DrlVMTypeManager::getUnboxedFieldDesc(void* vmTypeHandle,uint32 index) {
    assert(0);
    return NULL;
}

Type*
DrlVMTypeManager::getUnderlyingType(void* enumVMTypeHandle) {
    assert(false); return 0;
}

//////////////////////////////////////////////////////////////////////////////
///////////////////////// DrlVMMethodSignatureDesc /////////////////////////////
//////////////////////////////////////////////////////////////////////////////
Type**
DrlVMMethodSignatureDesc::getParamTypes() {
    if (paramTypes != NULL)
        return paramTypes;
    uint32 numParams = getNumParams();
    paramTypes = new (compilationInterface->getMemManager()) Type* [numParams];
    for (uint32 i=0; i<numParams; i++) {
        paramTypes[i] = getParamType(i);
    }
    return paramTypes;
}

uint32    
DrlVMMethodSignatureDesc::getNumParams() {
    return method_args_get_number(drlSigHandle);
}

Type*    
DrlVMMethodSignatureDesc::getParamType(uint32 paramIndex) {
    Type_Info_Handle typeHandle = method_args_get_type_info(drlSigHandle,paramIndex);
    bool isManagedPointer = 
        mtd_args_is_managed_pointer(drlSigHandle,paramIndex)?true:false;
    return compilationInterface->getTypeFromDrlVMTypeHandle(typeHandle,isManagedPointer);
}

Type*
DrlVMMethodSignatureDesc::getReturnType() {
    bool isManagedPointer = 
        mtd_ret_type_is_managed_pointer(drlSigHandle)?true:false;
    Type_Info_Handle typeHandle = method_ret_type_get_type_info(drlSigHandle);
    return compilationInterface->getTypeFromDrlVMTypeHandle(typeHandle,isManagedPointer);
}

//////////////////////////////////////////////////////////////////////////////
///////////////////////// DrlVMMethodDesc //////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

bool
DrlVMMethodDesc::isVarPinned(uint32 varIndex) {
    return false;
}

Type*
DrlVMMethodDesc::getVarType(uint32 varIndex) {
    bool isManagedPointer = 
        mtd_vars_is_managed_pointer(drlMethod,varIndex)?true:false;
    Type_Info_Handle typeHandle = method_vars_get_type_info(drlMethod,varIndex);
    return compilationInterface->getTypeFromDrlVMTypeHandle(typeHandle,isManagedPointer);
}

NamedType*
DrlVMMethodDesc::getParentType()    {
    TypeManager& typeManager = compilationInterface->getTypeManager();
    Class_Handle parentClassHandle = method_get_class(drlMethod);
    if (class_is_primitive(parentClassHandle))
        return typeManager.getValueType(parentClassHandle);
    return typeManager.getObjectType(parentClassHandle);
}

unsigned 
DrlVMMethodDesc::parseJavaHandlers(ExceptionCallback& callback) {
    uint32 numHandlers = getNumHandlers();
    for (uint32 i=0; i<numHandlers; i++) {
        unsigned beginOffset,endOffset,handlerOffset,handlerClassIndex;
        method_get_handler_info(drlMethod,i,&beginOffset,&endOffset,
                             &handlerOffset,&handlerClassIndex);
        if (!callback.catchBlock(beginOffset,endOffset-beginOffset,
                                 handlerOffset,0,handlerClassIndex))
        {
            // handlerClass failed to be resolved. LinkingException throwing helper
            // will be generated instead of method's body
            return handlerClassIndex;
        }
    }
    return MAX_UINT32; // all catchBlocks were processed successfully
}

void 
DrlVMMethodDesc::parseCliHandlers(ExceptionCallback& callback) {
    assert(false);

}


// accessors for method info, code and data
Byte*        DrlVMMethodDesc::getInfoBlock() {
    Method_Handle drlMethod = getDrlVMMethod();
    return methodGetStacknGCInfoBlock(drlMethod, getJitHandle());
}

uint32        DrlVMMethodDesc::getInfoBlockSize() {
    Method_Handle drlMethod = getDrlVMMethod();
    return methodGetStacknGCInfoBlockSize(drlMethod, getJitHandle());
}

Byte*        DrlVMMethodDesc::getCodeBlockAddress(int32 id) {
    Method_Handle drlMethod = getDrlVMMethod();
    return method_get_code_block_addr_jit_new(drlMethod,getJitHandle(), id);
}

uint32        DrlVMMethodDesc::getCodeBlockSize(int32 id) {
    Method_Handle drlMethod = getDrlVMMethod();
    return method_get_code_block_size_jit_new(drlMethod,getJitHandle(), id);
}

uint32
DrlVMMethodDesc::getUniqueId()
{
#ifdef _IPF_
//    assert(0);
    return 0;
#else
    Method_Handle       mh = getDrlVMMethod();
    return (POINTER_SIZE_INT)mh;
#endif
}

bool
DrlVMMethodDesc::isNoInlining() {
    return method_is_no_inlining(drlMethod)?true:false;
}    

bool
DrlVMMethodDesc::isRequireSecObject() {
    return method_is_require_security_object(drlMethod)?true:false;
}

bool
DrlVMMethodDesc::isMethodClassIsLikelyExceptionType() {
    Class_Handle ch = method_get_class(drlMethod);
    assert(ch!=NULL);
    return class_hint_is_exceptiontype(ch)?true:false;
}

bool
DrlVMMethodDesc::isAddressFinal()    {
    return false;
}

//////////////////////////////////////////////////////////////////////////////
///////////////////////// DrlVMFieldDesc ///////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

bool
DrlVMFieldDesc::isLiteral()    {
    return field_is_literal(drlField)?true:false;
}

bool
DrlVMFieldDesc::isUnmanagedStatic()    {
    return false;
}

NamedType*
DrlVMFieldDesc::getParentType()    {
    TypeManager& typeManager = compilationInterface->getTypeManager();
    Class_Handle parentClassHandle = field_get_class(drlField);
    if (class_is_primitive(parentClassHandle))
        return typeManager.getValueType(parentClassHandle);
    return typeManager.getObjectType(parentClassHandle);
}

Type*
DrlVMFieldDesc::getFieldType() {
    Type_Info_Handle typeHandle = field_get_type_info_of_field_value(drlField);
    return compilationInterface->getTypeFromDrlVMTypeHandle(typeHandle,false);
}

uint32
DrlVMFieldDesc::getOffset() {
    if(getParentType()->isObject()) {
        return field_get_offset(drlField);
    }
    else {
        assert(false); return 0;
    }
}


//////////////////////////////////////////////////////////////////////////////
//////////////////////////// DrlVMClassHierachyMethodIterator //////////////////
//////////////////////////////////////////////////////////////////////////////
MethodDesc* 
DrlVMClassHierarchyMethodIterator::getNext() { 
    MethodDesc* desc = compilationInterface.getMethodDesc(method_iterator_get_current(&iterator)); 
    method_iterator_advance(&iterator); 
    return desc; 
}


//////////////////////////////////////////////////////////////////////////////
//////////////////////////// DrlVMCompilationInterface /////////////////////////
//////////////////////////////////////////////////////////////////////////////

void 
DrlVMCompilationInterface::hardAssert(const char *message, int line, const char *file)
{
    ::std::cerr << message << " at line " << line << " of file " << file << ::std::endl;
    exit(1);
}

Type*
DrlVMCompilationInterface::getTypeFromDrlVMTypeHandle(Type_Info_Handle typeHandle) {
    return getTypeFromDrlVMTypeHandle(typeHandle, false);
}

Type*
DrlVMCompilationInterface::getTypeFromDrlVMTypeHandle(Type_Info_Handle typeHandle,
                                                                  bool isManagedPointer) {
    Type* type = NULL;
    if (isManagedPointer) {
        Type_Info_Handle pointedToTypeHandle = type_info_get_type_info(typeHandle);
        Type* pointedToType = getTypeFromDrlVMTypeHandle(pointedToTypeHandle);
        if (!pointedToType)
            return NULL;
        type = typeManager.getManagedPtrType(pointedToType);
    } else if (type_info_is_void(typeHandle)) {
        // void return type
        type = typeManager.getVoidType();
    } else if (type_info_is_reference(typeHandle)) {
        Class_Handle classHandle = type_info_get_class_no_exn(typeHandle);
        if (!classHandle)
            return NULL;
        type = typeManager.getObjectType(classHandle);
    } else if (type_info_is_primitive(typeHandle)) {
        // value type
        Class_Handle valueTypeHandle = type_info_get_class(typeHandle);
        if (!valueTypeHandle)
            return NULL;
        type = typeManager.getValueType(valueTypeHandle);
    } else if (type_info_is_vector(typeHandle)) {
        // vector
        Type_Info_Handle elemTypeInfo = type_info_get_type_info(typeHandle);
        Type* elemType = getTypeFromDrlVMTypeHandle(elemTypeInfo);
        if (!elemType)
            return NULL;
        type = typeManager.getArrayType(elemType);
    } else if (type_info_is_general_array(typeHandle)) {
        assert(0);
    } else {
        // should not get here
        assert(0);
    }
    return type;
}

VM_RT_SUPPORT DrlVMCompilationInterface::translateHelperId(RuntimeHelperId runtimeHelperId) {
    VM_RT_SUPPORT vmHelperId = (VM_RT_SUPPORT)-1;
    switch (runtimeHelperId) {
    case Helper_NewObj_UsingVtable:    vmHelperId = VM_RT_NEW_RESOLVED_USING_VTABLE_AND_SIZE; break; 
    case Helper_NewVector_UsingVtable: vmHelperId = VM_RT_NEW_VECTOR_USING_VTABLE; break;
    case Helper_NewMultiArray:         vmHelperId = VM_RT_MULTIANEWARRAY_RESOLVED; break;
    case Helper_LdInterface:           vmHelperId = VM_RT_GET_INTERFACE_VTABLE_VER0; break;
    case Helper_LdRef:                 vmHelperId = VM_RT_LDC_STRING; break;
    case Helper_ObjMonitorEnter:       vmHelperId = VM_RT_MONITOR_ENTER_NON_NULL; break;
    case Helper_ObjMonitorExit:        vmHelperId = VM_RT_MONITOR_EXIT_NON_NULL; break;
    case Helper_TypeMonitorEnter:      vmHelperId = VM_RT_MONITOR_ENTER_STATIC; break;
    case Helper_TypeMonitorExit:       vmHelperId = VM_RT_MONITOR_EXIT_STATIC; break;
    case Helper_Cast:                  vmHelperId = VM_RT_CHECKCAST; break;
    case Helper_IsInstanceOf:          vmHelperId = VM_RT_INSTANCEOF; break;
    case Helper_InitType:              vmHelperId = VM_RT_INITIALIZE_CLASS; break;
    case Helper_IsValidElemType:       vmHelperId = VM_RT_AASTORE_TEST; break;
    case Helper_Throw_KeepStackTrace:  vmHelperId = VM_RT_THROW; break;
    case Helper_Throw_SetStackTrace:   vmHelperId = VM_RT_THROW_SET_STACK_TRACE; break;
    case Helper_Throw_Lazy:            vmHelperId = VM_RT_THROW_LAZY; break;
    case Helper_NullPtrException:      vmHelperId = VM_RT_NULL_PTR_EXCEPTION; break;
    case Helper_ArrayBoundsException:  vmHelperId = VM_RT_IDX_OUT_OF_BOUNDS; break;
    case Helper_ElemTypeException:     vmHelperId = VM_RT_ARRAY_STORE_EXCEPTION; break;
    case Helper_DivideByZeroException: vmHelperId = VM_RT_DIVIDE_BY_ZERO_EXCEPTION; break;
    case Helper_Throw_LinkingException: vmHelperId = VM_RT_THROW_LINKING_EXCEPTION; break;
    case Helper_EnableThreadSuspension: vmHelperId = VM_RT_GC_SAFE_POINT; break;
    case Helper_GetTLSBase:            vmHelperId = VM_RT_GC_GET_TLS_BASE; break;
    case Helper_CharArrayCopy:         vmHelperId = VM_RT_CHAR_ARRAYCOPY_NO_EXC; break;
    case Helper_DivI32:                vmHelperId = VM_RT_IDIV; break;
    case Helper_DivU32:                assert(0); break;
    case Helper_DivI64:                vmHelperId = VM_RT_LDIV; break;
    case Helper_DivU64:                vmHelperId = VM_RT_ULDIV; break;
    case Helper_DivSingle:             vmHelperId = VM_RT_FDIV; break;
    case Helper_DivDouble:             vmHelperId = VM_RT_DDIV; break;
    case Helper_RemI32:                vmHelperId = VM_RT_IREM; break;
    case Helper_RemU32:                assert(0); break;
    case Helper_RemI64:                vmHelperId = VM_RT_LREM; break;
    case Helper_RemU64:                assert(0); break;
    case Helper_RemSingle:             vmHelperId = VM_RT_FREM; break;
    case Helper_RemDouble:             vmHelperId = VM_RT_DREM; break;
    case Helper_MulI64:                vmHelperId = VM_RT_LMUL; break;
    case Helper_ShlI64:                vmHelperId = VM_RT_LSHL; break;
    case Helper_ShrI64:                vmHelperId = VM_RT_LSHR; break;
    case Helper_ShruI64:               vmHelperId = VM_RT_LUSHR; break;
    case Helper_ConvStoI32:            vmHelperId = VM_RT_F2I; break;
    case Helper_ConvStoI64:            vmHelperId = VM_RT_F2L; break;
    case Helper_ConvDtoI32:            vmHelperId = VM_RT_D2I; break;
    case Helper_ConvDtoI64:            vmHelperId = VM_RT_D2L; break;
    case Helper_MethodEntry:           vmHelperId = VM_RT_JVMTI_METHOD_ENTER_CALLBACK; break;
    case Helper_MethodExit:             vmHelperId = VM_RT_JVMTI_METHOD_EXIT_CALLBACK; break;
    case Helper_WriteBarrier:          vmHelperId = VM_RT_GC_HEAP_WRITE_REF; break;

    default:
        assert(0);
    }
    return vmHelperId;
}

void*        
DrlVMCompilationInterface::getRuntimeHelperAddress(RuntimeHelperId runtimeHelperId) {
    VM_RT_SUPPORT drlHelperId = translateHelperId(runtimeHelperId);
    return vm_get_rt_support_addr(drlHelperId);
}

void*        
DrlVMCompilationInterface::getRuntimeHelperAddressForType(RuntimeHelperId runtimeHelperId, Type* type) {
    VM_RT_SUPPORT drlHelperId = translateHelperId(runtimeHelperId);
    Class_Handle handle = NULL;
    if (type != NULL && type->isNamedType())
        handle = (Class_Handle) ((NamedType *)type)->getVMTypeHandle();
    void* addr = vm_get_rt_support_addr_optimized(drlHelperId, handle);
    assert(addr != NULL);
    return addr;
}

CompilationInterface::MethodSideEffect  
DrlVMCompilationInterface::getMethodHasSideEffect(MethodDesc *m) {
    Method_Side_Effects mse = method_get_side_effects(((DrlVMMethodDesc*)m)->getDrlVMMethod());

    switch (mse) {
    case MSE_True:            return CompilationInterface::MSE_YES;
    case MSE_False:           return CompilationInterface::MSE_NO;
    case MSE_Unknown:         return CompilationInterface::MSE_UNKNOWN;
    case MSE_True_Null_Param: return CompilationInterface::MSE_NULL_PARAM;
    default:
        assert(0);
        return CompilationInterface::MSE_UNKNOWN;
    }
}

void
DrlVMCompilationInterface::setMethodHasSideEffect(MethodDesc *m, MethodSideEffect mse) {
    Method_Handle handle = ((DrlVMMethodDesc*)m)->getDrlVMMethod();

    switch (mse) {
    case CompilationInterface::MSE_YES:
        method_set_side_effects(handle, MSE_True);
        break;
    case CompilationInterface::MSE_NO:
        method_set_side_effects(handle, MSE_False);
        break;
    case CompilationInterface::MSE_UNKNOWN:
        method_set_side_effects(handle, MSE_Unknown);
        break;
    case CompilationInterface::MSE_NULL_PARAM:
        method_set_side_effects(handle, MSE_True_Null_Param);
        break;
    default:
        assert(0);
    }
}

CompilationInterface::VmCallingConvention 
DrlVMCompilationInterface::getRuntimeHelperCallingConvention(RuntimeHelperId id) {
    switch(id) {
    case Helper_NewMultiArray:
    case Helper_WriteBarrier:
        return CallingConvention_Cdecl;
    case Helper_ShlI64:                
    case Helper_ShrI64:                
    case Helper_ShruI64:
    case Helper_Throw_Lazy:
    case Helper_Throw_LinkingException:
        return CallingConvention_Drl;
    default:
        return CallingConvention_Stdcall;
    }
}

bool
DrlVMCompilationInterface::compileMethod(MethodDesc *method) {
    if (Log::isEnabled()) {
        Log::out() << "Jitrino requested compilation of " <<
            method->getParentType()->getName() << "::" <<
            method->getName() << method->getSignatureString() << ::std::endl;
    }
    JIT_Result res = vm_compile_method(getJitHandle(), ((DrlVMMethodDesc*)method)->getDrlVMMethod());
    return res == JIT_SUCCESS ? true : false;
}

void        
DrlVMCompilationInterface::setNumExceptionHandler(uint32 numHandlers) {
    method_set_num_target_handlers(methodToCompile->getDrlVMMethod(),getJitHandle(),
                                   numHandlers);
}

void
DrlVMCompilationInterface::setExceptionHandlerInfo(uint32 exceptionHandlerNumber,
                                                   Byte*  startAddr,
                                                 Byte*  endAddr,
                                                 Byte*  handlerAddr,
                                                 NamedType*  exceptionType,
                                                 bool   exceptionObjIsDead) {
    void* exn_handle;
    assert(exceptionType);
    if (exceptionType->isSystemObject())
        exn_handle = NULL;
    else
        exn_handle = exceptionType->getRuntimeIdentifier();
    method_set_target_handler_info(methodToCompile->getDrlVMMethod(),
                                   getJitHandle(),
                                   exceptionHandlerNumber,
                                   startAddr,
                                   endAddr,
                                   handlerAddr,
                                   (Class_Handle) exn_handle,
                                   exceptionObjIsDead ? TRUE : FALSE);
}

// token resolution methods
MethodSignatureDesc*
DrlVMCompilationInterface::resolveSignature(MethodDesc* enclosingMethodDesc,
                                          uint32 sigToken) {
    assert(0);
    return NULL;
}

Class_Handle
DrlVMCompilationInterface::methodGetClass(MethodDesc* method) {
    return method_get_class(((DrlVMMethodDesc*)method)->getDrlVMMethod());
}

FieldDesc*  
DrlVMCompilationInterface::resolveField(MethodDesc* enclosingMethodDesc,
                                        uint32 fieldToken,
                                        bool putfield) {
    Class_Handle enclosingDrlVMClass = methodGetClass(enclosingMethodDesc);
    
    Field_Handle resolvedField = 
        resolve_nonstatic_field(compileHandle,enclosingDrlVMClass,fieldToken,putfield);
    if (!resolvedField) return NULL;
    return getFieldDesc(resolvedField);
}

FieldDesc*
DrlVMCompilationInterface::resolveFieldByIndex(NamedType* klass, int index, NamedType **fieldType) {

    Class_Handle ch = (Class_Handle) klass->getVMTypeHandle();
    Field_Handle fh;
    fh = class_get_instance_field_recursive(ch,index);
    ::std::cerr << "load field "<< class_get_name((Class_Handle) klass->getVMTypeHandle()) << ".";
    ::std::cerr << field_get_name(fh) << " as ";
    (*fieldType)->print(::std::cerr); ::std::cerr << ::std::endl;
    return getFieldDesc(fh);
}

FieldDesc*
DrlVMCompilationInterface::resolveStaticField(MethodDesc* enclosingMethodDesc,
                                                   uint32 fieldToken, bool putfield) {
    Class_Handle enclosingDrlVMClass = methodGetClass(enclosingMethodDesc);

    Field_Handle resolvedField = 
        resolve_static_field(compileHandle,enclosingDrlVMClass,fieldToken,putfield);
    if (!resolvedField) return NULL;
    return getFieldDesc(resolvedField);
}

MethodDesc* 
DrlVMCompilationInterface::resolveMethod(MethodDesc* enclosingMethodDesc,
                                              uint32 methodToken) {
    assert(false); return 0;
}    

MethodDesc* 
DrlVMCompilationInterface::resolveVirtualMethod(MethodDesc* enclosingMethodDesc,
                                                     uint32 methodToken) {
    Class_Handle enclosingDrlVMClass = methodGetClass(enclosingMethodDesc);
    
    Method_Handle resolvedMethod = 
        resolve_virtual_method(compileHandle,enclosingDrlVMClass,methodToken);
    if (!resolvedMethod) return NULL;
    return getMethodDesc(resolvedMethod);
}    

MethodDesc* 
DrlVMCompilationInterface::resolveSpecialMethod(MethodDesc* enclosingMethodDesc,
                                                     uint32 methodToken) {
    Class_Handle enclosingDrlVMClass = methodGetClass(enclosingMethodDesc);
    
    Method_Handle resolvedMethod = 
        resolve_special_method(compileHandle,enclosingDrlVMClass,methodToken);
    if (!resolvedMethod) return NULL;
    return getMethodDesc(resolvedMethod);
}    

MethodDesc* 
DrlVMCompilationInterface::resolveStaticMethod(MethodDesc* enclosingMethodDesc,
                                                    uint32 methodToken) {
    Class_Handle enclosingDrlVMClass = methodGetClass(enclosingMethodDesc);
    
    Method_Handle resolvedMethod = 
        resolve_static_method(compileHandle,enclosingDrlVMClass,methodToken);
    if (!resolvedMethod) return NULL;
    return getMethodDesc(resolvedMethod);
}    

MethodDesc* 
DrlVMCompilationInterface::resolveInterfaceMethod(MethodDesc* enclosingMethodDesc,
                                                       uint32 methodToken) {
    Class_Handle enclosingDrlVMClass = methodGetClass(enclosingMethodDesc);
    
    Method_Handle resolvedMethod = 
        resolve_interface_method(compileHandle,enclosingDrlVMClass,methodToken);
    if (!resolvedMethod) return NULL;
    return getMethodDesc(resolvedMethod);
}    

NamedType*
DrlVMCompilationInterface::resolveNamedType(MethodDesc* enclosingMethodDesc,
                                                 uint32 typeToken) {
    Class_Handle enclosingDrlVMClass = methodGetClass(enclosingMethodDesc);
    
    Class_Handle ch = 
        resolve_class(compileHandle,enclosingDrlVMClass,typeToken);
    if (!ch) return NULL;
    if (class_is_primitive(ch))
        return typeManager.getValueType(ch);
    return typeManager.getObjectType(ch);
}

NamedType*
DrlVMCompilationInterface::resolveNamedTypeNew(MethodDesc* enclosingMethodDesc,
                                                    uint32 typeToken) {
    Class_Handle enclosingDrlVMClass = methodGetClass(enclosingMethodDesc);
    
    Class_Handle ch = 
        resolve_class_new(compileHandle,enclosingDrlVMClass,typeToken);
    if (!ch) return NULL;
    if (class_is_primitive(ch))
        return typeManager.getValueType(ch);
    return typeManager.getObjectType(ch);
}

Type*
DrlVMCompilationInterface::getFieldType(MethodDesc* enclosingMethodDesc,
                                             uint32 entryCPIndex) {
    Class_Handle enclosingDrlVMClass = methodGetClass(enclosingMethodDesc);
    Java_Type drlType = (Java_Type)class_get_cp_field_type(enclosingDrlVMClass, (unsigned short)entryCPIndex);
    switch (drlType) {
    case JAVA_TYPE_BOOLEAN:  return typeManager.getBooleanType();
    case JAVA_TYPE_CHAR:     return typeManager.getCharType();
    case JAVA_TYPE_BYTE:     return typeManager.getInt8Type();
    case JAVA_TYPE_SHORT:    return typeManager.getInt16Type();
    case JAVA_TYPE_INT:      return typeManager.getInt32Type();
    case JAVA_TYPE_LONG:     return typeManager.getInt64Type();
    case JAVA_TYPE_DOUBLE:   return typeManager.getDoubleType();
    case JAVA_TYPE_FLOAT:    return typeManager.getSingleType();
    case JAVA_TYPE_ARRAY:
    case JAVA_TYPE_CLASS:    return typeManager.getNullObjectType();

    case JAVA_TYPE_VOID:     // class_get_cp_field_type can't return VOID
    case JAVA_TYPE_STRING:   // class_get_cp_field_type can't return STRING
    default: assert(0);
    }
    assert(0);
    return NULL;
}
const char*
DrlVMCompilationInterface::methodSignatureString(MethodDesc* enclosingMethodDesc,
                                                      uint32 methodToken) {
    Class_Handle enclosingDrlVMClass = methodGetClass(enclosingMethodDesc);
    return class_get_cp_entry_signature(enclosingDrlVMClass, (unsigned short)methodToken);
}

void* 
DrlVMCompilationInterface::loadStringObject(MethodDesc* enclosingMethodDesc,
                                                uint32 stringToken) {
    Class_Handle enclosingDrlVMClass = methodGetClass(enclosingMethodDesc);
    return class_get_const_string_intern_addr(enclosingDrlVMClass,stringToken);
}

//
// Note: This is CLI only but temporarily here for Java also
//
void*
DrlVMCompilationInterface::loadToken(MethodDesc* enclosingMethodDesc,uint32 token) {
    assert(false); return 0;
}

Type*
DrlVMCompilationInterface::getConstantType(MethodDesc* enclosingMethodDesc,
                                         uint32 constantToken) {
    Class_Handle enclosingDrlVMClass = methodGetClass(enclosingMethodDesc);
    Java_Type drlType = (Java_Type)class_get_const_type(enclosingDrlVMClass,constantToken);
    switch (drlType) {
    case JAVA_TYPE_STRING:   return typeManager.getSystemStringType(); 
    case JAVA_TYPE_CLASS:    return typeManager.getSystemClassType(); 
    case JAVA_TYPE_DOUBLE:   return typeManager.getDoubleType();
    case JAVA_TYPE_FLOAT:    return typeManager.getSingleType();
    case JAVA_TYPE_INT:      return typeManager.getInt32Type();
    case JAVA_TYPE_LONG:     return typeManager.getInt64Type();
    default: assert(0);
    }
    assert(0);
    return NULL;
}

const void*
DrlVMCompilationInterface::getConstantValue(MethodDesc* enclosingMethodDesc,
                                          uint32 constantToken) {
    Class_Handle enclosingDrlVMClass = methodGetClass(enclosingMethodDesc);
    return class_get_const_addr(enclosingDrlVMClass,constantToken);
}

MethodDesc*
DrlVMCompilationInterface::getOverriddenMethod(NamedType* type, MethodDesc *methodDesc) {
    Method_Handle m = method_find_overridden_method((Class_Handle) type->getVMTypeHandle(),
                         ((DrlVMMethodDesc*)methodDesc)->getDrlVMMethod());
    if (!m)
        return NULL;
    return getMethodDesc(m);
}

ClassHierarchyIterator* 
DrlVMCompilationInterface::getClassHierarchyIterator(ObjectType* baseType) {
    DrlVMClassHierarchyIterator* iterator = new (getMemManager()) DrlVMClassHierarchyIterator(getTypeManager(), baseType);
    return iterator->isValid() ? iterator : NULL;
}

ClassHierarchyMethodIterator* 
DrlVMCompilationInterface::getClassHierarchyMethodIterator(ObjectType* baseType, MethodDesc* methodDesc) {
    DrlVMClassHierarchyMethodIterator* iterator = new (getMemManager()) DrlVMClassHierarchyMethodIterator(*this, baseType, methodDesc);
    return iterator->isValid() ? iterator : NULL;
}

// accessors for method info, code and data
Byte*        DrlVMCompilationInterface::getInfoBlock(MethodDesc* methodDesc) {
    Method_Handle drlMethod = ((DrlVMMethodDesc*)methodDesc)->getDrlVMMethod();
    return methodGetStacknGCInfoBlock(drlMethod, getJitHandle());
}

uint32        DrlVMCompilationInterface::getInfoBlockSize(MethodDesc* methodDesc) {
    Method_Handle drlMethod = ((DrlVMMethodDesc*)methodDesc)->getDrlVMMethod();
    return methodGetStacknGCInfoBlockSize(drlMethod,getJitHandle());
}

Byte*        DrlVMCompilationInterface::getCodeBlockAddress(MethodDesc* methodDesc, int32 id) {
    Method_Handle drlMethod = ((DrlVMMethodDesc*)methodDesc)->getDrlVMMethod();
    return method_get_code_block_addr_jit_new(drlMethod,getJitHandle(),id);
}

uint32        DrlVMCompilationInterface::getCodeBlockSize(MethodDesc* methodDesc, int32 id) {
    Method_Handle drlMethod = ((DrlVMMethodDesc*)methodDesc)->getDrlVMMethod();
    return method_get_code_block_size_jit_new(drlMethod,getJitHandle(),id);
}

void         DrlVMCompilationInterface::setNotifyWhenClassIsExtended(ObjectType * type, 
                                                                   void * callbackData) {
    void * typeHandle = type->getVMTypeHandle();
    vm_register_jit_extended_class_callback(getJitHandle(), (Class_Handle) typeHandle,callbackData);
}

void         DrlVMCompilationInterface::setNotifyWhenMethodIsOverridden(MethodDesc * methodDesc, 
                                                                      void * callbackData) {
    Method_Handle drlMethod = ((DrlVMMethodDesc *)methodDesc)->getDrlVMMethod();
    vm_register_jit_overridden_method_callback(getJitHandle(), drlMethod, callbackData);
}

void         DrlVMCompilationInterface::setNotifyWhenMethodIsRecompiled(MethodDesc * methodDesc, 
                                                                      void * callbackData) {
    Method_Handle drlMethod = ((DrlVMMethodDesc *)methodDesc)->getDrlVMMethod();
    vm_register_jit_recompiled_method_callback(getJitHandle(),drlMethod,callbackData);
}


bool DrlVMCompilationInterface::mayInlineObjectSynchronization(ObjectSynchronizationInfo & syncInfo) {
    unsigned threadIdReg, syncHeaderOffset, syncHeaderWidth, lockOwnerOffset, lockOwnerWidth;
    Boolean jitClearsCcv;
    Boolean mayInline = 
        jit_may_inline_object_synchronization(&threadIdReg, &syncHeaderOffset, &syncHeaderWidth,
                                              &lockOwnerOffset, &lockOwnerWidth, &jitClearsCcv);
    if (mayInline == TRUE) {
        syncInfo.threadIdReg = threadIdReg;
        syncInfo.syncHeaderOffset = syncHeaderOffset;
        syncInfo.syncHeaderWidth = syncHeaderWidth;
        syncInfo.lockOwnerOffset = lockOwnerOffset;
        syncInfo.lockOwnerWidth = lockOwnerWidth;
        syncInfo.jitClearsCcv = (jitClearsCcv == TRUE);
    }
    return mayInline == TRUE;
}

void DrlVMCompilationInterface::sendCompiledMethodLoadEvent(MethodDesc* methodDesc, MethodDesc* outerDesc,
        uint32 codeSize, void* codeAddr, uint32 mapLength, 
        AddrLocation* addrLocationMap, void* compileInfo) {

    Method_Handle method = (Method_Handle)getRuntimeMethodHandle(methodDesc);
    Method_Handle outer  = (Method_Handle)getRuntimeMethodHandle(outerDesc);

    compiled_method_load(method, codeSize, codeAddr, mapLength, addrLocationMap, compileInfo, outer); 
}

bool DrlVMDataInterface::areReferencesCompressed() {
    return (vm_references_are_compressed() != 0);
}

void * DrlVMDataInterface::getHeapBase() {
    return vm_heap_base_address();
}

void * DrlVMDataInterface::getHeapCeiling() {
    return vm_heap_ceiling_address();
}

void DrlVMBinaryRewritingInterface::rewriteCodeBlock(Byte* codeBlock, Byte*  newCode, size_t size) {
    vm_patch_code_block(codeBlock, newCode, size);
}


ObjectType * DrlVMCompilationInterface::resolveClassUsingBootstrapClassloader( const char * klassName ) {
    Class_Handle cls = class_load_class_by_name_using_bootstrap_class_loader(klassName);
    if( NULL == cls ) {
        return NULL;
    }
    return getTypeManager().getObjectType(cls);
};


MethodDesc* DrlVMCompilationInterface::resolveMethod( ObjectType* klass, const char * methodName, const char * methodSig) {
    Class_Handle cls = (Class_Handle)klass->getVMTypeHandle();
    assert( NULL != cls );  
    Method_Handle mh = class_lookup_method_recursively( cls, methodName, methodSig);
    if( NULL == mh ) {
        return NULL;
    }
    return getMethodDesc(mh, NULL);
};

JIT_Handle
DrlVMCompilationInterface::getJitHandle() const {
    return getCompilationContext()->getCurrentJITContext()->getJitHandle();
}




NamedType* DrlVMMethodDesc::getThrowType(uint32 i) {
    assert(i<=method_number_throws(drlMethod));
    Class_Handle ch = method_get_throws(drlMethod, i);
    assert(ch);
    NamedType* res = compilationInterface->getTypeManager().getObjectType(ch);
    return res;
}

bool DrlVMMethodDesc::hasAnnotation(NamedType* type) {
    return method_has_annotation(drlMethod, (Class_Handle)type->getVMTypeHandle());
}

} //namespace Jitrino
