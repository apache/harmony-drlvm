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
 * @version $Revision: 1.30.12.3.4.4 $
 */

#ifndef _DRLVMINTERFACEIMPL_H_
#define _DRLVMINTERFACEIMPL_H_

#include "Type.h"
#include "VMInterface.h"
#include "jit_export.h"
#include "jit_import.h"
#include "jit_runtime_support.h"
#include "jit_intf.h"
#include "mkernel.h"

#include <stdio.h>
#include <stdlib.h>

namespace Jitrino {

/**
 * @brief A lock used to protect method's data in multi-threaded compilation.
 */
extern Mutex g_compileLock;

//
// forward class definitions
//
class DrlVMCompilationInterface;

uint32 flagTLSSuspendRequestOffset();
uint32 flagTLSThreadStateOffset();

class DrlVMTypeManager : public TypeManager {
public:
    DrlVMTypeManager(MemoryManager& mm) : TypeManager(mm) {
        systemObjectVMTypeHandle = systemStringVMTypeHandle = NULL;
    }
    //
    // VM specific methods for TypeDesc
    //
    void*       getBuiltinValueTypeVMTypeHandle(Type::Tag);
    void*       getSystemObjectVMTypeHandle();
    void*       getSystemClassVMTypeHandle();
    void*       getSystemStringVMTypeHandle();
    void*       getArrayVMTypeHandle(void* elemVMTypeHandle,bool isUnboxed);
    const char* getTypeName(void* vmTypeHandle);
    const char* getTypeNameQualifier(void* vmTypeHandle);
    void*       getSuperTypeVMTypeHandle(void* vmTypeHandle) {
        return class_get_super_class((Class_Handle)vmTypeHandle);
    }
    const char* getMethodName(MethodDesc*);
    void*       getArrayElemVMTypeHandle(void* vmTypeHandle);
    bool        isArrayType(void* vmTypeHandle) {
        return class_is_array((Class_Handle)vmTypeHandle)?true:false;
    }
    bool        isArrayOfPrimitiveElements(void* vmTypeHandle);
    bool        isEnumType(void* vmTypeHandle);
    bool        isValueType(void* vmTypeHandle);
    bool        isLikelyExceptionType(void* vmTypeHandle);
    bool        isVariableSizeType(void* vmTypeHandle);
    bool        isFinalType(void* vmTypeHandle) {
        return class_property_is_final((Class_Handle)vmTypeHandle)?true:false;
    }
    bool        isInterfaceType(void* vmTypeHandle)  {
        return class_property_is_interface2((Class_Handle)vmTypeHandle)?true:false;
    }
    bool        isAbstractType(void* vmTypeHandle) {
        return class_property_is_abstract((Class_Handle)vmTypeHandle)?true:false;
    }
    bool        isSystemStringType(void* vmTypeHandle);
    bool        isSystemObjectType(void* vmTypeHandle);
    bool        isSystemClassType(void* vmTypeHandle);
    bool        isBeforeFieldInit(void* vmTypeHandle);
    bool        getClassFastInstanceOfFlag(void* vmTypeHandle);
    int         getClassDepth(void* vmTypeHandle);
    bool        needsInitialization(void* vmTypeHandle) {
        return class_needs_initialization((Class_Handle)vmTypeHandle)?true:false;
    }
    bool        isFinalizable(void* vmTypeHandle) {
        return class_is_finalizable((Class_Handle)vmTypeHandle)?true:false;
    }
    bool        isInitialized(void* vmTypeHandle) {
        return class_is_initialized((Class_Handle)vmTypeHandle)?true:false;
    }
    void*       getVTable(void* vmTypeHandle) {
        return (void *) class_get_vtable((Class_Handle)vmTypeHandle);
    }

    // 
    // In DRL, these are the same.
    //
    void*       getRuntimeClassHandle(void* vmTypeHandle) {
        return vmTypeHandle;
    }

    //
    // Allocation handle to be used with calls to runtime support functions for
    // object allocation
    //
    void*       getAllocationHandle(void* vmTypeHandle) {
        return (void *) class_get_allocation_handle((Class_Handle) vmTypeHandle);
    }

