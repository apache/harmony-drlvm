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
 * @version $Revision: 1.38.8.4.4.4 $
 */

#ifndef _VMINTERFACE_H_
#define _VMINTERFACE_H_

#include <cstring>
#include <string>
#ifdef __GNUC__
typedef ::std::size_t size_t;
#endif

#include "open/types.h"
#include "jit_export.h"
#include <iostream>
#include <ostream>
#include "PlatformDependant.h"

#define LOG_DOMAIN "jitrino"

namespace Jitrino {

// external declarations
class TypeManager;
class JITInstanceContext;
class Type;
class NamedType;
class ObjectType;
class MethodPtrType;
class PersistentInstructionId;
class MemoryManager;
class CompilationContext;
struct AddrLocation;

///////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////
//            O P A Q U E   D E S C' S  I M P L E M E N T E D   B Y    V M             //
///////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////

class ExceptionCallback {
public:
    virtual ~ExceptionCallback() {}

    virtual bool catchBlock(uint32 tryOffset,
                            uint32 tryLength,
                            uint32 handlerOffset,
                            uint32 handlerLength,
                            uint32 exceptionTypeToken) = 0;    
    virtual void finallyBlock(uint32 tryOffset,
                            uint32 tryLength,
                            uint32 handlerOffset,
                            uint32 handlerLength) = 0;
    virtual void filterBlock(uint32 tryOffset,
                            uint32 tryLength,
                            uint32 handlerOffset,
                            uint32 handlerLength,
                            uint32 expressionStart) = 0;
    virtual void faultBlock(uint32 tryOffset,
                            uint32 tryLength,
                            uint32 handlerOffset,
                            uint32 handlerLength) = 0;
};

//
// calli standalone sig, fptr callback of type sig, and methodDesc.parseMethodSig
//
class MethodSignatureDesc {
public:
    virtual ~MethodSignatureDesc() {}

    virtual uint32   getNumParams()                  = 0;
    virtual Type**   getParamTypes()                 = 0;
    virtual Type*    getParamType(uint32 paramIndex) = 0;
    virtual Type*    getReturnType()                 = 0;
};

class TypeMemberDesc {
public:
    virtual ~TypeMemberDesc() {}

    virtual const char* getName()            = 0;
    virtual const char* getSignatureString() = 0;
    virtual void  printFullName(::std::ostream& os) = 0;
    virtual NamedType*  getParentType()      = 0;
    virtual uint32      getId()              = 0;
    virtual bool        isPrivate()          = 0;
    virtual bool        isStatic()           = 0;
};

class FieldDesc : public TypeMemberDesc {
public:
    virtual ~FieldDesc() {}

    //
    // this field is constant after it is initialized
    // can only be mutated by constructor (instance fields) or
    // type initializer (static fields)
    //
    virtual bool    isInitOnly()         = 0;
    // accesses to field cannot be reordered or CSEed
    virtual bool    isVolatile()         = 0;
    //
    // this is a compile-time constant field -- static fields only
    //
    virtual bool    isLiteral()          = 0;
    virtual bool    isUnmanagedStatic()  = 0;
    virtual Type*   getFieldType()       = 0;
    virtual uint32  getOffset()          = 0;    // for non-static fields
    virtual void*   getAddress()         = 0;    // for static fields
};

class MethodDesc : public TypeMemberDesc {
public:
    virtual ~MethodDesc() {}

    virtual bool isNative()              = 0;
    virtual bool isSynchronized()        = 0;
    virtual bool isNoInlining()          = 0;
    virtual bool isStatic()              = 0;
    virtual bool isInstance()            = 0;    // In Java, all instance methods are virtual.  In CLI, not so.
    virtual bool isFinal()               = 0;
    virtual bool isVirtual()             = 0;
    virtual bool isAbstract()            = 0;    // isAbstract is probably not needed
    virtual bool isClassInitializer()    = 0;
    virtual bool isInstanceInitializer() = 0;
    virtual bool isMethodClassIsLikelyExceptionType() = 0;

    virtual bool isStrict()              = 0;    // floating-point strict
    virtual bool isRequireSecObject()    = 0;
    virtual bool isInitLocals()          = 0;    // call default constructor on local vars
    virtual bool isOverridden()          = 0;

    virtual bool        isJavaByteCodes()   = 0;    // are bytecodes Java or CLI?
    virtual const Byte* getByteCodes()      = 0;
    virtual uint32      getByteCodeSize()   = 0;
    virtual uint16      getMaxStack()       = 0;
    virtual uint32      getOffset()         = 0;
    virtual bool        isAddressFinal()    = 0;
    virtual void*       getIndirectAddress()= 0;
    virtual uint32      getNumHandlers()    = 0;
    virtual uint32      getNumThrows()    = 0;
    virtual NamedType*  getThrowType(uint32 i) = 0;
    virtual bool        hasAnnotation(NamedType* type) = 0;
    
