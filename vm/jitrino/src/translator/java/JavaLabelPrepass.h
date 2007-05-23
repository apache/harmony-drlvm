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
 * @author Intel, George A. Timoshenko
 * @version $Revision: 1.24.12.1.4.4 $
 *
 */

#ifndef _JAVALABELPREPASS_H_
#define _JAVALABELPREPASS_H_

#include "Log.h"
#include "Stl.h"
#include "open/types.h"
#include "VMInterface.h"
#include "BitSet.h"
#include "JavaByteCodeParser.h"
#include "Type.h"
#include "Opnd.h"
#include "ExceptionInfo.h"

namespace Jitrino {

class StateTable;

class VariableIncarnation : private Dlink {
public:
    VariableIncarnation(uint32 offset, uint32 block, Type*);
    void setMultipleDefs();
    void ldBlock(int32 blockNumber);
    Type* getDeclaredType();
    void setDeclaredType(Type*);

    // Link two chains of var incarnations and assign them a specified common type
    static void linkAndMergeIncarnations(VariableIncarnation*, VariableIncarnation*, Type*, TypeManager*);
    // Link two chains of var incarnations and assign them a common type
    static void linkAndMergeIncarnations(VariableIncarnation*, VariableIncarnation*, TypeManager*);
    // Link two chains of var incarnations
    static void linkIncarnations(VariableIncarnation*, VariableIncarnation*);
    // Merge a chain of var incarnations to assign them a specified common type
    void mergeIncarnations(Type*, TypeManager*);
    // Merge a chain of var incarnations to assign them a new type common for all incarnations
    void mergeIncarnations(TypeManager*);
    // Set common type for a chain of incarnations
    void setCommonType(Type*);
    void print(::std::ostream& out);

    Opnd* getOpnd();
    Opnd* getOrCreateOpnd(IRBuilder*);
    void createMultipleDefVarOpnd(IRBuilder*);
    void setTmpOpnd(Opnd*);
protected:
    void createVarOpnd(IRBuilder*);
private:
    friend class SlotVar;
    int32   definingOffset;  // offset where the def was found, -1 if multiple defs
    int32   definingBlock;   // block  where the def was found, -1 if spans basic block
    Type*   declaredType;
    Opnd*   opnd;
};

class SlotVar : private Dlink {
public:
    SlotVar(VariableIncarnation* varInc): var(varInc), linkOffset(0) {
        _prev = _next = NULL;
    }
    SlotVar(SlotVar* sv, MemoryManager& mm) {
        assert(sv);
        var = sv->getVarIncarnation();
        linkOffset = sv->getLinkOffset();
        _prev = _next = NULL;
        for (sv = (SlotVar*)sv->_next; sv; sv = (SlotVar*)sv->_next) {
            addVarIncarnations(sv, mm, sv->getLinkOffset());
        }
    }
    // Add var incarnations from SlotVar to the list.
    // Return true if var incarnation has been added.
    bool addVarIncarnations(SlotVar* var, MemoryManager& mm, uint32 linkOffset);
    VariableIncarnation* getVarIncarnation() {return var;}
    void mergeVarIncarnations(TypeManager* tm);
    uint32 getLinkOffset() {return linkOffset;}
    void print(::std::ostream& out);
private:
    VariableIncarnation* var;
    uint32 linkOffset;
};


class StateInfo {
public:
    StateInfo():  flags(0), stackDepth(0), stack(NULL), exceptionInfo(NULL) {}

    ExceptionInfo *getExceptionInfo()       { return exceptionInfo; }
    void           setCatchLabel()          { flags |= 1;           }
    void           setSubroutineEntry()     { flags |= 2;           }
    void           setFallThroughLabel()    { flags |= 4;           }
    void           clearFallThroughLabel()  { flags &=~4;           }
    void           setVisited()             { flags |= 8;           }
    bool           isCatchLabel()           { return (flags & 1) != 0;     }
    bool           isSubroutineEntry()      { return (flags & 2) != 0;     }
    bool           isFallThroughLabel()     { return (flags & 4) != 0;     }
    bool           isVisited()              { return (flags & 8) != 0;     }