    uint32      getVTableOffset()
    {
        return object_get_vtable_offset();
    }

    void*       getTypeHandleFromAllocationHandle(void* vmAllocationHandle)
    {
        return allocation_handle_get_class((Allocation_Handle)vmAllocationHandle);
    }


    bool        isSubClassOf(void* vmTypeHandle1,void* vmTypeHandle2);
    uint32      getUnboxedOffset(void* vmTypeHandle);
    uint32      getArrayElemOffset(void* vmElemTypeHandle,bool isUnboxed);
    uint32      getBoxedSize(void * vmTypeHandle);
    uint32      getUnboxedSize(void* vmTypeHandle);
    uint32      getUnboxedAlignment(void* vmTypeHandle);
    uint32      getUnboxedNumFields(void* vmTypeHandle);
    FieldDesc*  getUnboxedFieldDesc(void* vmTypeHandle,uint32 index);
    uint32      getArrayLengthOffset();
    Type*       getUnderlyingType(void* enumVMTypeHandle);

    Type*       getTypeFromPrimitiveDrlVMDataType(VM_Data_Type drlDataType) {
        switch (drlDataType) {
            case VM_DATA_TYPE_INT8:    return getInt8Type();
            case VM_DATA_TYPE_UINT8:   return getUInt8Type();
            case VM_DATA_TYPE_INT16:   return getInt16Type();
            case VM_DATA_TYPE_UINT16:  return getUInt16Type();
            case VM_DATA_TYPE_INT32:   return getInt32Type();
            case VM_DATA_TYPE_UINT32:  return getUInt32Type();
            case VM_DATA_TYPE_INT64:   return getInt64Type();
            case VM_DATA_TYPE_UINT64:  return getUInt64Type();
            case VM_DATA_TYPE_INTPTR:  return getIntPtrType();
            case VM_DATA_TYPE_UINTPTR: return getUIntPtrType();
            case VM_DATA_TYPE_F8:      return getDoubleType();
            case VM_DATA_TYPE_F4:      return getSingleType();
            case VM_DATA_TYPE_BOOLEAN: return getBooleanType();
            case VM_DATA_TYPE_CHAR:    return getCharType();
            default: assert(0);
       }
       return NULL;
    }

private:
    void*        systemObjectVMTypeHandle;
    void*        systemClassVMTypeHandle;
    void*        systemStringVMTypeHandle;
};

class DrlVMMethodSignatureDesc : public MethodSignatureDesc {
public:
    DrlVMMethodSignatureDesc(Method_Signature_Handle ms,DrlVMCompilationInterface* ci) 
        : drlSigHandle(ms), compilationInterface(ci), paramTypes(NULL) {}
    uint32    getNumParams();
    Type*     getParamType(uint32 paramIndex);
    Type**    getParamTypes();
    Type*     getReturnType();
private:
    Method_Signature_Handle  drlSigHandle;
    DrlVMCompilationInterface* compilationInterface;
    Type**                     paramTypes;
};

////////////////////////////////////////////
///////// DrlVMFieldDesc//////////////////////
////////////////////////////////////////////

class DrlVMFieldDesc : public FieldDesc {
public:
    DrlVMFieldDesc(Field_Handle field,
                 DrlVMCompilationInterface* ci,
                 uint32 i) 
    : drlField(field), compilationInterface(ci), id(i) {} 

    uint32        getId()          {return id;}
    const char*   getName()        {return field_get_name(drlField);}
    const char*   getSignatureString() { return field_get_descriptor(drlField); }
    void          printFullName(::std::ostream &os) { os<<getParentType()->getName()<<"::"<<field_get_name(drlField); }
    NamedType*    getParentType();
    bool          isPrivate()      {return field_is_private(drlField)?true:false;}
    bool          isStatic()       {return field_is_static(drlField)?true:false;}
    bool          isInitOnly()     {return field_is_final(drlField)?true:false;}    
    bool          isVolatile()     {return field_is_volatile(drlField)?true:false;}
    bool          isLiteral();
    bool          isUnmanagedStatic();
    Type*         getFieldType();
    uint32        getOffset();
    void*         getAddress()     {return field_get_address(drlField);}

