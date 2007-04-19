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
 * @author Intel, Pavel A. Ozhdikhin
 * @version $Revision: 1.17.8.1.4.4 $
 *
 */

#ifndef _IRBUILDER_H_
#define _IRBUILDER_H_

#include "MemoryManager.h"
#include "Opcode.h"
#include "Inst.h"
#include "CSEHash.h"
#include "simplifier.h"
#include "InlineInfo.h"
#include "IRBuilderFlags.h"
#include "PMFAction.h"


#include <iostream>

namespace Jitrino {

class PiCondition;
class VectorHandler;
class MapHandler;
class CompilationContext;
class SessionAction;
struct TranslatorFlags;


#define IRBUILDER_ACTION_NAME "irbuilder"

class IRBuilderAction : public Action {
public:
    void init();
    const IRBuilderFlags& getFlags() const {return irBuilderFlags;}
private:
    void readFlags();
    
    IRBuilderFlags    irBuilderFlags;
};


class IRBuilder : public SessionAction {
public:
    IRBuilder();
    void init(IRManager* irm, TranslatorFlags* traFlags, MemoryManager& tmpMM);
    
    //this session can't be placed into PMF path
    void run(){assert(0);}

    IRManager*          getIRManager()   const  { return irManager;}
    InstFactory*        getInstFactory() const  {return instFactory;}
    TypeManager*        getTypeManager() const  {return typeManager;}
    OpndManager*        getOpndManager() const  {return opndManager;}
    ControlFlowGraph*   getFlowGraph() const  {return flowGraph;}
    TranslatorFlags*    getTranslatorFlags() const {return translatorFlags;}

    // 
    // used to map bytecode offsets to instructions by JavaByteCodeTranslator
    Inst* getLastGeneratedInst();

    // TRANSLATOR GEN
    // gen methods used in translators (front ends)
    // if we had better compilers, we might separate these out into abstract methods 
    // in an interface superclass
    // note that some of these are also used by the _Simplifier methods below
    
    // arithmetic instructions
    Opnd* genAdd(Type* dstType, Modifier mod, Opnd* src1, Opnd* src2); // TR //SI
    Opnd* genMul(Type* dstType, Modifier mod, Opnd* src1, Opnd* src2);//TR //SI
    Opnd* genSub(Type* dstType, Modifier mod, Opnd* src1, Opnd* src2);//TR //SI
    Opnd* genCliDiv(Type* dstType, Modifier mod, Opnd* src1, Opnd* src2); // TR
    Opnd* genDiv(Type* dstType, Modifier mod, Opnd* src1, Opnd* src2); //TR
    Opnd* genCliRem(Type* dstType, Modifier mod, Opnd* src1, Opnd* src2); // TR
    Opnd* genRem(Type* dstType, Modifier mod, Opnd* src1, Opnd* src2);//TR
    Opnd* genNeg(Type* dstType, Opnd* src);//TR //SI
    Opnd* genMulHi(Type* dstType, Modifier mod, Opnd* src1, Opnd* src2); //SI
    Opnd* genMin(Type* dstType, Opnd* src1, Opnd* src2); //SI
    Opnd* genMax(Type* dstType, Opnd* src1, Opnd* src2); //SI
    Opnd* genAbs(Type* dstType, Opnd* src1); //SI
    // bitwise instructions
    Opnd* genAnd(Type* dstType, Opnd* src1, Opnd* src2); // TR //SI
    Opnd* genOr(Type* dstType, Opnd* src1, Opnd* src2);//TR //SI
    Opnd* genXor(Type* dstType, Opnd* src1, Opnd* src2);//TR //SI
    Opnd* genNot(Type* dstType, Opnd* src);//TR //SI
    // Conversion
    Opnd* genConv(Type* dstType, Type::Tag toType, Modifier ovfMod, Opnd* src); //TR //SI
    Opnd* genConvZE(Type* dstType, Type::Tag toType, Modifier ovfMod, Opnd* src); //TR //SI
    Opnd* genConvUnmanaged(Type* dstType, Type::Tag toType, Modifier ovfMod, Opnd* src); //TR //SI
    // Shift
    Opnd* genShl(Type* dstType, Modifier mod, Opnd* value, Opnd* shiftAmount);//TR //SI
    Opnd* genShr(Type* dstType, Modifier mods, Opnd* value, Opnd* shiftAmount);//TR //SI
    // Comparison
    Opnd* genCmp(Type* dstType, Type::Tag srcType, ComparisonModifier mod, Opnd* src1, Opnd* src2); // TR //SI
    // 3-way comparison, used to represent Java tests:
    //    (s1 cmp s2) ? 1 : (s2 cmp s1) : -1 : 0
    Opnd*      genCmp3(Type* dstType, Type::Tag srcType, ComparisonModifier mod, Opnd* src1, Opnd* src2); // TR
    void  genBranch(Type::Tag instType, ComparisonModifier mod, LabelInst* label, Opnd* src1, Opnd* src2); // TR //SI
    void  genBranch(Type::Tag instType, ComparisonModifier mod, LabelInst* label, Opnd* src); // TR //SI
    void  genJump(LabelInst* label); //TR
    void  genSwitch(uint32 numLabels, LabelInst* label[], LabelInst* defaultLabel, Opnd* src);//TR
    // Calls
    // If the call does not return a value, then the returned Opnd* is the
    // null Opnd*.
    Opnd* genDirectCall(MethodDesc* methodDesc, // TR //SI
                        Type* returnType,
                        Opnd* tauNullCheckedFirstArg, // 0 for unsafe
                        Opnd* tauTypesChecked,        // 0 to let IRBuilder find taus
                        uint32 numArgs,
                        Opnd* args[],
                        InlineInfoBuilder* inlineInfoBuilder);     // NULL if this call is not inlined
   