    // Exception handler and signature parsing API
    virtual unsigned    parseJavaHandlers(ExceptionCallback&) = 0;
    virtual void        parseCliHandlers(ExceptionCallback&) = 0;
    virtual bool        isVarPinned(uint32 varIndex) = 0;
    virtual Type*       getVarType(uint32 varType) = 0;
    virtual uint32      getNumVars() = 0;
    virtual MethodSignatureDesc* getMethodSig() = 0;

    //
    // accessors for method info, code and data
    //
    virtual Byte*       getInfoBlock()        = 0;
    virtual uint32      getInfoBlockSize()    = 0;
    virtual Byte*       getCodeBlockAddress(int32 id) = 0;
    virtual uint32      getCodeBlockSize(int32 id)    = 0;


    //
    // accessors for DynoptInfo of this method
    // 
    virtual uint32      getUniqueId() = 0;
    
    virtual void* getHandleMap() = 0;
    virtual void setHandleMap(void* ) = 0;
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

class ClassHierarchyIterator {
public:
    virtual ~ClassHierarchyIterator() {}

    virtual bool isValid() = 0; // true if iterator is valid
    virtual bool hasNext() = 0; // true if iterator is not done 
    virtual ObjectType* getNext() = 0; // return next class in iterator and advance iterator
};

class ClassHierarchyMethodIterator {
public:
    virtual ~ClassHierarchyMethodIterator() {}
    virtual bool hasNext() = 0; // true if iterator is not done 
    virtual MethodDesc* getNext() = 0; // return next class in iterator and advance iterator
};


class CompilationInterface {
public:

    CompilationInterface(CompilationContext* cc) : compilationContext(cc){}
    virtual ~CompilationInterface() {}

    virtual TypeManager&  getTypeManager() = 0;
    //
    // returns the method to compile
    //
    virtual MethodDesc*   getMethodToCompile() = 0;
    //
    //returns the owner of a method
    //
    virtual Class_Handle methodGetClass(MethodDesc* method) = 0;
    //
    // resolution methods
    //
    virtual MethodSignatureDesc* resolveSignature(MethodDesc* enclosingMethodDesc,
                                                  uint32 sigToken) = 0;
    virtual FieldDesc*  resolveField(MethodDesc* enclosingMethod,
                                     uint32 fieldToken,
                                     bool putfield) = 0;
    virtual FieldDesc*  resolveFieldByIndex(NamedType* klass, int index, NamedType **fieldType) = 0;
    virtual FieldDesc*  resolveStaticField(MethodDesc* enclosingMethod,
                                           uint32 fieldToken,
                                           bool putfield) = 0;
    virtual MethodDesc* resolveVirtualMethod(MethodDesc* enclosingMethod,
                                             uint32 methodToken) = 0;
    virtual MethodDesc* resolveSpecialMethod(MethodDesc* enclosingMethod,
                                             uint32 methodToken) = 0;
    virtual MethodDesc* resolveMethod(MethodDesc* enclosingMethod,
                                      uint32 methodToken) = 0;
    virtual MethodDesc* resolveStaticMethod(MethodDesc* enclosingMethod,
                                            uint32 methodToken) = 0;
    virtual MethodDesc* resolveInterfaceMethod(MethodDesc* enclosingMethod,
                                               uint32 methodToken) = 0;
    virtual NamedType*  resolveNamedType(MethodDesc* enclosingMethod,
                                         uint32 typeToken) = 0;
    virtual NamedType*  resolveNamedTypeNew(MethodDesc* enclosingMethod,
                                            uint32 typeToken) = 0;
    virtual Type*       getFieldType(MethodDesc* enclosingMethodDesc, uint32 entryCPIndex) = 0;
    virtual const char* methodSignatureString(MethodDesc* enclosingMethodDesc, uint32 methodToken) = 0;
    virtual void*       loadStringObject(MethodDesc* enclosingMethod,
                                              uint32 stringToken)= 0;
    virtual void*       loadToken(MethodDesc* enclosingMethod,uint32 token) = 0;
    virtual Type*       getConstantType(MethodDesc* enclosingMethod,
                                             uint32 constantToken) = 0;
    virtual const void* getConstantValue(MethodDesc* enclosingMethod,
                                              uint32 constantToken) = 0;
        // resolve-by-name methods
    /**
     * Resolve a system class by its name. 
     * Returns NULL if no such class found.
     */
    virtual ObjectType *    resolveClassUsingBootstrapClassloader( const char * klassName ) = 0;
    /**
     * Recursively looks up for a given method with a given signature in the given class.
     * Returns NULL if no such method found.
     */
    virtual MethodDesc* resolveMethod(ObjectType * klass, const char * methodName, const char * methodSig) = 0;

