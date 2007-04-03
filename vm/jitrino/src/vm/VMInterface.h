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

#ifndef _VMINTERFACE_H_
#define _VMINTERFACE_H_

#include <ostream>
#include "open/em.h"
#include "open/types.h"
#include "jit_runtime_support.h"
#include "jit_intf.h"

namespace Jitrino {

// external and forward declarations
class TypeManager;
class Type;
class NamedType;
class ObjectType;
class MethodPtrType;
class MemoryManager;
class CompilationContext;
class CompilationInterface;
template <class ELEM_TYPE> class PtrHashTable;


class VMInterface {
public:
    //
    // VM specific methods for types
    //
    static void*       getSystemObjectVMTypeHandle();
    static void*       getSystemClassVMTypeHandle();
    static void*       getSystemStringVMTypeHandle();
    static void*       getArrayVMTypeHandle(void* elemVMTypeHandle,bool isUnboxed);
    static const char* getTypeName(void* vmTypeHandle);
    static const char* getTypeNameQualifier(void* vmTypeHandle);
    static void*       getSuperTypeVMTypeHandle(void* vmTypeHandle);
    static void*       getArrayElemVMTypeHandle(void* vmTypeHandle);
    static bool        isArrayType(void* vmTypeHandle);
    static bool        isArrayOfPrimitiveElements(void* vmTypeHandle);
    static bool        isEnumType(void* vmTypeHandle);
    static bool        isValueType(void* vmTypeHandle);
    static bool        isFinalType(void* vmTypeHandle);
    static bool        isLikelyExceptionType(void* vmTypeHandle);
    static bool        isInterfaceType(void* vmTypeHandle);
    static bool        isAbstractType(void* vmTypeHandle);
    static bool        needsInitialization(void* vmTypeHandle);
    static bool        isFinalizable(void* vmTypeHandle);
    static bool        isBeforeFieldInit(void* vmTypeHandle);
    static bool        getClassFastInstanceOfFlag(void* vmTypeHandle);
    static int         getClassDepth(void* vmTypeHandle);
    static bool        isInitialized(void* vmTypeHandle);
    static void*       getVTable(void* vmTypeHandle);
    static void*       getRuntimeClassHandle(void* vmTypeHandle);
    static void*       getAllocationHandle(void* vmTypeHandle);
    static bool        isSubClassOf(void* vmTypeHandle1,void* vmTypeHandle2);
    static uint32      getArrayElemOffset(void* vmElemTypeHandle,bool isUnboxed);
    static uint32      getObjectSize(void * vmTypeHandle);
    static uint32      getArrayLengthOffset();

    static void*       getTypeHandleFromAllocationHandle(void* vmAllocationHandle);
    static void*       getTypeHandleFromVTable(void* vtHandle);

    static uint32      flagTLSSuspendRequestOffset();
    static uint32      flagTLSThreadStateOffset();


    // returns true if vtable pointers are compressed
    static bool          areVTablePtrsCompressed() {return vm_vtable_pointers_are_compressed();}

    // returns size of vtable pointer (currently 4 if compressed and 8 otherwise)
    //static uint32      getVTablePtrSize() {return vm_get_vtable_ptr_size();}

    // returns the offset of an object's virtual table
    static uint32      getVTableOffset();
    // returns the base for all vtables (addend to compressed vtable pointer)
    static uint64      getVTableBase() {return vm_get_vtable_base();}

    // returns true if instance fields that are references are compressed
    static bool        areReferencesCompressed() {return vm_references_are_compressed();}

    //
    // returns the base for the heap (addend to compressed heap references)
    //
    static void*       getHeapBase();
    static void*       getHeapCeiling();


    static void        rewriteCodeBlock(Byte* codeBlock, Byte*  newCode, size_t size);
};


///////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////
//            O P A Q U E   D E S C' S  I M P L E M E N T E D   B Y    V M             //
///////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////

class TypeMemberDesc {
public:
    TypeMemberDesc(uint32 id, CompilationInterface* ci)
        : id(id), compilationInterface(ci) {}
    virtual ~TypeMemberDesc() {}