   Opnd* genTauVirtualCall(MethodDesc* methodDesc,//TR
                                  Type* returnType,
                                  Opnd* tauNullCheckedFirstArg, // 0 to let IRBuilder add check
                                  Opnd* tauTypesChecked,        // 0 to let IRBuilder find it
                                  uint32 numArgs,
                                  Opnd* args[],
                                  InlineInfoBuilder* inlineInfoBuilder);     // NULL if this call is not inlined

    Opnd* genIndirectCall(      Type* returnType, //TR
                                  Opnd* funAddr,
                                  Opnd* tauNullCheckedFirstArg, // 0 for unsafe
                                  Opnd* tauTypesChecked,        // 0 to let IRBuilder find it
                                 uint32 numArgs,
                                  Opnd* args[],
                     InlineInfoBuilder* inlineInfoBuilder);     // NULL if this call is not inlined

    Opnd*  genIntrinsicCall(
                        IntrinsicCallId intrinsicId, //TR
                                  Type* returnType,
                                  Opnd* tauNullCheckedRefArgs, // 0 for unsafe
                                  Opnd* tauTypesChecked,       // 0 to let IRBuilder find it
                                 uint32 numArgs,
                                  Opnd*  args[]);

    Opnd*  genJitHelperCall(JitHelperCallId helperId,
                                  Type* returnType,
                                  uint32 numArgs,
                                  Opnd*  args[]);

    Opnd*  genVMHelperCall(CompilationInterface::RuntimeHelperId helperId,
                            Type* returnType,
                            uint32 numArgs,
                            Opnd*  args[]);

    
    void       genReturn(Opnd* src, Type* retType);//TR
    void       genReturn();//TR
    Opnd*      genCatch(Type* exceptionType); // TR
    void       genThrow(ThrowModifier mod, Opnd* exceptionObj);//TR
    void       genPseudoThrow();//TR
    void       genThrowSystemException(CompilationInterface::SystemExceptionId);//SI
    void       genThrowLinkingException(Class_Handle encClass, uint32 CPIndex, uint32 operation);//SI
    void       genLeave(LabelInst* label);//TR
    void       genEndFinally(); // TR
    void       genEndFilter(); // TR
    void       genEndCatch(); // TR
    void       genJSR(LabelInst* label); //TR
    void       genRet(Opnd *src);//TR
    Opnd*      genSaveRet();//TR
    // load, store & move
    Opnd* genLdConstant(int32 val); //TR //SI
    Opnd* genLdConstant(int64 val); //TR //SI
    Opnd* genLdConstant(float val); //TR //SI
    Opnd* genLdConstant(double val); //TR //SI
    Opnd* genLdConstant(Type *ptrtype, ConstInst::ConstValue val);//TR //SI
    Opnd*      genLdFloatConstant(float val);//TR
    Opnd*      genLdFloatConstant(double val);//TR
    Opnd*      genLdNull();//TR

