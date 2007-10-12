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

#include "open/hythread_ext.h"

#include "Type.h"
#include "VMInterface.h"
#include "CompilationContext.h"
#include "Log.h"
#include "JITInstanceContext.h"
#include "PlatformDependant.h"
#include "mkernel.h"
#include "VMMagic.h"

namespace Jitrino {




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

void*       
VMInterface::getTypeHandleFromVTable(void* vtHandle){
    return vtable_get_class((VTable_Handle)vtHandle);
}


// TODO: free TLS key on JIT deinitilization
uint32
VMInterface::flagTLSSuspendRequestOffset(){
    return (uint32)hythread_tls_get_request_offset();
}

uint32
VMInterface::flagTLSThreadStateOffset() {
    static hythread_tls_key_t key = 0;
    static size_t offset = 0;
    if (key == 0) {
        hythread_tls_alloc(&key);
        offset = hythread_tls_get_offset(key);
    }
    assert(fit32(offset));
    return (uint32)offset;
}

//////////////////////////////////////////////////////////////////////////////
///////////////////////// VMTypeManager /////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
//
// VM specific type manager
//
void*
TypeManager::getBuiltinValueTypeVMTypeHandle(Type::Tag type) {
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

void 
VMInterface::rewriteCodeBlock(Byte* codeBlock, Byte*  newCode, size_t size) {
    vm_patch_code_block(codeBlock, newCode, size);
}

void*
VMInterface::getSystemObjectVMTypeHandle() {
    return get_system_object_class();
}

void*
VMInterface::getSystemClassVMTypeHandle() {
    return get_system_class_class();
}

void*
VMInterface::getSystemStringVMTypeHandle() {
    return get_system_string_class();
}

void*
VMInterface::getArrayVMTypeHandle(void* elemVMTypeHandle,bool isUnboxed) {
    if (isUnboxed)
        return class_get_array_of_unboxed((Class_Handle) elemVMTypeHandle);
    return class_get_array_of_class((Class_Handle) elemVMTypeHandle);
}

const char* 
VMInterface::getTypeNameQualifier(void* vmTypeHandle) {
    return class_get_package_name((Class_Handle) vmTypeHandle);
}

void*
VMInterface::getArrayElemVMTypeHandle(void* vmTypeHandle) {
    return class_get_array_element_class((Class_Handle) vmTypeHandle);
}

const char* VMInterface::getTypeName(void* vmTypeHandle) {
    return class_get_name((Class_Handle) vmTypeHandle);
}

bool
VMInterface::isArrayOfPrimitiveElements(void* vmClassHandle) {
    return type_info_is_primitive(class_get_element_type_info((Class_Handle) vmClassHandle))?true:false;
}

bool
VMInterface::isEnumType(void* vmTypeHandle) {
    return class_is_enum((Class_Handle) vmTypeHandle);
}

bool
VMInterface::isValueType(void* vmTypeHandle) {
    return class_is_primitive((Class_Handle) vmTypeHandle);
}

bool
VMInterface::isLikelyExceptionType(void* vmTypeHandle) {
    return class_hint_is_exceptiontype((Class_Handle) vmTypeHandle)?true:false;
}

bool
VMInterface::isBeforeFieldInit(void* vmTypeHandle) {
    return class_is_before_field_init((Class_Handle) vmTypeHandle)?true:false;
}

bool        
VMInterface::getClassFastInstanceOfFlag(void* vmTypeHandle) {
    return class_get_fast_instanceof_flag((Class_Handle) vmTypeHandle)?true:false;
}

int 
VMInterface::getClassDepth(void* vmTypeHandle) {
    return class_get_depth((Class_Handle) vmTypeHandle);
}

uint32
VMInterface::getArrayLengthOffset() {
    return vector_length_offset();
}

uint32
VMInterface::getArrayElemOffset(void* vmElemTypeHandle,bool isUnboxed) {
    if (isUnboxed)
        return vector_first_element_offset_unboxed((Class_Handle) vmElemTypeHandle);
    return vector_first_element_offset_class_handle((Class_Handle) vmElemTypeHandle);
}

bool
VMInterface::isSubClassOf(void* vmTypeHandle1,void* vmTypeHandle2) {
    if (vmTypeHandle1 == (void*)(POINTER_SIZE_INT)0xdeadbeef ||
        vmTypeHandle2 == (void*)(POINTER_SIZE_INT)0xdeadbeef ) {
        return false;
    }
    return class_is_instanceof((Class_Handle) vmTypeHandle1,(Class_Handle) vmTypeHandle2)?true:false;
}    

uint32
VMInterface::getObjectSize(void * vmTypeHandle) {
    return class_get_boxed_data_size((Class_Handle) vmTypeHandle);
}

void*       VMInterface::getSuperTypeVMTypeHandle(void* vmTypeHandle) {
    return class_get_super_class((Class_Handle)vmTypeHandle);
}
bool        VMInterface::isArrayType(void* vmTypeHandle) {
    return class_is_array((Class_Handle)vmTypeHandle)?true:false;
}
bool        VMInterface::isFinalType(void* vmTypeHandle) {
    return class_property_is_final((Class_Handle)vmTypeHandle)?true:false;
}
bool        VMInterface::isInterfaceType(void* vmTypeHandle)  {
    return class_property_is_interface2((Class_Handle)vmTypeHandle)?true:false;
}
bool        VMInterface::isAbstractType(void* vmTypeHandle) {
    return class_property_is_abstract((Class_Handle)vmTypeHandle)?true:false;
}
bool        VMInterface::needsInitialization(void* vmTypeHandle) {
    return class_needs_initialization((Class_Handle)vmTypeHandle)?true:false;
}
bool        VMInterface::isFinalizable(void* vmTypeHandle) {
    return class_is_finalizable((Class_Handle)vmTypeHandle)?true:false;
}
bool        VMInterface::isInitialized(void* vmTypeHandle) {
    return class_is_initialized((Class_Handle)vmTypeHandle)?true:false;
}
void*       VMInterface::getVTable(void* vmTypeHandle) {
    return (void *) class_get_vtable((Class_Handle)vmTypeHandle);
}

//
// Allocation handle to be used with calls to runtime support functions for
// object allocation
//
void*       VMInterface::getAllocationHandle(void* vmTypeHandle) {
    return (void *) class_get_allocation_handle((Class_Handle) vmTypeHandle);
}

uint32      VMInterface::getVTableOffset()
{
    return object_get_vtable_offset();
}

void*       VMInterface::getTypeHandleFromAllocationHandle(void* vmAllocationHandle)
{
    return allocation_handle_get_class((Allocation_Handle)vmAllocationHandle);
}



//////////////////////////////////////////////////////////////////////////////
///////////////////////// MethodDesc //////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

uint32    
MethodDesc::getNumParams() const {
    return method_args_get_number(methodSig);
}

Type*    
MethodDesc::getParamType(uint32 paramIndex) const {
    Type_Info_Handle typeHandle = method_args_get_type_info(methodSig,paramIndex);
    return compilationInterface->getTypeFromDrlVMTypeHandle(typeHandle);
}

Type*
MethodDesc::getReturnType() const {
    Type_Info_Handle typeHandle = method_ret_type_get_type_info(methodSig);
    return compilationInterface->getTypeFromDrlVMTypeHandle(typeHandle);
}

Class_Handle MethodDesc::getParentHandle() const {
    return method_get_class(drlMethod);
}

void MethodDesc::getHandlerInfo(unsigned index, unsigned* beginOffset, unsigned* endOffset, unsigned* handlerOffset, unsigned* handlerClassIndex) const {
    method_get_handler_info(drlMethod,index,beginOffset,endOffset,handlerOffset,handlerClassIndex);
}

// accessors for method info, code and data
Byte*        MethodDesc::getInfoBlock() const {
    return methodGetStacknGCInfoBlock(drlMethod, getJitHandle());
}

uint32       MethodDesc::getInfoBlockSize() const {
    return methodGetStacknGCInfoBlockSize(drlMethod, getJitHandle());
}

Byte*        MethodDesc::getCodeBlockAddress(int32 id) const {
    return method_get_code_block_addr_jit_new(drlMethod,getJitHandle(), id);
}

uint32       MethodDesc::getCodeBlockSize(int32 id) const {
    return method_get_code_block_size_jit_new(drlMethod,getJitHandle(), id);
}

bool
MethodDesc::isNoInlining() const {
    return method_is_no_inlining(drlMethod)?true:false;
}    

bool
MethodDesc::isRequireSecObject() {
    return method_is_require_security_object(drlMethod)?true:false;
}

bool
TypeMemberDesc::isParentClassIsLikelyExceptionType() const {
    Class_Handle ch = getParentHandle();
    return class_hint_is_exceptiontype(ch);
}

const char*
CompilationInterface::getSignatureString(MethodDesc* enclosingMethodDesc, uint32 methodToken) {
    Class_Handle enclosingDrlVMClass = enclosingMethodDesc->getParentHandle();
    return class_get_cp_entry_signature(enclosingDrlVMClass, (unsigned short)methodToken);
}

Method_Side_Effects
MethodDesc::getSideEffect() const {
    return method_get_side_effects(drlMethod);
}

void
MethodDesc::setSideEffect(Method_Side_Effects mse) {
    method_set_side_effects(drlMethod, mse);
}

void        
MethodDesc::setNumExceptionHandler(uint32 numHandlers) {
    method_set_num_target_handlers(drlMethod,getJitHandle(),numHandlers);
}

void
MethodDesc::setExceptionHandlerInfo(uint32 exceptionHandlerNumber,
                                    Byte*  startAddr,
                                    Byte*  endAddr,
                                    Byte*  handlerAddr,
                                    NamedType*  exceptionType,
                                    bool   exceptionObjIsDead) 
{
    void* exn_handle;
    assert(exceptionType);
    if (exceptionType->isSystemObject() || exceptionType->isUnresolvedObject()) {
        exn_handle = NULL;
    } else {
        exn_handle = exceptionType->getRuntimeIdentifier();
    }
    method_set_target_handler_info(drlMethod,
        getJitHandle(),
        exceptionHandlerNumber,
        startAddr,
        endAddr,
        handlerAddr,
        (Class_Handle) exn_handle,
        exceptionObjIsDead ? TRUE : FALSE);
}


//////////////////////////////////////////////////////////////////////////////
///////////////////////// FieldDesc ///////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////


Class_Handle FieldDesc::getParentHandle() const {
    return field_get_class(drlField);
}

NamedType*
TypeMemberDesc::getParentType()    {
    TypeManager& typeManager = compilationInterface->getTypeManager();
    Class_Handle parentClassHandle = getParentHandle();
    if (class_is_primitive(parentClassHandle)) {
        assert(0);
        return typeManager.getValueType(parentClassHandle);
    }
    return typeManager.getObjectType(parentClassHandle);
}

Type*
FieldDesc::getFieldType() {
    Type_Info_Handle typeHandle = field_get_type_info_of_field_value(drlField);
    return compilationInterface->getTypeFromDrlVMTypeHandle(typeHandle);
}

uint32
FieldDesc::getOffset() const {
    return field_get_offset(drlField);
}


//////////////////////////////////////////////////////////////////////////////
//////////////////////////// ClassHierachyMethodIterator //////////////////
//////////////////////////////////////////////////////////////////////////////

ClassHierarchyMethodIterator::ClassHierarchyMethodIterator(
    CompilationInterface& compilationInterface, ObjectType* objType, MethodDesc* methodDesc)
    : compilationInterface(compilationInterface)
{
    valid = method_iterator_initialize(&iterator, methodDesc->getMethodHandle(), 
        (Class_Handle) objType->getVMTypeHandle());
}

bool ClassHierarchyMethodIterator::hasNext() const { 
    Method_Handle handle = method_iterator_get_current(&iterator); 
    return handle != NULL; 
}

MethodDesc* 
ClassHierarchyMethodIterator::getNext() { 
    MethodDesc* desc = compilationInterface.getMethodDesc(method_iterator_get_current(&iterator)); 
    method_iterator_advance(&iterator); 
    return desc; 
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////// CompilationInterface /////////////////////////
//////////////////////////////////////////////////////////////////////////////

Type*
CompilationInterface::getTypeFromDrlVMTypeHandle(Type_Info_Handle typeHandle) {
    Type* type = NULL;
    if (type_info_is_void(typeHandle)) {
        // void return type
        type = typeManager.getVoidType();
    } else if (type_info_is_reference(typeHandle)) {
        bool lazy = typeManager.isLazyResolutionMode();
        if (lazy && !type_info_is_resolved(typeHandle)) {
            const char* kname = type_info_get_type_name(typeHandle);
            assert(kname!=NULL);
            bool forceResolve = VMMagicUtils::isVMMagicClass(kname);
            if (!forceResolve) {
                return typeManager.getUnresolvedObjectType();
            }
        }
        Class_Handle classHandle = type_info_get_class_no_exn(typeHandle);
        if (!classHandle) {
            assert(!lazy);
            return NULL;
        }
        type = typeManager.getObjectType(classHandle);
    } else if (type_info_is_primitive(typeHandle)) {
        // value type
        Class_Handle valueTypeHandle = type_info_get_class(typeHandle);
        if (!valueTypeHandle)
            return NULL;
        type = typeManager.getValueType(valueTypeHandle);
    } else if (type_info_is_vector(typeHandle)) {
        // vector
        bool lazy = typeManager.isLazyResolutionMode();
        if (lazy && !type_info_is_resolved(typeHandle)) {
            Type* elemType = typeManager.getUnresolvedObjectType();
            uint32 dims =  type_info_get_num_array_dimensions(typeHandle);
            Type* arrayType = NULL;
            while (dims!=0) {
                arrayType = typeManager.getArrayType(arrayType==NULL ? elemType : arrayType);
                dims--;
            }
            assert(arrayType!=NULL && arrayType->isArrayType());
            return arrayType;
        }
        Type_Info_Handle elemTypeInfo = type_info_get_type_info(typeHandle);
        Type* elemType = getTypeFromDrlVMTypeHandle(elemTypeInfo);
        if (!elemType) {
            assert(!lazy);
            return NULL;
        }
        type = typeManager.getArrayType(elemType);
    } else {
        // should not get here
        assert(0);
    }
    return type;
}

const char*
CompilationInterface::getRuntimeHelperName(VM_RT_SUPPORT id){
    return vm_helper_get_name(id);
}

VM_RT_SUPPORT CompilationInterface::str2rid( const char * helperName ) {
    return vm_helper_get_by_name(helperName);
}

void*        
CompilationInterface::getRuntimeHelperAddress(VM_RT_SUPPORT id) {
    return vm_get_rt_support_addr(id);
}

void*        
CompilationInterface::getRuntimeHelperAddressForType(VM_RT_SUPPORT id, Type* type) {
    Class_Handle handle = NULL;
    if (type != NULL && type->isNamedType())
        handle = (Class_Handle) ((NamedType *)type)->getVMTypeHandle();
    void* addr = vm_get_rt_support_addr_optimized(id, handle);
    assert(addr != NULL);
    return addr;
}

HELPER_CALLING_CONVENTION 
CompilationInterface::getRuntimeHelperCallingConvention(VM_RT_SUPPORT id) {
    return vm_helper_get_calling_convention(id);
}

bool
CompilationInterface::isInterruptible(VM_RT_SUPPORT id)  {
    return INTERRUPTIBLE_ALWAYS == vm_helper_get_interruptibility_kind(id);
}

bool
CompilationInterface::mayBeInterruptible(VM_RT_SUPPORT id)  {
    return INTERRUPTIBLE_NEVER != vm_helper_get_interruptibility_kind(id);
}

bool
CompilationInterface::compileMethod(MethodDesc *method) {
    if (Log::isEnabled()) {
        Log::out() << "Jitrino requested compilation of " <<
            method->getParentType()->getName() << "::" <<
            method->getName() << method->getSignatureString() << ::std::endl;
    }
    JIT_Result res = vm_compile_method(getJitHandle(), method->getMethodHandle());
    return res == JIT_SUCCESS ? true : false;
}



void* 
CompilationInterface::loadStringObject(MethodDesc* enclosingMethodDesc,
                                                uint32 stringToken) {
    Class_Handle enclosingDrlVMClass = enclosingMethodDesc->getParentHandle();
    return class_get_const_string_intern_addr(enclosingDrlVMClass,stringToken);
}

Type*
CompilationInterface::getConstantType(MethodDesc* enclosingMethodDesc,
                                         uint32 constantToken) {
    Class_Handle enclosingDrlVMClass = enclosingMethodDesc->getParentHandle();
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
CompilationInterface::getConstantValue(MethodDesc* enclosingMethodDesc,
                                          uint32 constantToken) {
    Class_Handle enclosingDrlVMClass = enclosingMethodDesc->getParentHandle();
    return class_get_const_addr(enclosingDrlVMClass,constantToken);
}

MethodDesc*
CompilationInterface::getOverriddenMethod(NamedType* type, MethodDesc *methodDesc) {
    if (type->isUnresolvedType()) {
        return NULL;
    }
    Method_Handle m = method_find_overridden_method((Class_Handle) type->getVMTypeHandle(),
                         methodDesc->getMethodHandle());
    if (!m)
        return NULL;
    return getMethodDesc(m);
}

ClassHierarchyMethodIterator* 
CompilationInterface::getClassHierarchyMethodIterator(ObjectType* baseType, MethodDesc* methodDesc) {
    return new (getMemManager()) ClassHierarchyMethodIterator(*this, baseType, methodDesc);
}

void         CompilationInterface::setNotifyWhenClassIsExtended(ObjectType * type, 
                                                                   void * callbackData) {
    void * typeHandle = type->getVMTypeHandle();
    vm_register_jit_extended_class_callback(getJitHandle(), (Class_Handle) typeHandle,callbackData);
}

void         CompilationInterface::setNotifyWhenMethodIsOverridden(MethodDesc * methodDesc, 
                                                                      void * callbackData) {
    Method_Handle drlMethod = methodDesc->getMethodHandle();
    vm_register_jit_overridden_method_callback(getJitHandle(), drlMethod, callbackData);
}

void         CompilationInterface::setNotifyWhenMethodIsRecompiled(MethodDesc * methodDesc, 
                                                                      void * callbackData) {
    Method_Handle drlMethod = methodDesc->getMethodHandle();
    vm_register_jit_recompiled_method_callback(getJitHandle(),drlMethod,callbackData);
}

void CompilationInterface::sendCompiledMethodLoadEvent(MethodDesc* methodDesc, MethodDesc* outerDesc,
        uint32 codeSize, void* codeAddr, uint32 mapLength, 
        AddrLocation* addrLocationMap, void* compileInfo) {

    Method_Handle method = methodDesc->getMethodHandle();
    Method_Handle outer  = outerDesc->getMethodHandle();

    compiled_method_load(method, codeSize, codeAddr, mapLength, addrLocationMap, compileInfo, outer); 
}

void * VMInterface::getHeapBase() {
    return vm_heap_base_address();
}

void * VMInterface::getHeapCeiling() {
    return vm_heap_ceiling_address();
}

ObjectType * CompilationInterface::resolveClassUsingBootstrapClassloader( const char * klassName ) {
    Class_Handle cls = class_load_class_by_name_using_bootstrap_class_loader(klassName);
    if( NULL == cls ) {
        return NULL;
    }
    return getTypeManager().getObjectType(cls);
};


MethodDesc* CompilationInterface::resolveMethod( ObjectType* klass, const char * methodName, const char * methodSig) {
    Class_Handle cls = (Class_Handle)klass->getVMTypeHandle();
    assert( NULL != cls );  
    Method_Handle mh = class_lookup_method_recursively( cls, methodName, methodSig);
    if( NULL == mh ) {
        return NULL;
    }
    return getMethodDesc(mh, NULL);
};

JIT_Handle
CompilationInterface::getJitHandle() const {
    return getCompilationContext()->getCurrentJITContext()->getJitHandle();
}




NamedType* MethodDesc::getThrowType(uint32 i) {
    assert(i<=method_number_throws(drlMethod));
    Class_Handle ch = method_get_throws(drlMethod, i);
    assert(ch);
    NamedType* res = compilationInterface->getTypeManager().getObjectType(ch);
    return res;
}

bool MethodDesc::hasAnnotation(NamedType* type) const {
    return method_has_annotation(drlMethod, (Class_Handle)type->getVMTypeHandle());
}

void FieldDesc::printFullName(::std::ostream &os) { 
    os<<getParentType()->getName()<<"::"<<field_get_name(drlField); 
}
void MethodDesc::printFullName(::std::ostream& os) {
    os<<getParentType()->getName()<<"::"<<getName()<<method_get_descriptor(drlMethod);
}

FieldDesc*    CompilationInterface::getFieldDesc(Field_Handle field) {
    FieldDesc* fieldDesc = fieldDescs->lookup(field);
    if (fieldDesc == NULL) {
        fieldDesc = new (memManager)
            FieldDesc(field,this,nextMemberId++);
        fieldDescs->insert(field,fieldDesc);
    }
    return fieldDesc;
}

MethodDesc*   CompilationInterface:: getMethodDesc(Method_Handle method, JIT_Handle jit) {
    assert(method);
    MethodDesc* methodDesc = methodDescs->lookup(method);
    if (methodDesc == NULL) {
        methodDesc = new (memManager)
            MethodDesc(method, jit, this, nextMemberId++);
        methodDescs->insert(method,methodDesc);
    }
    return methodDesc;
}

CompilationInterface::CompilationInterface(Compile_Handle c, 
                                           Method_Handle m, JIT_Handle jit, 
                                           MemoryManager& mm, OpenMethodExecutionParams& comp_params, 
                                           CompilationContext* cc, TypeManager& tpm) :
compilationContext(cc), memManager(mm),
typeManager(tpm), compilation_params(comp_params)
{
    fieldDescs = new (mm) PtrHashTable<FieldDesc>(mm,32);
    methodDescs = new (mm) PtrHashTable<MethodDesc>(mm,32);
    compileHandle = c;
    nextMemberId = 0;
    methodToCompile = NULL;
    methodToCompile = getMethodDesc(m, jit);
    flushToZeroAllowed = false;
}

void CompilationInterface::lockMethodData(void)    { 
    assert(methodToCompile);
    Method_Handle mh = methodToCompile->getMethodHandle();
    method_lock(mh);
}

void CompilationInterface::unlockMethodData(void)  { 
    assert(methodToCompile);
    Method_Handle mh = methodToCompile->getMethodHandle();
    method_unlock(mh);
}

Byte*   CompilationInterface::allocateCodeBlock(size_t size, size_t alignment, CodeBlockHeat heat, int32 id, 
bool simulate) {
    return method_allocate_code_block(methodToCompile->getMethodHandle(), getJitHandle(), 
        size, alignment, heat, id, simulate ? CAA_Simulate : CAA_Allocate);
}
Byte*        CompilationInterface::allocateDataBlock(size_t size, size_t alignment) {
    return method_allocate_data_block(methodToCompile->getMethodHandle(),getJitHandle(),size, alignment);
}
Byte*        CompilationInterface::allocateInfoBlock(size_t size) {
    size += sizeof(void *);
    Byte *addr = method_allocate_info_block(methodToCompile->getMethodHandle(),getJitHandle(),size);
    return (addr + sizeof(void *));
}
Byte*        CompilationInterface::allocateJITDataBlock(size_t size, size_t alignment) {
    return method_allocate_jit_data_block(methodToCompile->getMethodHandle(),getJitHandle(),size, alignment);
}
MethodDesc*     CompilationInterface::getMethodDesc(Method_Handle method) {
    return getMethodDesc(method, getJitHandle());
}


static uint32 getArrayDims(Class_Handle cl, uint32 cpIndex) {
    return class_get_num_array_dimensions(cl, (unsigned short)cpIndex);
}

static NamedType* getUnresolvedType(TypeManager& typeManager, Class_Handle enclClass, uint32 cpIndex) {
    uint32 arrayDims = getArrayDims(enclClass, cpIndex);
    NamedType * res = typeManager.getUnresolvedObjectType();
    while (arrayDims > 0) {
        res = typeManager.getArrayType(res);
        arrayDims --;
    }
    return res;
}

NamedType* CompilationInterface::resolveNamedType(Class_Handle enclClass, uint32 cpIndex) {
    //this method is allowed to use only for unresolved exception types
    Class_Handle ch = resolve_class(compileHandle,enclClass,cpIndex);
    if (ch == NULL) {
        return typeManager.getUnresolvedObjectType();
    }
    assert(!class_is_primitive(ch));
    ObjectType* res = typeManager.getObjectType(ch);    
    assert(res->isLikelyExceptionType()); //double check that this method is used only to resolve exception types when verifier is off
    return res;
}

NamedType* CompilationInterface::getNamedType(Class_Handle enclClass, uint32 cpIndex, ResolveNewCheck checkNew) {
    Class_Handle ch = NULL;
    if (typeManager.isLazyResolutionMode() && !class_is_cp_entry_resolved(compileHandle, enclClass, cpIndex)) {
        const char* className = const_pool_get_class_name(enclClass, cpIndex);
        bool forceResolve = VMMagicUtils::isVMMagicClass(className);
        if (!forceResolve) {
            return getUnresolvedType(typeManager, enclClass, cpIndex);
        }
    }
    if (checkNew == ResolveNewCheck_DoCheck) {
        ch = resolve_class_new(compileHandle,enclClass,cpIndex);
    } else {
        ch = resolve_class(compileHandle,enclClass,cpIndex);
    }
    if (ch == NULL) {
        return typeManager.getUnresolvedObjectType();
    }
    if (class_is_primitive(ch)) {
        return typeManager.getValueType(ch);
    }
    return typeManager.getObjectType(ch);    
}

Type* CompilationInterface::getTypeFromDescriptor(Class_Handle enclClass, const char* descriptor) {
    ClassLoaderHandle loader = class_get_class_loader(enclClass);
    Type_Info_Handle tih = type_info_create_from_java_descriptor(loader, descriptor);
    return getTypeFromDrlVMTypeHandle(tih);
}

MethodDesc* 
CompilationInterface::getSpecialMethod(Class_Handle enclClass, uint32 cpIndex) {
    Method_Handle res = NULL;
    bool lazy = typeManager.isLazyResolutionMode();
    if (!lazy || class_is_cp_entry_resolved(compileHandle, enclClass, cpIndex)) {
        res =  resolve_special_method(compileHandle,enclClass, cpIndex);
    }
    if (!res) return NULL;
    return getMethodDesc(res);
}    

MethodDesc* 
CompilationInterface::getInterfaceMethod(Class_Handle enclClass, uint32 cpIndex) {
    Method_Handle res = NULL;
    bool lazy = typeManager.isLazyResolutionMode();
    if (!lazy || class_is_cp_entry_resolved(compileHandle, enclClass, cpIndex)) {
        res =  resolve_interface_method(compileHandle,enclClass, cpIndex);
    }
    if (!res) return NULL;
    return getMethodDesc(res);
}    

MethodDesc* 
CompilationInterface::getStaticMethod(Class_Handle enclClass, uint32 cpIndex) {
    Method_Handle res = NULL;
    bool lazy = typeManager.isLazyResolutionMode();
    if (!lazy || class_is_cp_entry_resolved(compileHandle, enclClass, cpIndex)) {
        res =  resolve_static_method(compileHandle,enclClass, cpIndex);
    }
    if (!res) return NULL;
    return getMethodDesc(res);
}    

MethodDesc* 
CompilationInterface::getVirtualMethod(Class_Handle enclClass, uint32 cpIndex) {
    Method_Handle res = NULL;
    bool lazy = typeManager.isLazyResolutionMode();
    if (!lazy || class_is_cp_entry_resolved(compileHandle, enclClass, cpIndex)) {
        res =  resolve_virtual_method(compileHandle,enclClass, cpIndex);
    }
    if (!res) return NULL;
    return getMethodDesc(res);
}    


FieldDesc*  
CompilationInterface::getNonStaticField(Class_Handle enclClass, uint32 cpIndex, bool putfield) {
    Field_Handle res = NULL;
    bool lazy = typeManager.isLazyResolutionMode();
    if (!lazy || class_is_cp_entry_resolved(compileHandle, enclClass, cpIndex)) {
        res = resolve_nonstatic_field(compileHandle, enclClass, cpIndex, putfield);
    }
    if (!res) {
        return NULL;
    }
    return getFieldDesc(res);
}


FieldDesc*  
CompilationInterface::getStaticField(Class_Handle enclClass, uint32 cpIndex, bool putfield) {
    Field_Handle res = NULL;
    bool lazy = typeManager.isLazyResolutionMode();
    if (!lazy || class_is_cp_entry_resolved(compileHandle, enclClass, cpIndex)) {
        res = resolve_static_field(compileHandle, enclClass, cpIndex, putfield);
    }
    if (!res) {
        return NULL;
    }
    return getFieldDesc(res);
}

Type*
CompilationInterface::getFieldType(Class_Handle enclClass, uint32 cpIndex) {
    Java_Type drlType = (Java_Type)class_get_cp_field_type(enclClass, (unsigned short)cpIndex);
    bool lazy = typeManager.isLazyResolutionMode();
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
        
        case JAVA_TYPE_CLASS:    
                if (lazy) {
                    const char* fieldTypeName = const_pool_get_field_descriptor(enclClass, cpIndex);
                    assert(fieldTypeName);
                    return getTypeFromDescriptor(enclClass, fieldTypeName);
                } 
                return typeManager.getUnresolvedObjectType();

        case JAVA_TYPE_VOID:     // class_get_cp_field_type can't return VOID
        case JAVA_TYPE_STRING:   // class_get_cp_field_type can't return STRING
        default: assert(0);
    }
    assert(0);
    return NULL;
}


} //namespace Jitrino