    uint32      getId() const { return id; }
    NamedType*  getParentType();
    bool        isParentClassIsLikelyExceptionType() const;


    virtual const char* getName() const           = 0;
    virtual const char* getSignatureString() const = 0;
    virtual void        printFullName(::std::ostream& os) = 0;
    virtual Class_Handle getParentHandle() const  = 0;

    virtual bool        isPrivate() const          = 0;
    virtual bool        isStatic() const          = 0;

protected:
    uint32 id;
    CompilationInterface* compilationInterface;

};

class FieldDesc : public TypeMemberDesc {
public:
    FieldDesc(Field_Handle field, CompilationInterface* ci, uint32 id) 
        : TypeMemberDesc(id, ci), drlField(field) {} 

        const char*   getName() const       {return field_get_name(drlField);}
        const char*   getSignatureString() const {return field_get_descriptor(drlField); }
        void          printFullName(::std::ostream &os);
        Class_Handle  getParentHandle() const;
        bool          isPrivate() const     {return field_is_private(drlField)?true:false;}
        bool          isStatic() const      {return field_is_static(drlField)?true:false;}
        //
        // this field is constant after it is initialized
        // can only be mutated by constructor (instance fields) or
        // type initializer (static fields)
        //
        bool          isInitOnly() const     {return field_is_final(drlField)?true:false;}    
        // accesses to field cannot be reordered or CSEed
        bool          isVolatile() const    {return field_is_volatile(drlField)?true:false;}
        //
        // this is a compile-time constant field -- static fields only
        //
        bool          isLiteral() const;
        Type*         getFieldType();
        uint32        getOffset() const; // for non-static fields
        void*         getAddress() const    {return field_get_address(drlField);} // for static fields
        Field_Handle  getFieldHandle() const  {return drlField; }

private:
    Field_Handle drlField;
};

class MethodDesc : public TypeMemberDesc {
public:
    MethodDesc(Method_Handle m, JIT_Handle jit, CompilationInterface* ci = NULL, uint32 id = 0)
        : TypeMemberDesc(id, ci), drlMethod(m),
        methodSig(method_get_signature(m)),
        handleMap(NULL),
        jitHandle(jit){}

        const char*  getName() const        {return method_get_name(drlMethod);}
        const char*  getSignatureString() const {return method_get_descriptor(drlMethod); }
        void         printFullName(::std::ostream& os);
        Class_Handle getParentHandle() const;

        bool         isPrivate() const      {return method_is_private(drlMethod)?true:false;}
        bool         isStatic() const       {return method_is_static(drlMethod)?true:false;}
        bool         isInstance() const     {return method_is_static(drlMethod)?false:true;}
        bool         isNative() const       {return method_is_native(drlMethod)?true:false;}
        bool         isSynchronized() const {return method_is_synchronized(drlMethod)?true:false;}
        bool         isNoInlining() const;
        bool         isFinal() const        {return method_is_final(drlMethod)?true:false;}
        bool         isVirtual() const      {return isInstance() && !isPrivate();}
        bool         isAbstract() const     {return method_is_abstract(drlMethod)?true:false;}
        // FP strict
        bool         isStrict() const       {return method_is_strict(drlMethod)?true:false;}
        bool         isRequireSecObject(); //FIXME drop???
        bool         isClassInitializer() const {return strcmp(getName(), "<clinit>") == 0; }
        bool         isInstanceInitializer() const {return strcmp(getName(), "<init>") == 0; }

        //
        // Method info
        //