    // the following method to be used only by the DrlVM implementation
    Field_Handle  getFieldHandle()  {return drlField; }
private:
    Field_Handle                drlField;
    DrlVMCompilationInterface*    compilationInterface;
    uint32                        id;
};

////////////////////////////////////////////
///////// DRLMethodDesc ////////////////////
////////////////////////////////////////////

class DrlVMMethodDesc : public MethodDesc {
public:
    DrlVMMethodDesc(Method_Handle m,
                  DrlVMCompilationInterface* ci,
                  uint32 i,
                  JIT_Handle jit)
        : drlMethod(m), id(i),
          compilationInterface(ci),
          methodSig(method_get_signature(m),ci),
          handleMap(NULL),
          jitHandle(jit){}

    DrlVMMethodDesc(Method_Handle m, JIT_Handle j)
        : drlMethod(m),
        compilationInterface(NULL),
        methodSig(method_get_signature(m), NULL),
        handleMap(NULL),
        jitHandle(j){}                    
    DrlVMMethodDesc(Method_Handle m)
        : drlMethod(m), 
        compilationInterface(NULL),
        methodSig(method_get_signature(m),NULL),
        handleMap(NULL),
        jitHandle(NULL){}

    uint32       getId()           {return id;}
    const char*  getName()         {return method_get_name(drlMethod);}
    const char*  getSignatureString() {return method_get_descriptor(drlMethod); }
    void         printFullName(::std::ostream& os) {
                           os<<getParentType()->getName()<<"::"<<getName()<<method_get_descriptor(drlMethod);}
    NamedType*   getParentType();
    bool         isPrivate()       {return method_is_private(drlMethod)?true:false;}
    bool         isStatic()        {return method_is_static(drlMethod)?true:false;}
    bool         isInstance()      {return method_is_static(drlMethod)?false:true;}
    bool         isNative()        {return method_is_native(drlMethod)?true:false;}
    bool         isSynchronized()  {return method_is_synchronized(drlMethod)?true:false;}
    bool         isNoInlining();
    bool         isFinal()         {return method_is_final(drlMethod)?true:false;}
    bool         isVirtual()       {return isInstance() && !isPrivate();}
    bool         isAbstract()      {return method_is_abstract(drlMethod)?true:false;}
    bool         isStrict()        {return method_is_strict(drlMethod)?true:false;}
    bool         isRequireSecObject();
    bool         isInitLocals()    {return false;}
    bool         isClassInitializer() {return strcmp(getName(), "<clinit>") == 0; }
    bool         isInstanceInitializer() {return strcmp(getName(), "<init>") == 0; }
    bool         isMethodClassIsLikelyExceptionType();
    //
    // Method info
    //

    bool         isJavaByteCodes() {return true;}
    const Byte*  getByteCodes()    {return method_get_byte_code_addr(drlMethod);}
    uint32       getByteCodeSize() {return (uint32) method_get_byte_code_size(drlMethod);}
    uint16       getMaxStack()     {return (uint16) method_get_max_stack(drlMethod);}
    uint32       getNumHandlers()  {return method_get_num_handlers(drlMethod);}
    uint32       getNumThrows() {return method_number_throws(drlMethod);}
    NamedType*   getThrowType(uint32 i);

    //
    // accessors for method info, code and data
    //
    virtual Byte*    getInfoBlock();
    virtual uint32   getInfoBlockSize();
    virtual Byte*    getCodeBlockAddress(int32 id);
    virtual uint32   getCodeBlockSize(int32 id);
    virtual uint32      getUniqueId();
    
    //
    // DRL kernel
    //
    bool         isOverridden()    {return method_is_overridden(drlMethod)?true:false;}
    uint32       getOffset()       {return method_get_offset(drlMethod);}
    void*        getIndirectAddress(){return method_get_indirect_address(drlMethod);}
    bool         isAddressFinal();
    //
    // Signature and handler parsing
    //
    unsigned parseJavaHandlers(ExceptionCallback&);
    void     parseCliHandlers(ExceptionCallback&);
    