    //
    // addExceptionInfo() adds both catch-blocks and handlers. 
    //     Catch-blocks should be listed in the same order as the 
    //     corresponding exception table entries were listed in byte-code. (according to VM spec)
    //
    void  addExceptionInfo(ExceptionInfo *info); 

    struct SlotInfo {
        Type *type;
        uint32 varNumber;
        uint16 slotFlags;
        SlotVar *vars;
        uint32 jsrLabelOffset;
        SlotInfo() : type(NULL), varNumber(0), slotFlags(0), vars(NULL), jsrLabelOffset(0){}
    };

    // remove all slots containing returnAddress for RET instruction with jsrNexOffset == offset
    void cleanFinallyInfo(uint32 offset);

    /* flags */
    enum {
        VarNumberIsSet = 0x01,
        IsNonNull      = 0x02,
        IsExactType    = 0x04,
        ChangeState    = 0x08,
        StackOpndAlive = 0x10,  // the following to get rid of phi nodes in the translator
        StackOpndSaved = 0x20   // the following to get rid of phi nodes in the translator
    };
    static bool isNonNull(uint32 flags)           { return (flags & IsNonNull)            != 0; }
    static bool isExactType(uint32 flags)         { return (flags & IsExactType)          != 0; }
    static uint32 setNonNull(uint32 flags,bool val) { 
        return (val ? (flags | IsNonNull) : (flags & ~IsNonNull));
    }
    static uint32 setExactType(uint32 flags,bool val){ 
        return (val ? (flags | IsExactType) : (flags & ~IsExactType));
    }
    static bool isStackOpndAlive(uint32 flags) {return (flags & StackOpndAlive) != 0;}
    static bool isStackOpndSaved(uint32 flags) {return (flags & StackOpndSaved) != 0;}
    static uint32 setStackOpndAlive(uint32 flags,bool val) {
        return (val ? (flags | StackOpndAlive) : (flags & ~StackOpndAlive));
    }

    static uint32 setStackOpndSaved(uint32 flags,bool val) {
        return (val ? (flags | StackOpndSaved) : (flags & ~StackOpndSaved));
    }

    static bool isVarNumberSet(struct SlotInfo s) { return (s.slotFlags & VarNumberIsSet) != 0; }
    static bool isNonNull(struct SlotInfo s)      { return (s.slotFlags & IsNonNull)      != 0; }
    static bool isExactType(struct SlotInfo s)    { return (s.slotFlags & IsExactType)    != 0; }
    static bool changeState(struct SlotInfo s)    { return (s.slotFlags & ChangeState)    != 0; }
    static void setVarNumber(struct SlotInfo *s)   { s->slotFlags |= VarNumberIsSet; }
    static void setNonNull(struct SlotInfo *s)     { s->slotFlags |= IsNonNull;      }
    static void setExactType(struct SlotInfo *s)   { s->slotFlags |= IsExactType;    }
    static void setChangeState(struct SlotInfo *s) { s->slotFlags |= ChangeState;    }
    static void print(struct SlotInfo s, ::std::ostream& os) {
        if (s.type == NULL)
            os << "null";
        else
            s.type->print(os);
        if (isVarNumberSet(s)) os << (int)s.varNumber<< ",";
        if (isNonNull(s))      os << ",nn";
        if (isExactType(s))    os << ",ex";
        if (changeState(s))    os << ",cs";
    }
    // add flags as needed
    friend class JavaLabelPrepass;
    friend class JavaByteCodeTranslator;
    int        flags;
    int        stackDepth;
    struct SlotInfo*  stack;
    ExceptionInfo *exceptionInfo;
};


class JavaLabelPrepass : public JavaByteCodeParserCallback {
public:
    typedef StlList<CatchBlock*> ExceptionTable;

    virtual ~JavaLabelPrepass() {
    }

    // for non-inlined methods the actual operands are null
    JavaLabelPrepass(MemoryManager& mm,
                     TypeManager& tm, 
                     MemoryManager& irManager,
                     MethodDesc&  md,
                     CompilationInterface& ci,
                     Opnd** actualArgs);    // NULL for non-inlined methods