    Opnd*      genLdRef(MethodDesc* enclosingMethod, uint32 stringToken, Type* type);//TR
    Opnd*      genLdVar(Type* dstType, VarOpnd* var);//TR
    Opnd*      genLdVarAddr(VarOpnd* var);//TR
    Opnd*      genLdInd(Type*, Opnd* ptr); // for use by front-ends, but not simplifier//TR
    Opnd*      genLdField(Type*, Opnd* base, FieldDesc* fieldDesc); //TR
    Opnd*      genLdStatic(Type*, FieldDesc* fieldDesc);//TR
    Opnd*      genLdElem(Type* elemType, Opnd* array, Opnd* index); //TR
    Opnd*      genLdElem(Type* elemType, Opnd* array, Opnd* index,
                         Opnd* tauNullCheck, Opnd* tauAddressInRange);
    Opnd*      genLdFieldAddr(Type* fieldType, Opnd* base, FieldDesc* fieldDesc); //TR
    Opnd*      genLdStaticAddr(Type* fieldType, FieldDesc* fieldDesc);//TR
    Opnd*      genLdElemAddr(Type* elemType, Opnd* array, Opnd* index);//TR
    Opnd*      genLdVirtFunAddr(Opnd* base, MethodDesc* methodDesc);//TR
    Opnd*      genLdFunAddr(MethodDesc* methodDesc);//TR
    Opnd*      genArrayLen(Type* dstType, Type::Tag type, Opnd* array); // TR
    // store instructions
    void       genStVar(VarOpnd* var, Opnd* src);//TR
    void       genStField(Type*, Opnd* base, FieldDesc* fieldDesc, Opnd* src);//TR
    void       genStStatic(Type*, FieldDesc* fieldDesc, Opnd* src);//TR
    void       genStElem(Type*, Opnd* array, Opnd* index, Opnd* src);//TR
    void       genStElem(Type*, Opnd* array, Opnd* index, Opnd* src,
                         Opnd* tauNullCheck, Opnd* tauBaseTypeCheck, Opnd* tauAddressInRange);
    void       genStInd(Type*, Opnd* ptr, Opnd* src);//TR
    // checks
    Opnd*      genTauCheckNull(Opnd* base);
    Opnd*      genTauCheckBounds(Opnd* array, Opnd* index, Opnd *tauNullChecked);
    Opnd*      genCheckFinite(Type* dstType, Opnd* src); // TR
    // allocation
    Opnd*      genNewObj(Type* type);//TR
    Opnd*      genNewArray(NamedType* elemType, Opnd* numElems);//TR
    Opnd*      genMultianewarray(NamedType* arrayType, uint32 dimensions, Opnd** numElems);//TR
    // sync
    void       genMonitorEnter(Opnd* src); // also inserts nullcheck of src//TR
    void       genMonitorExit(Opnd* src);  // also inserts nullcheck of src//TR
    // typemonitors
    void       genTypeMonitorEnter(Type *type);//TR
    void       genTypeMonitorExit(Type *type);//TR
    // lowered parts of monitor enter/exit;
    //   these assume src is already checked and is not null
    Opnd*      genLdLockAddr(Type *dstType, Opnd *obj);    // result is ref:int16//TR
    Opnd*      genBalancedMonitorEnter(Type *dstType, Opnd* src, Opnd *lockAddr); // result is int32 // TR
    void       genBalancedMonitorExit(Opnd* src, Opnd *lockAddr, Opnd *oldValue); // TR
    // type checking
    // CastException (succeeds if argument is null, returns casted object)
    Opnd*      genCast(Opnd* src, Type* type); // TR
    // returns trueResult if src is an instance of type, 0 otherwise
    Opnd*      genAsType(Opnd* src, Type* type); // TR
    // returns 1 if src is not null and an instance of type, 0 otherwise
    Opnd*      genInstanceOf(Opnd* src, Type* type); //TR
    void       genInitType(NamedType* type); //TR
    // labels
    void       genLabel(LabelInst* labelInst); //TR
    void       genFallThroughLabel(LabelInst* labelInst); //TR
    // method entry/exit
    LabelInst* genMethodEntryLabel(MethodDesc* methodDesc);//TR
    void       genMethodEntryMarker(MethodDesc* methodDesc);//TR
    void       genMethodEndMarker(MethodDesc* methodDesc, Opnd *obj, Opnd *retOpnd);//TR
    void       genMethodEndMarker(MethodDesc* methodDesc, Opnd *retOpnd);//TR
    // value object instructions
    Opnd*      genLdObj(Type* type, Opnd* addrOfValObj);//TR
    void       genStObj(Opnd* addrOfDstVal, Opnd* srcVal, Type* type);//TR
    void       genCopyObj(Type* type, Opnd* dstValPtr, Opnd* srcValPtr); // TR
    void       genInitObj(Type* type, Opnd* valPtr); //TR
    Opnd*      genSizeOf(Type* type);//TR
    Opnd*      genMkRefAny(Type* type, Opnd* ptr);//TR
    Opnd*      genRefAnyType(Opnd* typedRef);//TR
    Opnd*      genRefAnyVal(Type* type, Opnd* typedRef);//TR
    Opnd*      genUnbox(Type* type, Opnd* obj);//TR
    Opnd*      genBox(Type* type, Opnd* val); // TR
    Opnd*      genLdToken(MethodDesc* enclosingMethod, uint32 metadataToken);//TR
    // block instructions
    void       genCopyBlock(Opnd* dstAddr, Opnd* srcAddr, Opnd* size); // TR
    void       genInitBlock(Opnd* dstAddr, Opnd* val, Opnd* size); //TR
    Opnd*      genLocAlloc(Opnd* size);//TR
    Opnd*      genArgList(); // TR