    uint32    getNumVars()            {return method_vars_get_number(drlMethod);}
    bool      isVarPinned(uint32 varIndex);
    Type*     getVarType(uint32 varIndex);

    MethodSignatureDesc* getMethodSig() {return &methodSig;}

    //
    // DRL specific methods
    //
    Method_Handle    getDrlVMMethod() const   {return drlMethod;}
    JIT_Handle      getJitHandle() const {return jitHandle;}
    //
    // handleMap method are used to register/unregister main map for all Container handlers
    virtual void* getHandleMap() {return handleMap;}
    virtual void setHandleMap(void* hndMap) {handleMap = hndMap;}

private:
    Method_Handle               drlMethod;
    uint32                      id;
    DrlVMCompilationInterface*    compilationInterface;
    DrlVMMethodSignatureDesc      methodSig;
    void* handleMap;
    JIT_Handle                  jitHandle;
};


class DrlVMDataInterface : public DataInterface {
public:
    DrlVMDataInterface() {}
    virtual bool          areReferencesCompressed();
    virtual void*         getHeapBase();
    virtual void*         getHeapCeiling();
};


class DrlVMClassHierarchyIterator : public ClassHierarchyIterator {
public:
    DrlVMClassHierarchyIterator(TypeManager& typeManager, ObjectType* objType)
        : typeManager(typeManager) {
        valid = (class_iterator_initialize(&iterator, (Class_Handle) objType->getVMTypeHandle()) != 0);
    }
    bool isValid() { return valid; }
    bool hasNext() { Class_Handle handle = class_iterator_get_current(&iterator); return handle != NULL; }
    ObjectType* getNext() { ObjectType* type = typeManager.getObjectType(class_iterator_get_current(&iterator)); class_iterator_advance(&iterator); return type; }

private:
    TypeManager& typeManager;
    bool valid;
    ChaClassIterator iterator;
};

class DrlVMClassHierarchyMethodIterator : public ClassHierarchyMethodIterator {
public:
    DrlVMClassHierarchyMethodIterator(DrlVMCompilationInterface& compilationInterface, ObjectType* objType, MethodDesc* methodDesc)
        : compilationInterface(compilationInterface) {
        valid = (method_iterator_initialize(&iterator, ((DrlVMMethodDesc*) methodDesc)->getDrlVMMethod(), (Class_Handle) objType->getVMTypeHandle()) != 0);
    }
    bool isValid() { return valid; }
    bool hasNext() { Method_Handle handle = method_iterator_get_current(&iterator); return handle != NULL; }
    MethodDesc* getNext();

private:
    DrlVMCompilationInterface& compilationInterface;
    bool valid;
    ChaMethodIterator iterator;
};

/////////////////////////////////
//  DRL Compilation Interface  //
/////////////////////////////////

class DrlVMCompilationInterface : public CompilationInterface {
public:
    DrlVMCompilationInterface(Compile_Handle c,
                            Method_Handle m,
                            JIT_Handle jit,
                            MemoryManager& mm,
                            OpenMethodExecutionParams& comp_params, 
                            CompilationContext* cc) 
        : CompilationInterface(cc), memManager(mm), fieldDescs(mm,32), methodDescs(mm,32),
          dataInterface(), typeManager(mm), compilation_params(comp_params)
    {
        compileHandle = c;
        nextMemberId = 0;
        typeManager.init(*this);
        methodToCompile = NULL;
        methodToCompile = getMethodDesc(m, jit);
        flushToZeroAllowed = !methodToCompile->isJavaByteCodes();
    }

    void setCompilationContext(CompilationContext* cc) {
        compilationContext = cc;
    }

    // returns the method to compile
    MethodDesc*    getMethodToCompile()    {return methodToCompile;}