    bool    isLabel(uint32 offset)            { return labels->getBit(offset); }
    bool    isSubroutineEntry(uint32 offset)  { return subroutines->getBit(offset); }
    bool    getHasJsrLabels()                 { return hasJsrLabels;}
    uint32  getNumLabels()                    { return numLabels;}
    uint32  getNumVars()                      { return numVars;}
    uint32  getLabelId(uint32 offset);
    void    print_loc_vars(uint32 offset, uint32 index);
    //
    // exception info
    //

    enum JavaVarType {
         None = 0, A = 1, I = 2, L = 3, F = 4, D = 5, RET = 6, // JSR return address
         NumJavaVarTypes = 7
    };

    ExceptionTable& getExceptionTable() {return exceptionTable;}

    ///////////////////////////////////////////////////////////////////////////
    // Java Byte code parser callbacks
    ///////////////////////////////////////////////////////////////////////////

    // called before each byte code to indicate the next byte code's offset
    void offset(uint32 offset);

    // called after each byte code offset is worked out
    void offset_done(uint32 offset) {}

    // called when an error occurs during the byte codes parsing
    void parseError();

    // called to initialize parsing
    void parseInit() {}

    // called to indicate end of parsing
    void parseDone();

    // Variable information
    VariableIncarnation* getVarInc(uint32 offset, uint32 index);
    VariableIncarnation* getOrCreateVarInc(uint32 offset, uint32 index, Type* type, VariableIncarnation* prev);
    void                 createMultipleDefVarOpnds(IRBuilder*);

    //
    // operand stack manipulation (to keep track of state only !)
    //
    struct StateInfo::SlotInfo topType();
    struct StateInfo::SlotInfo popType();
    void                    popAndCheck(Type *type);
    void                    popAndCheck(JavaVarType type);
    void                    pushType(struct StateInfo::SlotInfo slot);
    void                    pushType(Type *type, uint32 varNumber);
    void                    pushType(Type *type);
    bool isCategory2(struct StateInfo::SlotInfo slot) { return slot.type == int64Type || slot.type == doubleType; }

    //
    bool        allExceptionTypesResolved() {return problemTypeToken == MAX_UINT32;}
    unsigned    getProblemTypeToken() {return problemTypeToken;}

    // cut and paste from Java_Translator.cpp
    // field, method, and type resolution
    //
    const char*             methodSignatureString(uint32 cpIndex);
    StateInfo*              getStateInfo()  { return &stateInfo; }
    StateTable*             getStateTable() { return stateTable; }

    static JavaVarType getJavaType(Type *type) {
        assert(type);
        switch(type->tag) {
        case Type::Boolean:  case Type::Char:
        case Type::Int8:     case Type::Int16:     case Type::Int32:
            return I;
        case Type::Int64:
            return L;
        case Type::Single:
            return F;
        case Type::Double:
            return D;
        case Type::Array:           
        case Type::Object:
        case Type::NullObject:
        case Type::UnresolvedObject:
        case Type::SystemString:
        case Type::SystemObject:
        case Type::SystemClass:
        case Type::CompressedArray:           
        case Type::CompressedObject:
        case Type::CompressedNullObject:
        case Type::CompressedSystemString:
        case Type::CompressedSystemObject:
            return A;
        case Type::IntPtr: // reserved for JSR
            return RET;
        default:
            ::std::cerr << "UNKNOWN "; type->print(::std::cerr); ::std::cerr << ::std::endl;
            assert(0);
            return None;
        }
    }

    // 
    // helper functions
    //
    void genReturn    (Type *type);
    void genLoad      (Type *type, uint32 index);
    void genTypeLoad  (uint32 index);
    void genStore     (Type *type, uint32 index, uint32 offset);
    void genTypeStore (uint32 index, uint32 offset);
    void genArrayLoad (Type *type);
    void genTypeArrayLoad();
    void genArrayStore(Type *type);
    void genTypeArrayStore();
    void genBinary    (Type *type);
    void genUnary     (Type *type);
    void genShift     (Type *type);
    void genConv      (Type *from, Type *to);
    void genCompare   (Type *type);
    void invoke       (MethodDesc *mdesc);
    void pseudoInvoke (const char* mdesc);
    static  uint32  getNumArgsBySignature(const char*& methodSig);
    static  Type*   getRetTypeBySignature(CompilationInterface& ci, Class_Handle enclClass, const char* methodSig);

