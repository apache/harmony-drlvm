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

/**
 * @author Intel, Pavel A. Ozhdikhin
 * @version $Revision: 1.17.8.1.4.4 $
 *
 */

#ifndef _IRBUILDER_H_
#define _IRBUILDER_H_

#include <iostream>
#include "MemoryManager.h"
#include "Opcode.h"
#include "Inst.h"
#include "CSEHash.h"
#include "simplifier.h"
#include "InlineInfo.h"

namespace Jitrino {

class JitrinoParameterTable;
class PiCondition;
class VectorHandler;
class MapHandler;
class CompilationContext;

    //
    // flags that control how the IRBuilder expands and optimizes IR instructions
    //
struct IRBuilderFlags {
    IRBuilderFlags () {
        expandMemAddrs = expandNullChecks = false;
            expandCallAddrs = expandVirtualCallAddrs = false;
            expandElemAddrs = expandElemTypeChecks = false;
            doSimplify = doCSE = false;
            insertMethodLabels = insertWriteBarriers = false;
            suppressCheckBounds = false;
            compressedReferences = false;
            genMinMaxAbs = false;
            genFMinMaxAbs = false;
            useNewTypeSystem = false;
            isBCMapinfoRequired = false;
        }
        /* expansion flags */
        bool expandMemAddrs      : 1;    // expand field/array element accesses
        bool expandElemAddrs     : 1;    // expand array elem address computation
        bool expandCallAddrs     : 1;    // expand fun address computation for direct calls
        bool expandVirtualCallAddrs : 1; // expand fun address computation for virtual calls
        bool expandNullChecks    : 1;    // explicit null checks
        bool expandElemTypeChecks: 1;    // explicit array elem type checks for stores
        /* optimization flags */
        bool doCSE               : 1;    // common subexpression elimination
        bool doSimplify          : 1;    // simplification
        /* label & debug insertion */
        bool insertMethodLabels  : 1;    // insert method entry/exit labels
        bool insertWriteBarriers : 1;    // insert write barriers to mem stores
        bool suppressCheckBounds : 1;
        bool compressedReferences: 1;    // are refs in heap compressed?
        bool genMinMaxAbs        : 1;
        bool genFMinMaxAbs       : 1;
        // LBS Project flags
        bool useNewTypeSystem    : 1;    // Use the new LBS type system rather than the old one
        bool isBCMapinfoRequired : 1;     // Produce HIR bc map info
    };

class IRBuilder {
public:
    static void readFlagsFromCommandLine(CompilationContext* cs);
    static void showFlagsFromCommandLine();
    //
    // constructor
    //
    IRBuilder(IRManager& irm);

    IRManager&      getIRManager()      {return irManager;}
    InstFactory&    getInstFactory()    {return instFactory;}
    TypeManager&    getTypeManager()    {return typeManager;}
    OpndManager&    getOpndManager()    {return opndManager;}
    FlowGraph&      getFlowGraph()      {return flowGraph;}

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

    Opnd*  genVMHelperCall(VMHelperCallId helperId,
                            Type* returnType,
                            uint32 numArgs,
                            Opnd*  args[]);

    void       genReturn(Opnd* src, Type* retType);//TR
    void       genReturn();//TR
    Opnd*      genCatch(Type* exceptionType); // TR
    void       genThrow(ThrowModifier mod, Opnd* exceptionObj);//TR
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