        const Byte*  getByteCodes() const   {return method_get_byte_code_addr(drlMethod);}
        uint32       getByteCodeSize() const {return (uint32) method_get_byte_code_size(drlMethod);}
        uint16       getMaxStack() const    {return (uint16) method_get_max_stack(drlMethod);}
        uint32       getNumHandlers() const {return method_get_num_handlers(drlMethod);}
        void getHandlerInfo(unsigned index, unsigned* beginOffset, 
            unsigned* endOffset, unsigned* handlerOffset, unsigned* handlerClassIndex) const;
        uint32       getNumThrows() const {return method_number_throws(drlMethod);}
        NamedType*   getThrowType(uint32 i);
        bool         hasAnnotation(NamedType* type) const;

        //
        // accessors for method info, code and data
        //
        Byte*    getInfoBlock() const;
        uint32   getInfoBlockSize() const;
        Byte*    getCodeBlockAddress(int32 id) const;
        uint32   getCodeBlockSize(int32 id) const;

        // sets and gets MethodSideEffect property for the compiled method
        Method_Side_Effects getSideEffect() const;
        void setSideEffect(Method_Side_Effects mse);

        //
        //    Exception registration API. 
        //
        void        setNumExceptionHandler(uint32 numHandlers);
        void        setExceptionHandlerInfo(uint32 exceptionHandlerNumber,
            Byte*  startAddr,
            Byte*  endAddr,
            Byte*  handlerAddr,
            NamedType*  exceptionType,
            bool   exceptionObjIsDead);


        //
        // DRL kernel
        //
        bool         isOverridden() const   {return method_is_overridden(drlMethod)?true:false;}
        uint32       getOffset() const      {return method_get_offset(drlMethod);}
        void*        getIndirectAddress() const {return method_get_indirect_address(drlMethod);}

        uint32    getNumVars() const        {return method_vars_get_number(drlMethod);}

        Method_Handle    getMethodHandle() const   {return drlMethod;}

        //
        // handleMap method are used to register/unregister main map for all Container handlers
        void* getHandleMap() const {return handleMap;}
        void setHandleMap(void* hndMap) {handleMap = hndMap;}

        uint32    getNumParams() const;
        Type*     getParamType(uint32 paramIndex) const;
        Type*     getReturnType() const;

private:
    JIT_Handle   getJitHandle() const {return jitHandle;}
    Method_Handle               drlMethod;
    Method_Signature_Handle  methodSig;
    void* handleMap;
    JIT_Handle                  jitHandle;
};

class ClassHierarchyMethodIterator {
public:
    ClassHierarchyMethodIterator(CompilationInterface& compilationInterface, ObjectType* objType, MethodDesc* methodDesc);
    bool isValid() const { return valid; }
    bool hasNext() const;
    MethodDesc* getNext();

private:
    CompilationInterface& compilationInterface;
    bool valid;
    ChaMethodIterator iterator;
};


class CompilationInterface {
public:
    CompilationInterface(Compile_Handle c,
        Method_Handle m,
        JIT_Handle jit,
        MemoryManager& mm,
        OpenMethodExecutionParams& comp_params, 
        CompilationContext* cc, TypeManager& tpm);