    // remaining instructions

    void nop();
    void aconst_null();
    void iconst(int32 val);
    void lconst(int64 val);
    void fconst(float val);
    void dconst(double val);
    void bipush(int8 val);
    void sipush(int16 val);
    void ldc(uint32 constPoolIndex);
    void ldc2(uint32 constPoolIndex);
    void iload(uint16 varIndex) ;
    void lload(uint16 varIndex) ;
    void fload(uint16 varIndex) ;
    void dload(uint16 varIndex) ;
    void aload(uint16 varIndex) ;
    void iaload() ;
    void laload() ;
    void faload() ;
    void daload() ;
    void aaload() ;
    void baload() ;
    void caload() ;
    void saload() ;
    void istore(uint16 varIndex, uint32 off) ;
    void lstore(uint16 varIndex, uint32 off) ;
    void fstore(uint16 varIndex, uint32 off) ;
    void dstore(uint16 varIndex, uint32 off) ;
    void astore(uint16 varIndex, uint32 off) ;
    void iastore() ;
    void lastore() ;
    void fastore() ;
    void dastore() ;
    void aastore() ;
    void bastore() ;
    void castore() ;
    void sastore() ;
    void pop() ;
    void pop2() ;
    void dup() ;
    void dup_x1() ;
    void dup_x2() ;
    void dup2() ;
    void dup2_x1() ;
    void dup2_x2() ;
    void swap() ;
    void iadd() ;
    void ladd() ;
    void fadd() ;
    void dadd() ;
    void isub() ;
    void lsub() ;
    void fsub() ;
    void dsub() ;
    void imul() ;
    void lmul() ;
    void fmul() ;
    void dmul() ;
    void idiv() ;
    void ldiv() ;
    void fdiv() ;
    void ddiv() ;
    void irem() ;
    void lrem() ;
    void frem() ;
    void drem() ;
    void ineg() ;
    void lneg() ;
    void fneg() ;
    void dneg() ;
    void ishl() ;
    void lshl() ;
    void ishr() ;
    void lshr() ;
    void iushr() ;
    void lushr() ;
    void iand() ;
    void land() ;
    void ior() ;
    void lor() ;
    void ixor() ;
    void lxor() ;
    void iinc(uint16 varIndex,int32 amount) ;
    void i2l() ;
    void i2f() ;
    void i2d() ;
    void l2i() ;
    void l2f() ;
    void l2d() ;
    void f2i() ;
    void f2l() ;
    void f2d() ;
    void d2i() ;
    void d2l() ;
    void d2f() ;
    void i2b() ;
    void i2c() ;
    void i2s() ;
    void lcmp() ;
    void fcmpl() ;
    void fcmpg() ;
    void dcmpl() ;
    void dcmpg() ;
    void ifeq(uint32 targetOffset,uint32 nextOffset);
    void ifne(uint32 targetOffset,uint32 nextOffset);
    void iflt(uint32 targetOffset,uint32 nextOffset);
    void ifge(uint32 targetOffset,uint32 nextOffset);
    void ifgt(uint32 targetOffset,uint32 nextOffset);
    void ifle(uint32 targetOffset,uint32 nextOffset);
    void if_icmpeq(uint32 targetOffset,uint32 nextOffset);
    void if_icmpne(uint32 targetOffset,uint32 nextOffset);
    void if_icmplt(uint32 targetOffset,uint32 nextOffset);
    void if_icmpge(uint32 targetOffset,uint32 nextOffset);
    void if_icmpgt(uint32 targetOffset,uint32 nextOffset);
    void if_icmple(uint32 targetOffset,uint32 nextOffset);
    void if_acmpeq(uint32 targetOffset,uint32 nextOffset);
    void if_acmpne(uint32 targetOffset,uint32 nextOffset);
    void goto_(uint32 targetOffset,uint32 nextOffset);
    void jsr(uint32 offset, uint32 nextOffset);
    void ret(uint16 varIndex);
    void tableswitch(JavaSwitchTargetsIter*);
    void lookupswitch(JavaLookupSwitchTargetsIter*);
    void incrementReturn();
    void ireturn(uint32 off);
    void lreturn(uint32 off);
    void freturn(uint32 off);
    void dreturn(uint32 off);
    void areturn(uint32 off);
    void return_(uint32 off);
    void getstatic(uint32 constPoolIndex) ;
    void putstatic(uint32 constPoolIndex) ;
    void getfield(uint32 constPoolIndex) ;
    void putfield(uint32 constPoolIndex) ;
    void invokevirtual(uint32 constPoolIndex) ;
    void invokespecial(uint32 constPoolIndex) ;
    void invokestatic(uint32 constPoolIndex) ;
    void invokeinterface(uint32 constPoolIndex,uint32 count) ;
    void new_(uint32 constPoolIndex) ;
    void newarray(uint8 type) ;
    void anewarray(uint32 constPoolIndex) ;
    void arraylength() ;
    void athrow() ;
    void checkcast(uint32 constPoolIndex) ;
    int  instanceof(const uint8* bcp, uint32 constPoolIndex, uint32 off) ;
    void monitorenter() ;
    void monitorexit() ;
    void multianewarray(uint32 constPoolIndex,uint8 dimensions) ;
    void ifnull(uint32 targetOffset,uint32 nextOffset);
    void ifnonnull(uint32 targetOffset,uint32 nextOffset);
    void pushCatchLabel(uint32 offset) {
        labelStack->push((uint8*)methodDesc.getByteCodes()+offset);
    }
    void pushRestart(uint32 offset) {
        labelStack->push((uint8*)methodDesc.getByteCodes()+offset);
    }
private:
    friend class JavaExceptionParser;
    friend struct CatchOffsetVisitor;
    friend class JavaByteCodeTranslator;