    void       genTauTypeCompare(Opnd *arg0, MethodDesc* methodDesc, LabelInst *target,
                                 Opnd *tauNullChecked);//TR

    Opnd*      genArgCoercion(Type* argType, Opnd* actualArg); // TR
    // actual parameter and variable definitions
    Opnd*      genArgDef(Modifier, Type*); // TR
    VarOpnd*   genVarDef(Type*, bool isPinned);//TR
    // Phi-node instruction
    Opnd*      genPhi(uint32 numArgs, Opnd* args[]);//TR

    // label manipulation for translators
    LabelInst* createLabel();
    // this should really be genBlocks
    void       createLabels(uint32 numLabels, LabelInst** labels);
    void       killCSE();
    LabelInst* getCurrentLabel() {return currentLabel;}


    // SIMPLIFIER GEN
    // gen methods for use in _Simplifier below, also may be used elsewhere
    // selection
    Opnd* genSelect(Type* dstType, Opnd* src1, Opnd* src2, Opnd *src3); //SI
    // Shift
    Opnd* genShladd(Type* dstType, Opnd* value, Opnd* shiftAmount, Opnd* addto); //SI
    // Control flow
    // load, store & move
    Opnd* genLdRef(Modifier mod, Type *dstType,//TR //SI
                      uint32 token, MethodDesc *enclosingMethod); // for simplifier use
    Opnd* genTauLdInd(Modifier mod, Type *dstType, Type::Tag ldType, Opnd *ptr, //SI
                      Opnd *tauNonNullBase, Opnd *tauAddressInRange); // for simplifier use
    Opnd* genLdFunAddrSlot(MethodDesc* methodDesc); //SI
    Opnd* genGetVTable(ObjectType* type); //SI
    // compressed reference instructions
    Opnd* genUncompressRef(Opnd *compref); //SI
    Opnd* genCompressRef(Opnd *uncompref); //SI
    Opnd* genLdFieldOffsetPlusHeapbase(FieldDesc* fieldDesc); //SI
    Opnd* genLdArrayBaseOffsetPlusHeapbase(Type *elemType); //SI
    Opnd* genLdArrayLenOffsetPlusHeapbase(Type *elemType); //SI
    Opnd* genAddOffsetPlusHeapbase(Type *ptrType, Opnd* compref, Opnd* offset); //SI