    //
    //    System exceptions
    //
    enum SystemExceptionId {
        Exception_NullPointer = 0,
        Exception_ArrayIndexOutOfBounds,
        Exception_ArrayTypeMismatch,
        Exception_DivideByZero,
        Num_SystemExceptions
    };
    //
    //    Runtime helper methods
    //
    enum RuntimeHelperId {
        Helper_Null = 0,
        Helper_NewObj_UsingVtable,    // Object* obj = f(void * vtableId, uint32 size)
        Helper_NewVector_UsingVtable, // Vector* vec = f(uint32 numElem, void * arrVtableId);
        Helper_NewObj,             // Object* obj    = f(void* objTypeRuntimeId)
        Helper_NewVector,          // Vector* vec    = f(uint32 numElem, void* arrTypeRuntimeId)
        Helper_NewMultiArray,      // Array * arr    = f(void* arrTypeRuntimeId,uint32 numDims, uint32 dimN, ...,uint32 dim1)    
        Helper_LdInterface,        // Vtable* vtable = f(Object* obj, void* interfRuntimeId)
        Helper_LdRef,           // [String|Ref]* str = f(void* classRuntimeId, uint32 strToken)
        Helper_ObjMonitorEnter,    //                = f(Object* obj)
        Helper_ObjMonitorExit,     //                = f(Object* obj)
        Helper_TypeMonitorEnter,   //                = f(void* typeRuntimeId)
        Helper_TypeMonitorExit,    //                = f(void* typeRuntimeId)
        Helper_Cast,               // toType* obj    = f(Object* obj, void* toTypeRuntimeId)
        Helper_IsInstanceOf,       // [1(yes)/0(no)] = f(Object* obj, void* typeRuntimeId) a
        Helper_InitType,           //                = f(void* typeRutimeId)
        Helper_IsValidElemType,    // [1(yes)/0(no)] = f(Object* elem, Object* array)
        Helper_Throw_KeepStackTrace, //                f(Object* exceptionObj)
        Helper_Throw_SetStackTrace, //                 f(Object* exceptionObj)
        Helper_Throw_Lazy,          //                 f(MethodHandle /*of the <init>*/, .../*<init> params*/, ClassHandle)
        Helper_EndCatch,
        Helper_NullPtrException,   //                  f()
        Helper_ArrayBoundsException,//                 f()
        Helper_ElemTypeException,  //                  f()
        Helper_DivideByZeroException, //               f()
        Helper_Throw_LinkingException, //              f(uint32 ConstPoolNdx, void* classRuntimeId, uint32 opcode)
        Helper_CharArrayCopy,      //                = f(char[] src, uint32 srcPos, char[] dst, uint32 dstPos, uint32 len)
        Helper_DivI32,        // int32  z        = f(int32  x, int32  y)
        Helper_DivU32,        // uint32 z       = f(uint32 x, uint32 y)
        Helper_DivI64,        // int64  z       = f(int64  x, int64  y)
        Helper_DivU64,        // uint64 z        = f(uint64 x, uint64 y)
        Helper_DivSingle,     // float  z        = f(float  x, float  y)
        Helper_DivDouble,     // double z        = f(double x, double y)
        Helper_RemI32,        // int32  z        = f(int32  x, int32  y)
        Helper_RemU32,        // uint32 z       = f(uint32 x, uint32 y)
        Helper_RemI64,        // int64  z       = f(int64  x, int64  y)
        Helper_RemU64,        // uint64 z        = f(uint64 x, uint64 y)
        Helper_RemSingle,     // float  z        = f(float  x, float  y)
        Helper_RemDouble,     // double z        = f(double x, double y)
        Helper_MulI64,        // int64  z        = f(int64 x, int64 y) 
        Helper_ShlI64,        // int64  z        = f
        Helper_ShrI64,        // int64  z        = f
        Helper_ShruI64,       // int64  z        = f
        Helper_ConvStoI32,    // int32  x        = f(float x) 
        Helper_ConvStoI64,    // int64  x        = f(float x) 
        Helper_ConvDtoI32,    // int32  x        = f(double x) 
        Helper_ConvDtoI64,    // int64  x        = f(double x) 
        Helper_EnableThreadSuspension,//           f()
        Helper_GetTLSBase,    // int *           = f()
        Helper_MethodEntry, // f(MethodHandle)
        Helper_MethodExit,   // f(MethodHandle, void* ret_value)
        Helper_WriteBarrier,
        Num_Helpers
    };

    enum VmCallingConvention {
        CallingConvention_Drl = 0,
        CallingConvention_Stdcall,
        CallingConvention_Cdecl,
        Num_CallingConvention
    };


    static const char*   getRuntimeHelperName(RuntimeHelperId helperId);
    /**
     * Returns RuntimeHelperId by its string representation. Name comparison 
     * is case-sensitive.
     * If the helperName is unknown, then Helper_Null is returned.
     */
    static RuntimeHelperId str2rid( const char * helperName );