    typedef StlMultiMap<uint32, uint32> JsrEntryToJsrNextMap;
    typedef std::pair<JsrEntryToJsrNextMap::const_iterator, JsrEntryToJsrNextMap::const_iterator> JsrEntriesMapCIterRange;
    typedef StlMap<uint32, uint32> RetToSubEntryMap;

    // compilation environment
    MemoryManager&    memManager;
    TypeManager&    typeManager;
    MethodDesc&     methodDesc;
    CompilationInterface& compilationInterface;
    // simulates the stack operation
    int             blockNumber;
    StateInfo       stateInfo;
    StateTable*     stateTable;
    // information about variables
    StlHashMap<uint32,VariableIncarnation*> localVars;
    // basic label info
    bool            nextIsLabel;
    BitSet*         labels;
    BitSet*         subroutines;
    uint32*         labelOffsets;    // array containing offsets of labels
    uint32          numLabels;
    uint32          numVars; 
    bool            isFallThruLabel;
    // exception info
    uint32          numCatchHandlers;
    // Java JSR
    bool            hasJsrLabels;
    //
    // mapping [Subroutine entry offset] -> [JSR next offset (=offset of instruction that immediately follows the JSR) ]
    //
    JsrEntryToJsrNextMap jsrEntriesMap;
    //
    // mapping [RET offset] -> [Soubroutine entry offset (target of JSR inst)]
    //
    RetToSubEntryMap retToSubEntryMap;

    // helpers
    Type           *int32Type, *int64Type, *singleType, *doubleType;
    ExceptionTable exceptionTable;

    // if an exception type can not be resolved, its token is being kept here
    unsigned       problemTypeToken;

    // private helper methods
    void setLabel(uint32 offset);
    void setSubroutineEntry(uint32 offset) { subroutines->setBit(offset,true); }
    void checkTargetForRestart(uint32 target);
    void propagateStateInfo(uint32 offset, bool isFallthru);
    void setJsrLabel(uint32 offset);
    void setStackVars();
    void propagateLocalVarToHandlers(uint32 varIndex);
    RetToSubEntryMap* getRetToSubEntryMapPtr() { return &retToSubEntryMap; }
};



class StateTable 
{
public:
    virtual ~StateTable() {
    }