    // Class type is a subclass of ch=mh->getParentType()  The function returns
    // a method description for a method overriding mh in type or in the closest
    // superclass of ch that overrides mh.
    virtual MethodDesc* getOverriddenMethod(NamedType *type, MethodDesc * methodDesc) = 0;

    // Return NULL if an iterator is not available for this type
    virtual ClassHierarchyIterator* getClassHierarchyIterator(ObjectType* baseType) = 0;

    // Return NULL if an iterator is not available for this type and method
    virtual ClassHierarchyMethodIterator* getClassHierarchyMethodIterator(ObjectType* baseType, MethodDesc* methodDesc) = 0;

    // prints message, something about line and file, and hard-exits
    virtual void hardAssert(const char *message, int line, const char *file) = 0;

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
    //
    // Code block heat - used when a method is split into hot and cold parts
    //
    enum CodeBlockHeat {
        CodeBlockHeatMin,
        CodeBlockHeatDefault,
        CodeBlockHeatMax
    };

    virtual void*        getRuntimeHelperAddress(RuntimeHelperId) = 0;
    virtual void*        getRuntimeHelperAddressForType(RuntimeHelperId id, Type* type) = 0;
    static const char*   getRuntimeHelperName(RuntimeHelperId helperId);
    /**
     * Returns RuntimeHelperId by its string representation. Name comparison 
     * is case-sensitive.
     * If the helperName is unknown, then Helper_Null is returned.
     */
    static RuntimeHelperId str2rid( const char * helperName );

    //
    // method side effects (for lazy exception throwing optimization)
    //
    enum MethodSideEffect {
        MSE_UNKNOWN,
        MSE_YES,
        MSE_NO,
        MSE_NULL_PARAM
    };

    virtual MethodSideEffect getMethodHasSideEffect(MethodDesc *m) = 0;
    virtual void             setMethodHasSideEffect(MethodDesc *m, MethodSideEffect mse) = 0;

    //
    //    Exception registration API. 
    //    All functions a for the method being compiled
    //
    virtual void        setNumExceptionHandler(uint32 numHandlers) = 0;
    virtual void        setExceptionHandlerInfo(uint32 exceptionHandlerNumber,
                                                Byte*  startAddr,
                                                Byte*  endAddr,
                                                Byte*  handlerAddr,
                                                NamedType*  exceptionType,
                                                bool   exceptionObjIsDead) = 0;
    //
    // returns true if instance fields that are references are compressed
    //
    virtual bool         areReferencesCompressed() = 0;
    //
    // returns the base for the heap (addend to compressed heap references)
    //
    virtual void*        getHeapBase() = 0;
    //
    // returns the offset of an object's virtual table
    //
    virtual uint32       getVTableOffset() = 0;
    //
    // returns true if vtable pointers are compressed
    //
    virtual bool         areVTablePtrsCompressed() = 0;
    //
    // returns size of vtable pointer (currently 4 if compressed and 
    // 8 otherwise)
    //
    virtual uint32       getVTablePtrSize() = 0;
    //
    // returns the base for all vtables (addend to compressed vtable pointer)
    //
    virtual uint64       getVTableBase() = 0;
    //
    // accessors for method info, code and data
    //
    virtual Byte*        getInfoBlock(MethodDesc*)        = 0;
    virtual uint32       getInfoBlockSize(MethodDesc*)    = 0;
    virtual Byte*        getCodeBlockAddress(MethodDesc*, int32 id) = 0;
    virtual uint32       getCodeBlockSize(MethodDesc*, int32 id)    = 0;
    //
    // Memory allocation API
    // all of these are for the method being compiled
    virtual Byte*        allocateCodeBlock(size_t size, size_t alignment, CodeBlockHeat, int32, bool) = 0; 
    virtual Byte*        allocateDataBlock(size_t size, size_t alignment) = 0;
    virtual Byte*        allocateInfoBlock(size_t size) = 0;
    virtual Byte*        allocateJITDataBlock(size_t size, size_t alignment)  = 0;
    virtual Byte*        allocateMemoryBlock(size_t)   = 0;
    //
    //
    
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
    virtual void         lockMethodData(void)   = 0;
    
    /**
     * Releases a lock which protects method's data.
     */
    virtual void         unlockMethodData(void) = 0;
    