    // RECURSIVE GEN
    Opnd*      genIndirectMemoryCall(Type* returnType,
                                     Opnd* funAddr,
                                     Opnd* tauNullCheckedFirstArg, // 0 to let IRBuilder add check
                                     Opnd* tauTypesChecked,        // 0 to let IRBuilder find it
                                     uint32 numArgs,
                                     Opnd* args[],
                                     InlineInfoBuilder* inlineInfoBuilder);     // NULL if this call is not inlined

    Opnd*      genLdFieldAddrNoChecks(Type* fieldType, Opnd* base, FieldDesc* fieldDesc);
    Opnd*      genLdElemAddrNoChecks(Type *elemType, Opnd* array, Opnd* index);
    Opnd*      genLdStaticAddrNoChecks(Type *fieldType, FieldDesc* fieldDesc);
    Opnd*      genTauLdVirtFunAddrSlot(Opnd* base, Opnd* tauOk, MethodDesc* methodDesc);
    Opnd*      genLdVTable(Opnd* base, Type* type);
    Opnd*      genTauLdVTable(Opnd* base, Opnd *tauNullChecked, Type* type);
    Opnd*      genLdArrayBaseAddr(Type* elemType, Opnd* array);
    Opnd*      genAddScaledIndex(Opnd* ptr, Opnd* index);
    void       genTauStRef(Type*, Opnd *objectbase, Opnd* ptr, Opnd* value,
                           Opnd *tauBaseNonNull, Opnd *tauAddressInRange,
                           Opnd *tauELemTypeChecked);
    void       genTauStInd(Type*, Opnd* ptr, Opnd* src,
                           Opnd *tauBaseNonNull, Opnd *tauAddressInRange,
                           Opnd *tauElemTypeChecked);
    // checks
    Opnd*      genTauCheckZero(Opnd* base);
    Opnd*      genTauCheckDivOpnds(Opnd* num, Opnd* denom);
    Opnd*      genTauCheckElemType(Opnd* array, Opnd* src, Opnd *tauNullChecked,
                                   Opnd* tauIsArray);
    Opnd*      genTauCheckBounds(Opnd* ub, Opnd* index);
    Opnd*      genTauArrayLen(Type* dstType, Type::Tag type, Opnd* array,
                              Opnd *tauNullChecked, Opnd *tauBaseTypeChecked);
    Opnd*      genTauBalancedMonitorEnter(Type *dstType, Opnd* src, Opnd *lockAddr,
                                          Opnd *tauNullChecked); // result is int32
    Opnd*      genTauCheckFinite(Opnd* src);
    // tau operations
    Opnd*      genTauSafe();
    Opnd*      genTauMethodSafe();
    Opnd*      genTauUnsafe();
    Opnd*      genTauCheckCast(Opnd* src, Opnd *tauNullChecked, Type* type);
    Opnd*      genTauStaticCast(Opnd *src, Opnd *tauCheckedCast, Type *castType);
    Opnd*      genTauHasType(Opnd *src, Type *hasType);
    Opnd*      genTauHasExactType(Opnd *src, Type *hasType);
    Opnd*      genTauIsNonNull(Opnd *src);
    Opnd*      genTauAnd(Opnd *src1, Opnd *src2);

    // UNUSED GEN
    Opnd*      genPrefetch(Opnd* base, Opnd *offset, Opnd *hints);
    Opnd*      genCopy(Opnd* src);
    Opnd*      genTauPi(Opnd* src, Opnd *tau, PiCondition *cond);
    Opnd*      genScaledDiffRef(Opnd* src1, Opnd* src2);
    // compressed reference instructions
    Opnd*      genLdFieldOffset(FieldDesc* fieldDesc);
    Opnd*      genLdArrayBaseOffset(Type *elemType);
    Opnd*      genLdArrayLenOffset(Type *elemType);
    Opnd*      genAddOffset(Type *ptrType, Opnd* ref, Opnd* offset);
    // lowered parts of monitor enter/exit;
    //   these assume src is already checked and is not null
    void       genIncRecCount(Opnd *obj, Opnd *oldLock);   // result is ref:int16
    Opnd*      genTauOptimisticBalancedMonitorEnter(Type *dstType, Opnd* src, Opnd *lockAddr,
                                                    Opnd *tauNullChecked); // result is int32
    void       genOptimisticBalancedMonitorExit(Opnd* src, Opnd *lockAddr, Opnd *oldValue);
    void       genMonitorEnterFence(Opnd *src);
    void       genMonitorExitFence(Opnd *src);
    // checks
    void       genSourceLineNumber(uint32 fileId, uint32 lineNumber);

protected:
    void appendInstUpdateInlineInfo(Inst* inst, InlineInfoBuilder* builder, MethodDesc* target_md);

private:

    void readFlagsFromCommandLine(SessionAction* argSource, const char* argPrefix);

    // RECURSIVE GEN
    // additional gen methods used only recursively
    Opnd*      genPredCmp(Type *dstType, Type::Tag instType, ComparisonModifier mod,
                          Opnd *src1, Opnd *src2);
    void       genPredBranch(LabelInst* label, Opnd *src);

private:
    //
    // private helper methods
    //
    Opnd*    propagateCopy(Opnd*);
    Inst*    appendInst(Inst*);
    Type*    getOpndTypeFromLdType(Type* ldType);
    Opnd*    createOpnd(Type*);
    PiOpnd*  createPiOpnd(Opnd *org);
    Opnd*    lookupHash(uint32 opc);
    Opnd*    lookupHash(uint32 opc, uint32 op);
    Opnd*    lookupHash(uint32 opc, uint32 op1, uint32 op2);
    Opnd*    lookupHash(uint32 opc, uint32 op1, uint32 op2, uint32 op3);
    Opnd*    lookupHash(uint32 opc, Opnd* op) { return lookupHash(opc, op->getId()); };
    Opnd*    lookupHash(uint32 opc, Opnd* op1, Opnd* op2) { return lookupHash(opc, op1->getId(), op2->getId()); };
    Opnd*    lookupHash(uint32 opc, Opnd* op1, Opnd* op2, Opnd* op3) { return lookupHash(opc, op1->getId(), op2->getId(), op3->getId()); };
    void     insertHash(uint32 opc, Inst*);
    void     insertHash(uint32 opc, uint32 op, Inst*);
    void     insertHash(uint32 opc, uint32 op1, uint32 op2, Inst*);
    void     insertHash(uint32 opc, uint32 op1, uint32 op2, uint32 op3, Inst*);
    void     insertHash(uint32 opc, Opnd* op, Inst*i) { insertHash(opc, op->getId(), i); };
    void     insertHash(uint32 opc, Opnd* op1, Opnd* op2, Inst*i) { insertHash(opc, op1->getId(), op2->getId(), i); };
    void     insertHash(uint32 opc, Opnd* op1, Opnd* op2, Opnd* op3, Inst*i) { insertHash(opc, op1->getId(), op2->getId(), op3->getId(), i); };
    void     invalid();    // called when the builder detects invalid IR
    void setBcOffset(uint32 bcOffset) {  offset =  bcOffset; };
    uint32 getBcOffset() {  return offset; };

    friend class    JavaByteCodeTranslator;
    
    //
    // private fields
    //
    // references to other translation objects
    //
    IRManager*          irManager;
    OpndManager*        opndManager;        // generates operands
    TypeManager*        typeManager;        // generates types
    InstFactory*        instFactory;        // generates instructions
    ControlFlowGraph*   flowGraph;          // generates blocks
    IRBuilderFlags      irBuilderFlags;     // flags that control translation 
    TranslatorFlags*    translatorFlags;
    //
    // translation state
    //
    LabelInst*          currentLabel;       // current header label
    CSEHashTable*        cseHashTable;       // hash table for CSE
    
    //
    // simplifier to fold constants etc.
    //
    Simplifier* simplifier;
    //
    // method-safe operand
    Opnd*               tauMethodSafeOpnd;

    // current bc offset
    uint32 offset;
    VectorHandler* bc2HIRmapHandler;
    MapHandler* lostBCMapOffsetHandler;
};

} //namespace Jitrino 

#endif // _IRBUILDER_H_