    void*        getRuntimeHelperAddress(RuntimeHelperId);
    void*        getRuntimeHelperAddressForType(RuntimeHelperId, Type*);

    // sets and gets MethodSideEffect property for the compiled method
    CompilationInterface::MethodSideEffect getMethodHasSideEffect(MethodDesc *m);
    void setMethodHasSideEffect(MethodDesc *m, CompilationInterface::MethodSideEffect mse);

    //
    //    Exception registration API. 
    //    All functions a for the method being compiled
    //
    void        setNumExceptionHandler(uint32 numHandlers);
    void        setExceptionHandlerInfo(uint32 exceptionHandlerNumber,
                                        Byte*  startAddr,
                                        Byte*  endAddr,
                                        Byte*  handlerAddr,
                                        NamedType*  exceptionType,
                                        bool   exceptionObjIsDead);
    bool          areReferencesCompressed() {
        return dataInterface.areReferencesCompressed();
    }
    void*         getHeapBase() {
        return dataInterface.getHeapBase();
    }
    uint32        getVTableOffset() {
        return object_get_vtable_offset();
    }
    bool          areVTablePtrsCompressed() {
        return (vm_vtable_pointers_are_compressed() != 0);
    }
    uint32        getVTablePtrSize() {
        return vm_get_vtable_ptr_size();
    }
    uint64        getVTableBase() {
        return vm_get_vtable_base();
    }

    // to get the owner of a method
    Class_Handle methodGetClass(MethodDesc* method);
    // token resolution methods
    MethodSignatureDesc* resolveSignature(MethodDesc* enclosingMethodDesc,
                                          uint32 sigToken);
    FieldDesc*  resolveField(MethodDesc* enclosingMethod,
                             uint32 fieldToken,
                             bool putfield);
    FieldDesc*  resolveFieldByIndex(NamedType *klass, int index, NamedType **fieldType);
    FieldDesc*  resolveStaticField(MethodDesc* enclosingMethod,
                                   uint32 fieldToken,
                                   bool putfield);
    MethodDesc* resolveVirtualMethod(MethodDesc* enclosingMethod,
                                     uint32 methodToken);
    MethodDesc* resolveSpecialMethod(MethodDesc* enclosingMethod,
                                     uint32 methodToken);
    MethodDesc* resolveMethod(MethodDesc* enclosingMethod,
                                         uint32 methodToken);
    MethodDesc* resolveStaticMethod(MethodDesc* enclosingMethod,
                                    uint32 methodToken);
    MethodDesc* resolveInterfaceMethod(MethodDesc* enclosingMethod,
                                       uint32 methodToken);
    NamedType*  resolveNamedType(MethodDesc* enclosingMethod,
                                 uint32 typeToken);
    NamedType*  resolveNamedTypeNew(MethodDesc* enclosingMethod,
                                    uint32 typeToken);
    Type*       getFieldType(MethodDesc* enclosingMethodDesc, uint32 entryCPIndex);
    const char* methodSignatureString(MethodDesc* enclosingMethodDesc, uint32 methodToken);

        // resolve-by-name methods
    virtual ObjectType * resolveClassUsingBootstrapClassloader( const char * klassName );
    virtual MethodDesc * resolveMethod( ObjectType * klass, const char * methodName, const char * methodSig);

    void*        loadStringObject(MethodDesc* enclosingMethod,
                                 uint32 stringToken);
    void*        loadToken(MethodDesc* enclosingMethod,uint32 token);
    Type*        getConstantType(MethodDesc* enclosingMethod,
                                uint32 constantToken);
    const void*    getConstantValue(MethodDesc* enclosingMethod,
                                 uint32 constantToken);
    MethodDesc* getOverriddenMethod(NamedType *type, MethodDesc * methodDesc); 

    ClassHierarchyIterator* getClassHierarchyIterator(ObjectType* baseType);

    ClassHierarchyMethodIterator* getClassHierarchyMethodIterator(ObjectType* baseType, MethodDesc* methodDesc);

    void hardAssert(const char *message, int line, const char *file);