    //
    // methods that register JIT to be notified of various events
    //
    virtual void         setNotifyWhenClassIsExtended(ObjectType * type, 
                                                      void *       callbackData) = 0;
    virtual void         setNotifyWhenMethodIsOverridden(MethodDesc * methodDesc, 
                                                         void *       callbackData) = 0;
    virtual void         setNotifyWhenMethodIsRecompiled(MethodDesc * methodDesc, 
                                                         void *       callbackData) = 0; 

    //
    
    // write barrier instructions
    virtual bool         insertWriteBarriers()        = 0;

    // flush to zero allowed on floating-pointer operations
    virtual bool         isFlushToZeroAllowed()         = 0;

    // produce BC to native map info
    virtual bool isBCMapInfoRequired() = 0;

    virtual bool isCompileLoadEventRequired() = 0;

    // send compile
    virtual void sendCompiledMethodLoadEvent(MethodDesc * methodDesc, 
        uint32 codeSize, void* codeAddr, uint32 mapLength, 
        AddrLocation* addrLocationMap, void* compileInfo) = 0;

    // get compilation params
    virtual OpenMethodExecutionParams& getCompilationParams() = 0;

    // synchronization inlining
    struct ObjectSynchronizationInfo {
        uint32 threadIdReg;      // the register number that holds id of the current thread
        uint32 syncHeaderOffset; // offset in bytes of the sync header from the start of the object   
        uint32 syncHeaderWidth;  // width in bytes of the sync header
        uint32 lockOwnerOffset;  // offset in bytes of the lock owner field from the start of the object
        uint32 lockOwnerWidth;   // width in bytes of the lock owner field in the sync header
        bool   jitClearsCcv;     // whether the JIT needs to clear ar.ccv
    };
    //
    //  Returns true if JIT may inline VM functionality for monitorenter and monitorexit
    //  If true is returned 'syncInfo' is filled in with the synchronization parameters.
    //
    virtual bool         mayInlineObjectSynchronization(ObjectSynchronizationInfo & syncInfo) = 0;

    enum VmCallingConvention {
        CallingConvention_Drl = 0,
        CallingConvention_Stdcall,
        CallingConvention_Cdecl,
        Num_CallingConvention
    };

    // 
    // Returns the calling convention for managed code.
    //
    virtual VmCallingConvention getManagedCallingConvention() = 0;
    virtual VmCallingConvention getRuntimeHelperCallingConvention(RuntimeHelperId id) = 0;

    /**
     * Requests VM to request this JIT to synchronously (in the same thread) compile given method.
     * @param method method to compile
     * @return true on successful compilation, false otherwise
     */
    virtual bool compileMethod(MethodDesc *method) = 0;

    /**
     * @param method JIT internal method descriptor
     * @return runtime handle of the corresponding VM object for the method 
     */
    virtual void* getRuntimeMethodHandle(MethodDesc *method) = 0;

    virtual CompilationContext* getCompilationContext() const {return compilationContext;}

protected:
    /** Settings per compilation session: vminterface + optimizer flags and so on..
      * Today we pass compilation interface through the all compilation. To avoid global
      * changes in JIT subcomponents interfaces CompilationContext struct is placed here.
      */
    CompilationContext* compilationContext;
};

// AddrLocation data structure should be put in VM-JIT interface
struct AddrLocation {
    void* start_addr;
    uint16 location;
};

class DataInterface {
public:
    virtual ~DataInterface() {}

    //
    // returns true if instance fields that are references are compressed
    //
    virtual bool         areReferencesCompressed() = 0;
    //
    // returns the base for the heap (addend to compressed heap references)
    //
    virtual void*        getHeapBase() = 0;
};


class GCInterface {
public:
    virtual ~GCInterface() {}

    virtual void enumerateRootReference(void** reference) = 0;
    virtual void enumerateCompressedRootReference(uint32* reference) = 0;
    virtual void enumerateRootManagedReference(void** slotReference, int slotOffset) = 0;
};



class BinaryRewritingInterface {
public:
    virtual ~BinaryRewritingInterface() {}

    virtual void rewriteCodeBlock(Byte* codeBlock, Byte* newCode, size_t length) = 0;
};

class Compiler {
public:
    virtual ~Compiler() {}

    //
    //  Return true if the method has been successfully compiled,
    //  false - otherwise
    //
    virtual bool compileMethod(CompilationInterface*) = 0;
};

// assert which works even in release mode
#define jitrino_assert(compInterface, e) { if (!(e)) { compInterface.hardAssert("Assertion failed", __LINE__, __FILE__); } }

} //namespace Jitrino 

#endif // _VMINTERFACE_H_