    VmCallingConvention getRuntimeHelperCallingConvention(RuntimeHelperId id);
    void*       getRuntimeHelperAddress(RuntimeHelperId);
    void*       getRuntimeHelperAddressForType(RuntimeHelperId, Type*);


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
    //FIXME
    //MethodDesc* resolveMethod(MethodDesc* enclosingMethod,uint32 methodToken){assert(false); return 0;}
    MethodDesc* resolveStaticMethod(MethodDesc* enclosingMethod,
        uint32 methodToken);
    MethodDesc* resolveInterfaceMethod(MethodDesc* enclosingMethod,
        uint32 methodToken);
    NamedType*  resolveNamedType(MethodDesc* enclosingMethod,
        uint32 typeToken);
    NamedType*  resolveNamedTypeNew(MethodDesc* enclosingMethod,
        uint32 typeToken);
    Type*       getFieldType(MethodDesc* enclosingMethodDesc, uint32 entryCPIndex);


    // resolve-by-name methods
    /**
     * Resolve a system class by its name. 
     * Returns NULL if no such class found.
     */
    ObjectType * resolveClassUsingBootstrapClassloader( const char * klassName );
    /**
     * Recursively looks up for a given method with a given signature in the given class.
     * Returns NULL if no such method found.
     */
    MethodDesc* resolveMethod(ObjectType * klass, const char * methodName, const char * methodSig);

    // Class type is a subclass of ch=mh->getParentType()  The function returns
    // a method description for a method overriding mh in type or in the closest
    // superclass of ch that overrides mh.
    MethodDesc* getOverriddenMethod(NamedType *type, MethodDesc * methodDesc);

    ClassHierarchyMethodIterator* getClassHierarchyMethodIterator(ObjectType* baseType, MethodDesc* methodDesc);

    void*        loadStringObject(MethodDesc* enclosingMethodDesc, uint32 stringToken);
    Type*        getConstantType(MethodDesc* enclosingMethodDesc, uint32 constantToken);
    const void*  getConstantValue(MethodDesc* enclosingMethodDesc, uint32 constantToken);
    const char*  getSignatureString(MethodDesc* enclosingMethodDesc, uint32 methodToken);

    // Memory allocation API
    // all of these are for the method being compiled
    Byte*   allocateCodeBlock(size_t size, size_t alignment, CodeBlockHeat heat, 
        int32 id, bool simulate);

    Byte*   allocateDataBlock(size_t size, size_t alignment);

    Byte*   allocateInfoBlock(size_t size);

    Byte*   allocateJITDataBlock(size_t size, size_t alignment);

    /**
     * Acquires a lock to protect method's data modifications (i.e. code/info 
     * block allocations, exception handlers registration, etc) in 
     * multi-threaded compilation.
     * The lock *must not* surround a code which may lead to execution of 
     * managed code, or a race and hang happen.
     * For example, the managed code execution may happen during a resolution
     * (invocation of resolve_XXX) to locate a class through a custom class 
     * loader.
     * Note, that the lock is *not* per-method, and shared across all the 
     * methods.
     */
    void    lockMethodData(void);

    /**
     * Releases a lock which protects method's data.
     */
    void    unlockMethodData(void);

    // methods that register JIT to be notified of various events
    void    setNotifyWhenClassIsExtended(ObjectType * type, void * callbackData);
    void    setNotifyWhenMethodIsOverridden(MethodDesc * methodDesc, void * callbackData);
    void    setNotifyWhenMethodIsRecompiled(MethodDesc * methodDesc, void * callbackData);

    // write barrier instructions
    bool    needWriteBarriers() const {
        return compilation_params.exe_insert_write_barriers;
    }

    bool    isBCMapInfoRequired() const {
        bool res = compilation_params.exe_do_code_mapping;
        // exe_do_code_mapping should be used for different ti related byte code
        // mapping calculations
        // full byte code mapping could be enabled by IRBuilder flag now 
        // this method used to access to byte code low level maps and
        // enables byte codes for stack traces only
        //        res = true;
        return res;
    }
    void    setBCMapInfoRequired(bool is_supported) const {
        compilation_params.exe_do_code_mapping = is_supported;
    }