    StateTable(MemoryManager& mm,TypeManager& tm, JavaLabelPrepass& jlp, uint32 size, uint32 numvars) :
               memManager(mm), typeManager(tm), prepass(jlp),
               hashtable(mm), maxDepth(numvars), numVars(numvars)
               {
                    assert(sizeof(POINTER_SIZE_INT)>=sizeof(uint32));
                    assert(sizeof(uint32*)>=sizeof(uint32));
               }
    StateInfo *getStateInfo(uint32 offset) {
        return hashtable[offset];
    }
    StateInfo *createStateInfo(uint32 offset) {
        StateInfo *state = hashtable[offset]; //lookup((uint32*)(POINTER_SIZE_INT)offset);
        if (state == NULL) {
            state = new (memManager) StateInfo();
            hashtable[offset] = state;
        }
        if(Log::isEnabled()) {
            Log::out() << "CREATESTATE " <<(int)offset << " depth " << state->stackDepth << ::std::endl;
            printState(state);
        }
        return state;
    }

    void copySlotInfo(StateInfo::SlotInfo& to, StateInfo::SlotInfo& from);
    void  mergeSlots(StateInfo::SlotInfo* inSlot, StateInfo::SlotInfo* slot, uint32 offset, bool isVar);
    void  setStateInfo(StateInfo *inState, uint32 offset, bool isFallThru);
    void  setStateInfoFromFinally(StateInfo *inState, uint32 offset);

    void restoreStateInfo(StateInfo *stateInfo, uint32 offset) {
        if(Log::isEnabled()) {
            Log::out() << "INIT_STATE_FOR_BLOCK " <<(int)offset << " depth " << stateInfo->stackDepth << ::std::endl;
            printState(stateInfo);
        }
        StateInfo *state = hashtable[offset]; 
        assert(state != NULL && (state->stack || state->stackDepth==0));
        stateInfo->flags      = state->flags;
        stateInfo->stackDepth = state->stackDepth;
        for (int i=0; i < stateInfo->stackDepth; i++)
            stateInfo->stack[i] = state->stack[i];
        stateInfo->exceptionInfo = state->exceptionInfo;
        for (ExceptionInfo *except = stateInfo->exceptionInfo; except != NULL;
             except = except->getNextExceptionInfoAtOffset()) {
            if (except->isCatchBlock() && 
                except->getBeginOffset() <= offset && offset < except->getEndOffset()) {
                CatchBlock* block = (CatchBlock*)except;
                for (CatchHandler *handler = block->getHandlers(); handler != NULL;
                     handler = handler->getNextHandler()) {
                    int cstart = handler->getBeginOffset();
                    Log::out() << "SETCATCHINFO "<<(int)cstart<<" "<<(int)prepass.getNumVars()<< ::std::endl;
                    prepass.pushCatchLabel(cstart);
                    int stackDepth = stateInfo->stackDepth;
                    stateInfo->stackDepth = prepass.getNumVars();
                    setStateInfo(stateInfo,cstart,false);
                    stateInfo->stackDepth = stackDepth;
                }
            }
        }
    }

    int getMaxStackOverflow() { return maxDepth; }
    void printState(StateInfo *state) {
        if (state == NULL) return;
        struct StateInfo::SlotInfo *stack = state->stack;
        for (int i=0; i < state->stackDepth; i++) {
            Log::out() << "STACK " << i << ":";
            StateInfo::print(stack[i],Log::out());
            Log::out() << ::std::endl;
            Log::out() << "        var: ";
            if (stack[i].vars) {
                stack[i].vars->print(Log::out());
            } else {
                Log::out() << "null";
            }
            Log::out() << ::std::endl;
        }
    }
protected:
    virtual bool keyEquals(uint32 *key1,uint32 *key2) const {
        return key1 == key2;
    }
    virtual uint32 getKeyHashCode(uint32 *key) const {
        // return hash of address bits
        return ((uint32)(POINTER_SIZE_INT)key);
    }
private:
    MemoryManager& memManager;
    TypeManager& typeManager;
    JavaLabelPrepass& prepass;
    StlHashMap<uint32, StateInfo*> hashtable;
    int maxDepth;
    int numVars;
};

#endif // _JAVALABELPREPASS_H_


} //namespace Jitrino 