    Opnd*      genLdString(MethodDesc* enclosingMethod, uint32 stringToken);//TR
    Opnd*      genLdVar(Type* dstType, VarOpnd* var);//TR
    Opnd*      genLdVarAddr(VarOpnd* var);//TR
    Opnd*      genLdInd(Type*, Opnd* ptr); // for use by front-ends, but not simplifier//TR
    Opnd*      genLdField(Type*, Opnd* base, FieldDesc* fieldDesc); //TR
    Opnd*      genLdStatic(Type*, FieldDesc* fieldDesc);//TR
    Opnd*      genLdElem(Type* elemType, Opnd* array, Opnd* index); //TR
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
    void       genMethodEndMarker(MethodDesc* methodDesc, Opnd *obj);//TR
    void       genMethodEndMarker(MethodDesc* methodDesc);//TR
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
    Opnd* genLdString(Modifier mod, Type *dstType,//TR //SI
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

   	friend class	JavaByteCodeTranslator;
    //
    // private classes
    //
    class _Simplifier : public Simplifier {
    public:
        _Simplifier(IRManager &irm, IRBuilder* irb)
            : Simplifier(irm, false, 0), irBuilder(*irb)
        {
        }
    protected:
        // numeric
        virtual Inst*
        genAdd(Type* type, Modifier mod, Opnd* src1, Opnd* src2){
            return irBuilder.genAdd(type, mod, src1, src2)->getInst();
        }
        virtual Inst*
        genSub(Type* type, Modifier mod, Opnd* src1, Opnd* src2) {
            return irBuilder.genSub(type, mod, src1, src2)->getInst();
        }
        virtual Inst* 
        genNeg(Type* type, Opnd* src) {
            return irBuilder.genNeg(type, src)->getInst();
        }
        virtual Inst*
        genMul(Type* type, Modifier mod, Opnd* src1, Opnd* src2){
            return irBuilder.genMul(type, mod, src1, src2)->getInst();
        }
        virtual Inst*
        genMulHi(Type* type, Modifier mod, Opnd* src1, Opnd* src2){
            return irBuilder.genMulHi(type, mod, src1, src2)->getInst();
        }
        virtual Inst*
        genMin(Type* type, Opnd* src1, Opnd* src2){
            return irBuilder.genMin(type, src1, src2)->getInst();
        }
        virtual Inst*
        genMax(Type* type, Opnd* src1, Opnd* src2){
            return irBuilder.genMax(type, src1, src2)->getInst();
        }
        virtual Inst*
        genAbs(Type* type, Opnd* src1){
            return irBuilder.genAbs(type, src1)->getInst();
        }
        // bitwise
        virtual Inst*
        genAnd(Type* type, Opnd* src1, Opnd* src2){
            return irBuilder.genAnd(type, src1, src2)->getInst();
        }
        virtual Inst*
        genOr(Type* type, Opnd* src1, Opnd* src2){
            return irBuilder.genOr(type, src1, src2)->getInst();
        }
        virtual Inst*
        genXor(Type* type, Opnd* src1, Opnd* src2){
            return irBuilder.genXor(type, src1, src2)->getInst();
        }
        virtual Inst*
        genNot(Type* type, Opnd* src1){
            return irBuilder.genNot(type, src1)->getInst();
        }
        virtual Inst*
        genSelect(Type* type,
                  Opnd* src1, Opnd* src2, Opnd* src3){
            return irBuilder.genSelect(type, src1, src2, src3)->getInst();
        }
        // conversion
        virtual Inst*
        genConv(Type* dstType, Type::Tag toType, Modifier ovfMod, Opnd* src){
            return irBuilder.genConv(dstType, toType, ovfMod, src)->getInst();
        }
        // shifts
        virtual Inst*
        genShladd(Type* type,
                  Opnd* src1, Opnd* src2, Opnd *src3){
            return irBuilder.genShladd(type, src1, src2, src3)->getInst();
        }
        virtual Inst*
        genShl(Type* type, Modifier smmod,
               Opnd* src1, Opnd* src2){
            return irBuilder.genShl(type, smmod, src1, src2)->getInst();
        }
        virtual Inst*
        genShr(Type* type, Modifier mods,
               Opnd* src1, Opnd* src2){
            return irBuilder.genShr(type, mods, src1, src2)->getInst();
        }
        // comparison
        virtual Inst*
        genCmp(Type* type, Type::Tag insttype,
               ComparisonModifier mod, Opnd* src1, Opnd* src2){
            return irBuilder.genCmp(type, insttype, mod, src1, src2)->getInst();
        }
        // control flow
        virtual void
        genJump(LabelInst* label) {
            irBuilder.genJump(label);
        }
        virtual void
        genBranch(Type::Tag instType, ComparisonModifier mod, 
                  LabelInst* label, Opnd* src1, Opnd* src2) {
            irBuilder.genBranch(instType, mod, label, src1, src2);
        }
        virtual void
        genBranch(Type::Tag instType, ComparisonModifier mod, 
                  LabelInst* label, Opnd* src1) {
            irBuilder.genBranch(instType, mod, label, src1);
        }
        virtual Inst* 
        genDirectCall(MethodDesc* methodDesc,
                      Type* returnType,
                      Opnd* tauNullCheckedFirstArg,
                      Opnd* tauTypesChecked,
                      uint32 numArgs,
                      Opnd* args[],
                      InlineInfoBuilder* inlineBuilder)
        {
            irBuilder.genDirectCall(methodDesc, returnType,
                                    tauNullCheckedFirstArg, tauTypesChecked, 
                                    numArgs, args, inlineBuilder);
            return irBuilder.getCurrentLabel()->prev();
        }
        // load, store & mov
        virtual Inst*
        genLdConstant(int32 val) {
            return irBuilder.genLdConstant(val)->getInst();
        }
        virtual Inst*
        genLdConstant(int64 val) {
            return irBuilder.genLdConstant(val)->getInst();
        }
        virtual Inst*
        genLdConstant(float val) {
            return irBuilder.genLdConstant(val)->getInst();
        }
        virtual Inst*
        genLdConstant(double val) {
            return irBuilder.genLdConstant(val)->getInst();
        }
        virtual Inst*
        genLdConstant(Type *type, ConstInst::ConstValue val) {
            return irBuilder.genLdConstant(type, val)->getInst();
        }
        virtual Inst*
        genTauLdInd(Modifier mod, Type* dstType, Type::Tag ldType, Opnd* src,
                    Opnd *tauNonNullBase, Opnd *tauAddressInRange) {
            return irBuilder.genTauLdInd(mod, dstType, ldType, src,
                                         tauNonNullBase, tauAddressInRange)->getInst();
        }
        virtual Inst*
        genLdString(Modifier mod, Type* dstType,
                    uint32 token, MethodDesc *enclosingMethod) {
            return irBuilder.genLdString(mod, dstType,
                                         token, enclosingMethod)->getInst();
        }
        virtual Inst* 
        genLdFunAddrSlot(MethodDesc* methodDesc) {
            return irBuilder.genLdFunAddrSlot(methodDesc)->getInst();
        }
        virtual Inst* 
        genGetVTableAddr(ObjectType* type) {
            return irBuilder.genGetVTable(type)->getInst();
        }
        // compressed references
        virtual Inst* genCompressRef(Opnd *uncompref){
            return irBuilder.genCompressRef(uncompref)->getInst();
        }
        virtual Inst* genUncompressRef(Opnd *compref){
            return irBuilder.genUncompressRef(compref)->getInst();
        }
        virtual Inst *genLdFieldOffsetPlusHeapbase(FieldDesc* fd) {
            return irBuilder.genLdFieldOffsetPlusHeapbase(fd)->getInst();
        }
        virtual Inst *genLdArrayBaseOffsetPlusHeapbase(Type *elemType) {
            return irBuilder.genLdArrayBaseOffsetPlusHeapbase(elemType)->getInst();
        }
        virtual Inst *genLdArrayLenOffsetPlusHeapbase(Type *elemType) {
            return irBuilder.genLdArrayLenOffsetPlusHeapbase(elemType)->getInst();
        }
        virtual Inst *genAddOffsetPlusHeapbase(Type *ptrType, Opnd *compRef, 
                                               Opnd *offsetPlusHeapbase) {
            return irBuilder.genAddOffsetPlusHeapbase(ptrType, compRef, 
                                                      offsetPlusHeapbase)->getInst();
        }
        virtual Inst *genTauSafe() {
            return irBuilder.genTauSafe()->getInst();
        }
        virtual Inst *genTauMethodSafe() {
            return irBuilder.genTauMethodSafe()->getInst();
        }
        virtual Inst *genTauUnsafe() {
            return irBuilder.genTauUnsafe()->getInst();
        }
        virtual Inst* genTauStaticCast(Opnd *src, Opnd *tauCheckedCast, Type *castType) {
            return irBuilder.genTauStaticCast(src, tauCheckedCast, castType)->getInst();
        }
        virtual Inst* genTauHasType(Opnd *src, Type *castType) {
            return irBuilder.genTauHasType(src, castType)->getInst();
        }
        virtual Inst* genTauHasExactType(Opnd *src, Type *castType) {
            return irBuilder.genTauHasExactType(src, castType)->getInst();
        }
        virtual Inst* genTauIsNonNull(Opnd *src) {
            return irBuilder.genTauIsNonNull(src)->getInst();
        }
        // helper for store simplification, builds/finds simpler src, possibly
        // modifies typetag or store modifier. 
        virtual Opnd*
        simplifyStoreSrc(Opnd *src, Type::Tag &typetag, Modifier &mod,
                         bool compressRef) {
            return 0;
        }
        
        virtual void  
        foldBranch(BranchInst* br, bool isTaken) {
            assert(0);
        }
        virtual void  
        foldSwitch(SwitchInst* sw, uint32 index) {
            assert(0);
        }
        virtual void  
        eliminateCheck(Inst* checkInst, bool alwaysThrows) {
            assert(0);
        }    
        virtual void genThrowSystemException(CompilationInterface::SystemExceptionId id) {
            irBuilder.genThrowSystemException(id);
        }
    private:
        IRBuilder&  irBuilder;
    };
    //
    // private fields
    //
    // references to other translation objects
    //
    IRManager&          irManager;
    OpndManager&        opndManager;        // generates operands
    TypeManager&        typeManager;        // generates types
    InstFactory&        instFactory;        // generates instructions
    FlowGraph&          flowGraph;          // generates blocks
    IRBuilderFlags&     irBuilderFlags;     // flags that control translation 
    //
    // translation state
    //
    MemoryManager       tempMemoryManager;  // for use ONLY by IRBuilder
    LabelInst*          currentLabel;       // current header label
    CSEHashTable        cseHashTable;       // hash table for CSE
    //
    // simplifier to fold constants etc.
    //
    _Simplifier simplifier;
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