    bool    isCompileLoadEventRequired() const {
        return compilation_params.exe_notify_compiled_method_load;
    }

    void    sendCompiledMethodLoadEvent(MethodDesc* methodDesc, MethodDesc* outerDesc,
        uint32 codeSize, void* codeAddr, uint32 mapLength, 
        AddrLocation* addrLocationMap, void* compileInfo);

    OpenMethodExecutionParams& getCompilationParams() const { 
        return compilation_params;
    }

    /**
     * Requests VM to request this JIT to synchronously (in the same thread) compile given method.
     * @param method method to compile
     * @return true on successful compilation, false otherwise
     */
    bool compileMethod(MethodDesc *method);

    // returns the method to compile
    MethodDesc*     getMethodToCompile() const {return methodToCompile;}

    TypeManager&    getTypeManager() const {return typeManager;}
    MemoryManager&  getMemManager() const {return memManager;}

    Type*           getTypeFromDrlVMTypeHandle(Type_Info_Handle);

    FieldDesc*      getFieldDesc(Field_Handle field);
    MethodDesc*     getMethodDesc(Method_Handle method);

    void setCompilationContext(CompilationContext* cc) {compilationContext = cc;}

    CompilationContext* getCompilationContext() const {return compilationContext;}

private:
    /** 
     * Settings per compilation session: vminterface + optimizer flags and so on..
     * Today we pass compilation interface through the all compilation. To avoid global
     * changes in JIT subcomponents interfaces CompilationContext struct is placed here.
     */
    CompilationContext* compilationContext;

    VM_RT_SUPPORT   translateHelperId(RuntimeHelperId runtimeHelperId);
    JIT_Handle      getJitHandle() const;
    MethodDesc*     getMethodDesc(Method_Handle method, JIT_Handle jit);

    MemoryManager&              memManager;
    PtrHashTable<FieldDesc>*    fieldDescs;
    PtrHashTable<MethodDesc>*   methodDescs;
    TypeManager&                typeManager;
    MethodDesc*                 methodToCompile;
    Compile_Handle              compileHandle;
    bool                        flushToZeroAllowed;
    uint32                      nextMemberId;
    OpenMethodExecutionParams&  compilation_params;
};

class GCInterface {
public:
    GCInterface(GC_Enumeration_Handle gcHandle) : gcHandle(gcHandle) {}
    virtual ~GCInterface() {}

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


class ThreadDumpEnumerator : public GCInterface {
public:
    ThreadDumpEnumerator() : GCInterface(NULL) {}

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

//
// The Persistent Instruction Id is used to generate a profile instruction map and to 
// feedback profile information into Jitrino.
//
class PersistentInstructionId {
public:
    PersistentInstructionId() 
        : methodDesc(NULL), localInstructionId((uint32)-1) {}

        PersistentInstructionId(MethodDesc* methodDesc, uint32 localInstructionId) 
            : methodDesc(methodDesc), localInstructionId(localInstructionId) {}

            bool isValid() const { return (methodDesc != NULL); }

            MethodDesc& getMethodDesc() const { return *methodDesc; }
            uint32 getLocalInstructionId() const { return localInstructionId; }

            // For IPF codegen to store block ids into pid
            bool hasValidLocalInstructionId() const { return localInstructionId != (uint32)-1; }

            bool operator==(const PersistentInstructionId& pid) { return methodDesc == pid.methodDesc && localInstructionId == pid.localInstructionId; }
private:
    MethodDesc* methodDesc;     // The source method at point the id was generated
    uint32 localInstructionId;  // The persistent local instruction id
};

inline ::std::ostream& operator<<(::std::ostream& os, const PersistentInstructionId& pid) { 
    os << (pid.isValid() ? pid.getMethodDesc().getName() : "NULL") << ":" 
        << (unsigned int) pid.getLocalInstructionId(); 
    return os;
}

} //namespace Jitrino 

#endif // _VMINTERFACE_H_