    // accessors for method info, code and data
    Byte*        getInfoBlock(MethodDesc*);
    uint32       getInfoBlockSize(MethodDesc*);
    Byte*        getCodeBlockAddress(MethodDesc*, int32 id);
    uint32       getCodeBlockSize(MethodDesc*, int32 id);

    // Memory allocation API
    // all of these are for the method being compiled
    Byte*        allocateCodeBlock(size_t size, size_t alignment, CodeBlockHeat heat, int32 id, 
                                   bool simulate) {
        uint32 drlHeat;
        if (heat == CodeBlockHeatMin)
            drlHeat = CODE_BLOCK_HEAT_COLD;
        else if (heat == CodeBlockHeatMax)
            drlHeat = CODE_BLOCK_HEAT_MAX/2;
        else {
            assert (heat == CodeBlockHeatDefault);
            drlHeat = CODE_BLOCK_HEAT_DEFAULT;
        }
        return method_allocate_code_block(methodToCompile->getDrlVMMethod(), getJitHandle(), 
            size, alignment, drlHeat, id, simulate ? CAA_Simulate : CAA_Allocate);
    }
    Byte*        allocateDataBlock(size_t size, size_t alignment) {
        return method_allocate_data_block(methodToCompile->getDrlVMMethod(),getJitHandle(),size, alignment);
    }
    Byte*        allocateInfoBlock(size_t size) {
        size += sizeof(void *);
        Byte *addr = method_allocate_info_block(methodToCompile->getDrlVMMethod(),getJitHandle(),size);
        return (addr + sizeof(void *));
    }
    Byte*        allocateJITDataBlock(size_t size, size_t alignment) {
        return method_allocate_jit_data_block(methodToCompile->getDrlVMMethod(),getJitHandle(),size, alignment);
    }
    // allocate memory block with no purpose specified
    Byte*        allocateMemoryBlock(size_t size) {
        return (Byte *)malloc(size);
    }

    void    lockMethodData(void)    { g_compileLock.lock();     }
    void    unlockMethodData(void)  { g_compileLock.unlock();   }

    // methods that register JIT to be notified of various events
    void         setNotifyWhenClassIsExtended(ObjectType * type, void * callbackData);
    void         setNotifyWhenMethodIsOverridden(MethodDesc * methodDesc, void * callbackData);
    void         setNotifyWhenMethodIsRecompiled(MethodDesc * methodDesc, void * callbackData);

    
    
    // write barriers stuff
    bool         insertWriteBarriers() {
        return compilation_params.exe_insert_write_barriers;
    }

    bool isBCMapInfoRequired() {
        bool res = compilation_params.exe_do_code_mapping;
        // exe_do_code_mapping should be used for different ti related byte code
        // mapping calculations
        // full byte code mapping could be enabled by IRBuilder flag now 
        // this method used to access to byte code low level maps and
        // enables byte codes for stack traces only
//        res = true;
        return res;
    }
    void setBCMapInfoRequired(bool is_supported) {
        compilation_params.exe_do_code_mapping = is_supported;
    }

    bool isCompileLoadEventRequired() {
        // additional compilation param is needed to handle this event
        return false;
    }

    virtual void sendCompiledMethodLoadEvent(MethodDesc * methodDesc, 
        uint32 codeSize, void* codeAddr, uint32 mapLength, 
        AddrLocation* addrLocationMap, void* compileInfo);

    virtual OpenMethodExecutionParams& getCompilationParams() { 
        return compilation_params;
    }

    // should flush to zero be allowed for floating-point operations ?
    bool         isFlushToZeroAllowed() {
        return flushToZeroAllowed;
    }
    //
    //  Returns true if jit may inline VM functionality for monitorenter and monitorexit
    //  If true is returned 'syncInfo' is filled in with the synchronization parameters.
    //
    bool         mayInlineObjectSynchronization(ObjectSynchronizationInfo & syncInfo);

    // 
    // Returns the calling convention for managed code.
    //
    VmCallingConvention getManagedCallingConvention() {
        switch (vm_managed_calling_convention()) {
        case CC_Vm:
            return CallingConvention_Drl;
        default:
            assert(0);
            return (VmCallingConvention) -1;
        };
    }
    VmCallingConvention getRuntimeHelperCallingConvention(RuntimeHelperId id);

    bool compileMethod(MethodDesc *method);

    void* getRuntimeMethodHandle(MethodDesc *method) {
        return ((DrlVMMethodDesc*)method)->getDrlVMMethod();
    }

    //
    // for internal use
    //
    TypeManager&       getTypeManager() {return typeManager;}
    MemoryManager&     getMemManager()  {return memManager;}
    Type*              getTypeFromDrlVMTypeHandle(Type_Info_Handle,
                                                  bool isManagedPointer);
    DrlVMFieldDesc*    getFieldDesc(Field_Handle field) {
        DrlVMFieldDesc* fieldDesc = fieldDescs.lookup(field);
        if (fieldDesc == NULL) {
            fieldDesc = new (memManager)
                DrlVMFieldDesc(field,this,nextMemberId++);
            fieldDescs.insert(field,fieldDesc);
        }
        return fieldDesc;
    }
    DrlVMMethodDesc*    getMethodDesc(Method_Handle method) {
        return getMethodDesc(method, getJitHandle());
    }

    DrlVMMethodDesc*    getMethodDesc(Method_Handle method, JIT_Handle jit) {
        assert(method);
        DrlVMMethodDesc* methodDesc = methodDescs.lookup(method);
        if (methodDesc == NULL) {
            methodDesc = new (memManager)
                DrlVMMethodDesc(method,this,nextMemberId++, jit);
            methodDescs.insert(method,methodDesc);
        }
        return methodDesc;
    }

private:
    Type*  getTypeFromDrlVMTypeHandle(Type_Info_Handle);
    VM_RT_SUPPORT translateHelperId(RuntimeHelperId runtimeHelperId);
    JIT_Handle getJitHandle() const;

    MemoryManager&               memManager;
    PtrHashTable<DrlVMFieldDesc>   fieldDescs;
    PtrHashTable<DrlVMMethodDesc>  methodDescs;
    DrlVMDataInterface             dataInterface;
    DrlVMTypeManager               typeManager;
    DrlVMMethodDesc*               methodToCompile;
    Compile_Handle               compileHandle;
    bool                         flushToZeroAllowed;
    uint32                       nextMemberId;
    OpenMethodExecutionParams&   compilation_params;
};

class DrlVMGCInterface : public GCInterface {
public:
    DrlVMGCInterface(GC_Enumeration_Handle gcHandle) : gcHandle(gcHandle) {}

    virtual void enumerateRootReference(void** reference) {
        vm_enumerate_root_reference(reference, FALSE);
    }

    virtual void enumerateCompressedRootReference(uint32* reference) {
        vm_enumerate_compressed_root_reference(reference, FALSE);
    }

    virtual void enumerateRootManagedReference(void** slotReference, int slotOffset) {
        vm_enumerate_root_interior_pointer(slotReference, slotOffset, FALSE);
    }

private:
    GC_Enumeration_Handle gcHandle;
};


class DrlVMThreadDumpEnumerator : public GCInterface {
public:
    DrlVMThreadDumpEnumerator() {}

    virtual void enumerateRootReference(void** reference) {
        vm_check_if_monitor(reference, 0, 0, 0, FALSE, 1);
    }

    virtual void enumerateCompressedRootReference(uint32* reference) {
        vm_check_if_monitor(0, 0, reference, 0, FALSE, 2);
    }

    virtual void enumerateRootManagedReference(void** slotReference, int slotOffset) {
        vm_check_if_monitor(slotReference, 0, 0, slotOffset, FALSE, 3);
    }
};

class DrlVMBinaryRewritingInterface : public BinaryRewritingInterface {
public:
    DrlVMBinaryRewritingInterface() {}
    virtual void rewriteCodeBlock(Byte* codeBlock, Byte* newCode, size_t size);
};

} //namespace Jitrino

#endif // _DRLVMINTERFACEIMPL_H_
