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
 * @version $Revision: 1.26.8.1.4.4 $
 *
 */

#include "Log.h"
#include "PropertyTable.h"
#include "methodtable.h"
#include "inliner.h"
#include "irmanager.h"
#include "FlowGraph.h"
#include "Inst.h"
#include "Dominator.h"
#include "Loop.h"
#include "Profiler.h"
#include "simplifier.h"
#include "JavaByteCodeParser.h"
#include "PropertyTable.h"

namespace Jitrino {

#define MAX_INLINE_GROWTH_FACTOR_PROF 500
#define MIN_INLINE_STOP_PROF 100
#define MIN_BENEFIT_THRESHOLD_PROF 100
#define INLINE_LARGE_THRESHOLD_PROF 150

#define MAX_INLINE_GROWTH_FACTOR 170
#define MIN_INLINE_STOP 50
#define MIN_BENEFIT_THRESHOLD 200
#define INLINE_LARGE_THRESHOLD 70


#define CALL_COST 1

#define INLINE_SMALL_THRESHOLD 12
#define INLINE_SMALL_BONUS 300
#define INLINE_MEDIUM_THRESHOLD 50
#define INLINE_MEDIUM_BONUS 200
#define INLINE_LARGE_PENALTY 500
#define INLINE_LOOP_BONUS 200
#define INLINE_LEAF_BONUS 0
#define INLINE_SYNCH_BONUS 50
#define INLINE_RECURSION_PENALTY 300
#define INLINE_EXACT_ARG_BONUS 0
#define INLINE_EXACT_ALL_BONUS 0
#define INLINE_SKIP_EXCEPTION_PATH true

Inliner::Inliner(MemoryManager& mm, IRManager& irm, bool doProfileOnly) 
    : _mm(mm), _toplevelIRM(irm), 
      _typeManager(irm.getTypeManager()), _instFactory(irm.getInstFactory()),
      _opndManager(irm.getOpndManager()),
      _hasProfileInfo(irm.getFlowGraph().hasEdgeProfile()),
      _useInliningTranslatorCall(!irm.getFlowGraph().hasEdgeProfile()),
      _inlineCandidates(mm), _initByteSize(irm.getMethodDesc().getByteCodeSize()), 
      
      _currentByteSize(irm.getMethodDesc().getByteCodeSize()), 
      
      _inlineTree(new (mm) InlineNode(irm, 0, 0)) {
    JitrinoParameterTable& propertyTable = irm.getParameterTable();
    
    _doProfileOnlyInlining = doProfileOnly;
    _useInliningTranslator = !doProfileOnly;
    
    _maxInlineGrowthFactor = ((double) propertyTable.lookupInt("opt::inline::growth_factor", doProfileOnly ? MAX_INLINE_GROWTH_FACTOR_PROF : MAX_INLINE_GROWTH_FACTOR)) / 100;
    _minInlineStop = propertyTable.lookupUint("opt::inline::min_stop", doProfileOnly ? MIN_INLINE_STOP_PROF : MIN_INLINE_STOP);
    _minBenefitThreshold = propertyTable.lookupInt("opt::inline::min_benefit_threshold", 
                                                   doProfileOnly ? MIN_BENEFIT_THRESHOLD_PROF : MIN_BENEFIT_THRESHOLD);
    
    _inlineSmallMaxByteSize = propertyTable.lookupUint("opt::inline::small_method_max_size", INLINE_SMALL_THRESHOLD);
    _inlineSmallBonus = propertyTable.lookupInt("opt::inline::small_method_bonus", INLINE_SMALL_BONUS);
    
    _inlineMediumMaxByteSize = propertyTable.lookupUint("opt::inline::medium_method_max_size", INLINE_MEDIUM_THRESHOLD);
    _inlineMediumBonus = propertyTable.lookupInt("opt::inline::medium_method_bonus", INLINE_MEDIUM_BONUS);
    
    _inlineLargeMinByteSize = propertyTable.lookupUint("opt::inline::large_method_min_size", doProfileOnly ? INLINE_LARGE_THRESHOLD_PROF : INLINE_LARGE_THRESHOLD);
    _inlineLargePenalty = propertyTable.lookupInt("opt::inline::large_method_penalty", INLINE_LARGE_PENALTY);

    _inlineLoopBonus = propertyTable.lookupInt("opt::inline::loop_bonus", INLINE_LOOP_BONUS);
    _inlineLeafBonus = propertyTable.lookupInt("opt::inline::leaf_bonus", INLINE_LEAF_BONUS);
    _inlineSynchBonus = propertyTable.lookupInt("opt::inline::synch_bonus", INLINE_SYNCH_BONUS);
    _inlineRecursionPenalty = propertyTable.lookupInt("opt::inline::recursion_penalty", INLINE_RECURSION_PENALTY);
    _inlineExactArgBonus = propertyTable.lookupInt("opt::inline::exact_single_parameter_bonus", INLINE_EXACT_ARG_BONUS);
    _inlineExactAllBonus = propertyTable.lookupInt("opt::inline::exact_all_parameter_bonus", INLINE_EXACT_ALL_BONUS);

    _inlineSkipExceptionPath = propertyTable.lookupBool("opt::inline::skip_exception_path", INLINE_SKIP_EXCEPTION_PATH);
    const char* skipMethods = propertyTable.lookup("opt::inline::skip_methods");
    if(skipMethods == NULL) {
        _inlineSkipMethodTable = NULL;
    } else {
        ::std::string skipMethodsStr = skipMethods;
        _inlineSkipMethodTable = new (_mm) Method_Table(skipMethodsStr.c_str(), "SKIP_METHODS", true);
    }

    _usesOptimisticBalancedSync = (propertyTable.lookupBool("opt::sync::optimistic", false)
                                   ? propertyTable.lookupBool("opt::sync::optcatch", true)
                                   : false);
}

int32 
Inliner::computeInlineBenefit(CFGNode* node, MethodDesc& methodDesc, InlineNode* parentInlineNode, uint32 loopDepth) 
{
    int32 benefit = 0;

    if (Log::cat_opt_inline()->isDebugEnabled()) {
        Log::cat_opt_inline()->debug << "Computing Inline benefit for "
                               << methodDesc.getParentType()->getName()
                               << "." << methodDesc.getName() << ::std::endl;
    }
    //
    // Size impact
    //
    uint32 size = methodDesc.getByteCodeSize();
    Log::cat_opt_inline()->debug << "  size is " << (int) size << ::std::endl;
    if(size < _inlineSmallMaxByteSize) {
        // Large bonus for smallest methods
        benefit += _inlineSmallBonus;
        Log::cat_opt_inline()->debug << "  isSmall, benefit now = " << benefit << ::std::endl;
    } else if(size < _inlineMediumMaxByteSize) {
        // Small bonus for somewhat small methods
        benefit += _inlineMediumBonus;
        Log::cat_opt_inline()->debug << "  isMedium, benefit now = " << benefit << ::std::endl;
    } else if(size > _inlineLargeMinByteSize) {
        // Penalty for large methods
        benefit -= _inlineLargePenalty * (loopDepth+1); 
        Log::cat_opt_inline()->debug << "  isLarge, benefit now = " << benefit << ::std::endl;
    }
    benefit -= size;
    Log::cat_opt_inline()->debug << "  Subtracting size, benefit now = " << benefit << ::std::endl;

    //
    // Loop depth impact - add bonus for deep call sites
    //
    benefit += _inlineLoopBonus*loopDepth;
    Log::cat_opt_inline()->debug << "  Loop Depth is " << (int) loopDepth
                           << ", benefit now = " << benefit << ::std::endl;

    //
    // Synch bonus - may introduce synch removal opportunities
    //
    if(methodDesc.isSynchronized()){
        benefit += _inlineSynchBonus;
        Log::cat_opt_inline()->debug << "  Method is synchronized, benefit now = " << benefit << ::std::endl;
    }
    //
    // Recursion penalty - discourage recursive inlining
    //
    for(; parentInlineNode != NULL; parentInlineNode = parentInlineNode->getParent()) {
        if(&methodDesc == &(parentInlineNode->getIRManager().getMethodDesc())) {
            benefit -= _inlineRecursionPenalty;
            Log::cat_opt_inline()->debug << "  Subtracted one recursion level, benefit now = " << benefit << ::std::endl;
        }
    }
    
    //
    // Leaf bonus - try to inline leaf methods
    //
    if(isLeafMethod(methodDesc)) {
        benefit += _inlineLeafBonus;
        Log::cat_opt_inline()->debug << "  Added leaf bonus, benefit now = " << benefit << ::std::endl;
    }

    //
    // Exact argument bonus - may introduce specialization opportunities
    //
    Inst* last = node->getLastInst();
    if(last->getOpcode() == Op_DirectCall) {
        MethodCallInst* call = last->asMethodCallInst();
        assert(call != NULL);
        bool exact = true;
        // first 2 opnds are taus; skip them
        assert(call->getNumSrcOperands() >= 2);
        assert(call->getSrc(0)->getType()->tag == Type::Tau);
        assert(call->getSrc(1)->getType()->tag == Type::Tau);
        for(uint32 i = 2; i < call->getNumSrcOperands(); ++i) {
            Opnd* arg = call->getSrc(i);
            assert(arg->getType()->tag != Type::Tau);
            if(arg->getInst()->isConst()) {
                benefit += _inlineExactArgBonus;
                Log::cat_opt_inline()->debug << "  Src " << (int) i
                                       << " is const, benefit now = " << benefit << ::std::endl;
            } else if(arg->getType()->isObject() && Simplifier::isExactType(arg)) {
                benefit += _inlineExactArgBonus;
                Log::cat_opt_inline()->debug << "  Src " << (int) i
                                       << " is exacttype, benefit now = " << benefit << ::std::endl;
            }  else {
                exact = false;
                Log::cat_opt_inline()->debug << "  Src " << (int) i
                                       << " is inexact, benefit now = " << benefit << ::std::endl;
            }
        }
        if(call->getNumSrcOperands() > 2 && exact) {
            benefit += _inlineExactAllBonus;
            Log::cat_opt_inline()->debug << "  Added allexact bonus, benefit now = " << benefit << ::std::endl;
        }
    }        
    
    //
    // Use profile information if available
    //
    if(_doProfileOnlyInlining && _toplevelIRM.getFlowGraph().hasEdgeProfile()) {
        double heatThreshold = _toplevelIRM.getHeatThreshold();
        double nodeCount = node->getFreq();
        double scale = nodeCount / heatThreshold;
        if(scale > 100)
            scale = 100;
        // Remove any loop bonus as this is already accounted for in block count
        benefit -= _inlineLoopBonus*loopDepth;
        // Scale by call site 'hotness'.
        benefit = (uint32) ((double) benefit * scale);

        Log::cat_opt_inline()->debug << "  HeatThreshold=" << heatThreshold
                               << ", nodeCount=" << nodeCount
                               << ", scale=" << scale
                               << "; benefit now = " << benefit
                               << ::std::endl;
    }
    return benefit;
}

class JavaByteCodeLeafSearchCallback : public JavaByteCodeParserCallback {
public:
    JavaByteCodeLeafSearchCallback() {
        leaf = true;
    }
    void parseInit() {}
    // called after parsing ends, but not if an error occurs
    void parseDone() {}
    // called when an error occurs parsing the byte codes
    void parseError() {}
    bool isLeaf() const { return leaf; }
protected:
    bool leaf;

    void offset(uint32 offset) {};
    void offset_done(uint32 offset) {};
    void nop()  {}
    void aconst_null()  {}
    void iconst(int32)  {}
    void lconst(int64)  {}
    void fconst(float)  {}
    void dconst(double)  {}
    void bipush(int8)  {}
    void sipush(int16)  {}
    void ldc(uint32)  {}
    void ldc2(uint32)  {}
    void iload(uint16 varIndex)  {}
    void lload(uint16 varIndex)  {}
    void fload(uint16 varIndex)  {}
    void dload(uint16 varIndex)  {}
    void aload(uint16 varIndex)  {}
    void iaload()  {}
    void laload()  {}
    void faload()  {}
    void daload()  {}
    void aaload()  {}
    void baload()  {}
    void caload()  {}
    void saload()  {}
    void istore(uint16 varIndex,uint32 off)  {}
    void lstore(uint16 varIndex,uint32 off)  {}
    void fstore(uint16 varIndex,uint32 off)  {}
    void dstore(uint16 varIndex,uint32 off)  {}
    void astore(uint16 varIndex,uint32 off)  {}
    void iastore()  {}
    void lastore()  {}
    void fastore()  {}
    void dastore()  {}
    void aastore()  {}
    void bastore()  {}
    void castore()  {}
    void sastore()  {}
    void pop()  {}
    void pop2()  {}
    void dup()  {}
    void dup_x1()  {}
    void dup_x2()  {}
    void dup2()  {}
    void dup2_x1()  {}
    void dup2_x2()  {}
    void swap()  {}
    void iadd()  {}
    void ladd()  {}
    void fadd()  {}
    void dadd()  {}
    void isub()  {}
    void lsub()  {}
    void fsub()  {}
    void dsub()  {}
    void imul()  {}
    void lmul()  {}
    void fmul()  {}
    void dmul()  {}
    void idiv()  {}
    void ldiv()  {}
    void fdiv()  {}
    void ddiv()  {}
    void irem()  {}
    void lrem()  {}
    void frem()  {}
    void drem()  {}
    void ineg()  {}
    void lneg()  {}
    void fneg()  {}
    void dneg()  {}
    void ishl()  {}
    void lshl()  {}
    void ishr()  {}
    void lshr()  {}
    void iushr()  {}
    void lushr()  {}
    void iand()  {}
    void land()  {}
    void ior()  {}
    void lor()  {}
    void ixor()  {}
    void lxor()  {}
    void iinc(uint16 varIndex,int32 amount)  {}
    void i2l()  {}
    void i2f()  {}
    void i2d()  {}
    void l2i()  {}
    void l2f()  {}
    void l2d()  {}
    void f2i()  {}
    void f2l()  {}
    void f2d()  {}
    void d2i()  {}
    void d2l()  {}
    void d2f()  {}
    void i2b()  {}
    void i2c()  {}
    void i2s()  {}
    void lcmp()  {}
    void fcmpl()  {}
    void fcmpg()  {}
    void dcmpl()  {}
    void dcmpg()  {}
    void ifeq(uint32 targetOffset,uint32 nextOffset)  {}
    void ifne(uint32 targetOffset,uint32 nextOffset)  {}
    void iflt(uint32 targetOffset,uint32 nextOffset)  {}
    void ifge(uint32 targetOffset,uint32 nextOffset)  {}
    void ifgt(uint32 targetOffset,uint32 nextOffset)  {}
    void ifle(uint32 targetOffset,uint32 nextOffset)  {}
    void if_icmpeq(uint32 targetOffset,uint32 nextOffset)  {}
    void if_icmpne(uint32 targetOffset,uint32 nextOffset)  {}
    void if_icmplt(uint32 targetOffset,uint32 nextOffset)  {}
    void if_icmpge(uint32 targetOffset,uint32 nextOffset)  {}
    void if_icmpgt(uint32 targetOffset,uint32 nextOffset)  {}
    void if_icmple(uint32 targetOffset,uint32 nextOffset)  {}
    void if_acmpeq(uint32 targetOffset,uint32 nextOffset)  {}
    void if_acmpne(uint32 targetOffset,uint32 nextOffset)  {}
    void goto_(uint32 targetOffset,uint32 nextOffset)  {}
    void jsr(uint32 offset, uint32 nextOffset)  {}
    void ret(uint16 varIndex)  {}
    void tableswitch(JavaSwitchTargetsIter*)  {}
    void lookupswitch(JavaLookupSwitchTargetsIter*)  {}
    void ireturn(uint32 off)  {}
    void lreturn(uint32 off)  {}
    void freturn(uint32 off)  {}
    void dreturn(uint32 off)  {}
    void areturn(uint32 off)  {}
    void return_(uint32 off)  {}
    void getstatic(uint32 constPoolIndex)  {}
    void putstatic(uint32 constPoolIndex)  {}
    void getfield(uint32 constPoolIndex)  {}
    void putfield(uint32 constPoolIndex)  {}
    void invokevirtual(uint32 constPoolIndex)  { leaf = false; }
    void invokespecial(uint32 constPoolIndex)  { leaf = false; }
    void invokestatic(uint32 constPoolIndex)  { leaf = false; }
    void invokeinterface(uint32 constPoolIndex,uint32 count)  { leaf = false; }
    void new_(uint32 constPoolIndex)  {}
    void newarray(uint8 type)  {}
    void anewarray(uint32 constPoolIndex)  {}
    void arraylength()  {}
    void athrow()  {}
    void checkcast(uint32 constPoolIndex)  {}
    int  instanceof(const uint8* bcp, uint32 constPoolIndex, uint32 off)  {return 3;}
    void monitorenter()  {}
    void monitorexit()  {}
    void multianewarray(uint32 constPoolIndex,uint8 dimensions)  {}
    void ifnull(uint32 targetOffset,uint32 nextOffset)  {}
    void ifnonnull(uint32 targetOffset,uint32 nextOffset) {}
};

bool
Inliner::isLeafMethod(MethodDesc& methodDesc) {
    if(methodDesc.isJavaByteCodes()) {
        uint32 size = methodDesc.getByteCodeSize();
        const Byte* bytecodes = methodDesc.getByteCodes();
        ByteCodeParser parser((const uint8*)bytecodes,size);
        JavaByteCodeLeafSearchCallback leafTester;
        parser.parse(&leafTester);
        return leafTester.isLeaf();
    }
    return false;
}

void
Inliner::reset()
{
    _hasProfileInfo = true; // _irm.getFlowGraph().hasEdgeProfile();
    _inlineCandidates.clear();
    _useInliningTranslatorCall = false;
}

bool
Inliner::canInlineFrom(MethodDesc& methodDesc)
{
    // 
    // Test if calls in this method should be inlined
    //
    bool doInline = !methodDesc.isClassInitializer() && !methodDesc.getParentType()->isLikelyExceptionType();
    Log::cat_opt_inline()->debug << "Can inline from " << methodDesc.getParentType()->getName() << "." << methodDesc.getName() << " == " << doInline << ::std::endl;
    return doInline;
}

bool
Inliner::canInlineInto(MethodDesc& methodDesc)
{
    // 
    // Test if a call to this method should be inlined
    //
    bool doSkip = (_inlineSkipMethodTable == NULL) ? false : _inlineSkipMethodTable->accept_this_method(methodDesc);
    if(doSkip)
        Log::cat_opt_inline()->debug << "Skipping inlining of " << methodDesc.getParentType()->getName() << "." << methodDesc.getName() << ::std::endl;    
    bool doInline = !doSkip && !methodDesc.isNative() && !methodDesc.isNoInlining() && !methodDesc.getParentType()->isLikelyExceptionType();
    Log::cat_opt_inline()->debug << "Can inline this " << methodDesc.getParentType()->getName() << "." << methodDesc.getName() << " == " << doInline << ::std::endl;
    return doInline;
}

void
Inliner::connectRegion(InlineNode* inlineNode) {
    if(inlineNode == _inlineTree.getRoot())
        // This is the top level graph.
        return;
    
    if(_useInliningTranslatorCall)
        // Inlining translator connects the region during translation
        return;

    IRManager &inlinedIRM = inlineNode->getIRManager();
    FlowGraph &inlinedFlowGraph = inlinedIRM.getFlowGraph();
    Inst *callInst = inlineNode->getCallInst();
    MethodDesc &methodDesc = inlinedIRM.getMethodDesc();
    
    // Mark inlined region
    Opnd *obj = 0;
    if (methodDesc.isClassInitializer()) {
        assert(callInst->getNumSrcOperands() >= 3);
        Opnd *obj = callInst->getSrc(2); // this parameter for DirectCall
        if (obj && !obj->isNull()) obj = 0;
    }
    Inst* entryMarker = 
        obj ? _instFactory.makeMethodMarker(MethodMarkerInst::Entry, 
        &methodDesc, obj)
        : _instFactory.makeMethodMarker(MethodMarkerInst::Entry, &methodDesc);
    Inst* exitMarker = 
        obj ? _instFactory.makeMethodMarker(MethodMarkerInst::Exit, 
        &methodDesc, obj)
        : _instFactory.makeMethodMarker(MethodMarkerInst::Exit, 
        &methodDesc);
    CFGNode* entry = inlinedFlowGraph.getEntry();
    entry->prependAfterCriticalInst(entryMarker);
    CFGNode* retNode = inlinedFlowGraph.getReturn();
    if(retNode != NULL)
        retNode->append(exitMarker);
    
    // Fuse callsite arguments with incoming parameters
    uint32 numArgs = callInst->getNumSrcOperands() - 2; // omit taus
    Opnd *tauNullChecked = callInst->getSrc(0);
    Opnd *tauTypesChecked = callInst->getSrc(1);
    assert(tauNullChecked->getType()->tag == Type::Tau);
    assert(tauTypesChecked->getType()->tag == Type::Tau);
    Inst* first = entry->getFirstInst();
    Inst* inst;
    Opnd* thisPtr = 0;
    uint32 j;
    for(j = 0, inst = first->next(); j < numArgs && inst != first;) {
        switch (inst->getOpcode()) {
        case Op_DefArg:
            {
                Opnd* dst = inst->getDst(); 
                Opnd* src = callInst->getSrc(j+2); // omit taus
                Inst* copy = _instFactory.makeCopy(dst, src);
                copy->insertBefore(inst);
                inst->unlink();
                inst = copy;
                ++j;
                if (!thisPtr) {
                    src = thisPtr;
                }
            }
            break;
        case Op_TauHasType:
            {
                // substitute tau parameter for hasType test of parameter
                Opnd* src = inst->getSrc(0);
                if (tauTypesChecked->getInst()->getOpcode() != Op_TauUnsafe) {
                    // it's worth substituting the tau
                    for (uint32 i=0; i<numArgs; ++i) {
                        if (src == callInst->getSrc(i+2)) {
                            // it is a parameter
                            Opnd* dst = inst->getDst();
                            Inst* copy = _instFactory.makeCopy(dst, tauTypesChecked);
                            copy->insertBefore(inst);
                            inst->unlink();
                            inst = copy;
                        }
                    }
                }
            }
            break;
        case Op_TauIsNonNull:
            {
                // substitute tau parameter for isNonNull test of this parameter
                Opnd* src = inst->getSrc(0);
                if (src == thisPtr) {
                    Opnd* dst = inst->getDst();
                    Inst* copy = _instFactory.makeCopy(dst, tauNullChecked);
                    copy->insertBefore(inst);
                    inst->unlink();
                    inst = copy;
                }
            }
            break;
        case Op_TauHasExactType:
        default:
            break;
        }
        inst = inst->next();
    }
    assert(j == numArgs);
    
    //
    // Convert returns in inlined region into stvar/ldvar
    //
    if(retNode != NULL) {
        Opnd* dst = callInst->getDst();
        VarOpnd* retVar = NULL;
        if(!dst->isNull())
            retVar = _opndManager.createVarOpnd(dst->getType(), false);
        
        // Iterate return sites
        assert(retNode != NULL);
        bool isSsa = inlinedIRM.getInSsa();
        Opnd** phiArgs = isSsa ? new (_toplevelIRM.getMemoryManager()) Opnd*[retNode->getInDegree()] : NULL;
        uint32 phiCount = 0;
        const CFGEdgeDeque& inEdges = retNode->getInEdges();
        CFGEdgeDeque::const_iterator eiter;
        for(eiter = inEdges.begin(); eiter != inEdges.end(); ++eiter) {
            CFGNode* retSite = (*eiter)->getSourceNode();
            Inst* ret = retSite->getLastInst();
            assert(ret->getOpcode() == Op_Return);
            if(retVar != NULL) {
                Opnd* tmp = ret->getSrc(0);
                assert(tmp != NULL);
                if(isSsa) {
                    SsaVarOpnd* ssaVar = _opndManager.createSsaVarOpnd(retVar);
                    phiArgs[phiCount++] = ssaVar;
                    retSite->append(_instFactory.makeStVar(ssaVar, tmp));
                } else {
                    retSite->append(_instFactory.makeStVar(retVar, tmp));
                }
            }
            ret->unlink();
        }
        if(retVar != NULL) {
            // Insert phi and ldVar after call site
            CFGNode* joinNode = inlinedFlowGraph.splitReturnNode();
            joinNode->setFreq(retNode->getFreq());
            ((CFGEdge*) joinNode->findTarget(retNode))->setEdgeProb(1.0);
            if(isSsa) {
                SsaVarOpnd* ssaVar = _opndManager.createSsaVarOpnd(retVar);
                joinNode->append(_instFactory.makePhi(ssaVar, phiCount, phiArgs));
                joinNode->append(_instFactory.makeLdVar(dst, ssaVar));
            } else {
                joinNode->append(_instFactory.makeLdVar(dst, retVar));
            }

            // Set return operand.
            assert(inlinedIRM.getReturnOpnd() == NULL);
            inlinedIRM.setReturnOpnd(dst);
        }
    }
    
    //
    // Insert explicit catch_all/monitor_exit/rethrow at exception exits for a synchronized inlined region
    //
    if(methodDesc.isSynchronized() && inlinedIRM.getFlowGraph().getUnwind()) {
        // Add monitor exit to unwind.
        CFGNode* unwind = inlinedFlowGraph.getUnwind();
        const CFGEdgeDeque& unwindEdges = unwind->getInEdges();
        
        // Test if an exception exit exists
        if(!unwindEdges.empty()) {
            CFGNode* dispatch = inlinedFlowGraph.createDispatchNode();
            
            // Insert catch all
            Opnd* ex = _opndManager.createSsaTmpOpnd(_typeManager.getSystemObjectType());
            CFGNode* handler = inlinedFlowGraph.createCatchNode(0, ex->getType());
            handler->append(_instFactory.makeCatch(ex));
            if(methodDesc.isStatic()) {
                // Release class monitor
                handler->append(_instFactory.makeTypeMonitorExit(methodDesc.getParentType()));
            } else {
                // Release object monitor
                assert(callInst->getNumSrcOperands() > 2);
                Opnd* obj = callInst->getSrc(2); // object is arg 2;
                TranslatorFlags& translatorFlags = *inlinedIRM.getCompilationContext()->getTranslatorFlags();
                if (!translatorFlags.ignoreSync && !translatorFlags.syncAsEnterFence) {
                    if (_usesOptimisticBalancedSync) {
                        // We may need to insert an optimistically balanced monitorexit.
                        // Note that we shouldn't have an un-optimistic balanced monitorexit,
                        // since there is at least one edge to UNWIND; this edge should have
                        // prevented balancing for a synchronized method, which would have
                        // a missing monitorexit on the exception path.
                        
                        CFGNode *aRetNode = retNode;
                        bool done = false;
                        int count = 0;
                        // We should have an optbalmonexit on a return path
                        while (!done && aRetNode != NULL && !aRetNode->getInEdges().empty()) {
                            const CFGEdgeDeque& retEdges = aRetNode->getInEdges();
                            if (!retEdges.empty()) {
                                CFGEdge *anEdgeToReturn = *retEdges.begin();
                                CFGNode *aMonexitNode = anEdgeToReturn->getSourceNode();
                                Inst *lastInst = aMonexitNode->getLastInst();
                                Inst *firstInst = aMonexitNode->getFirstInst();
                                while (lastInst != firstInst) {
                                    if (lastInst->getOpcode() == Op_OptimisticBalancedMonitorExit) {
                                        Opnd *srcObj = lastInst->getSrc(0);
                                        Opnd *lockAddr = lastInst->getSrc(1);
                                        Opnd *enterDst = lastInst->getSrc(2);
                                        
                                        handler->append(_instFactory.makeOptimisticBalancedMonitorExit(srcObj,
                                                                                                       lockAddr,
                                                                                                       enterDst));
                                        done = true;
                                        break;
                                    }
                                    lastInst = lastInst->prev();
                                }
                                if (!done) {
                                    // we may have empty nodes or something, so try a predecessor.
                                    assert(count < 10); // try to bound this search
                                    count += 1;
                                    // we have to search further.
                                    aRetNode = aMonexitNode;
                                    assert(aRetNode);
                                    assert(!aRetNode->getInEdges().empty());
                                }
                            } else {
                                assert(0);
                            }
                        }
                        assert(done);
                    } else {
                        Opnd* tauSafe = _opndManager.createSsaTmpOpnd(_typeManager.getTauType());
                        handler->append(_instFactory.makeTauSafe(tauSafe)); // monenter success guarantees non-null
                        handler->append(_instFactory.makeTauMonitorExit(obj, tauSafe));
                    }
                }
            }
            
            // Insert rethrow
            CFGNode* rethrow = inlinedFlowGraph.createBlockNode();
            rethrow->append(_instFactory.makeThrow(Throw_NoModifier, ex));
            
            // Redirect exception exits to monitor_exit
            CFGEdgeDeque::const_iterator eiter;
            for(eiter = unwindEdges.begin(); eiter != unwindEdges.end(); ) {
                CFGEdge* edge = *eiter;
                ++eiter;
                inlinedFlowGraph.replaceEdgeTarget(edge, dispatch);
            }
            assert(unwindEdges.empty());
            inlinedFlowGraph.addEdge(dispatch, handler);
            inlinedFlowGraph.addEdge(handler, rethrow);
            inlinedFlowGraph.addEdge(handler, unwind);
            inlinedFlowGraph.addEdge(rethrow, unwind);
        }
    }
    
    //
    // Add type initialization if necessary
    //
    if ((methodDesc.isStatic() || methodDesc.isInstanceInitializer()) &&
        methodDesc.getParentType()->needsInitialization()) {
        // initialize type for static methods
        Inst* initType = _instFactory.makeInitType(methodDesc.getParentType());
        entry->prepend(initType);
        inlinedFlowGraph.splitNodeAtInstruction(initType);
        inlinedFlowGraph.addEdge(entry, inlinedFlowGraph.getUnwind());
    }
}

void
Inliner::inlineAndProcessRegion(InlineNode* inlineNode,
                                DominatorTree* dtree, LoopTree* ltree) {
    IRManager &inlinedIRM = inlineNode->getIRManager();
    FlowGraph &inlinedFlowGraph = inlinedIRM.getFlowGraph();
    CFGNode *callNode = inlineNode->getCallNode();
    Inst *callInst = inlineNode->getCallInst();
    MethodDesc &methodDesc = inlinedIRM.getMethodDesc();
    
    if (Log::cat_opt_inline()->isDebugEnabled()) {
        Log::out() << "inlineAndProcessRegion "
                   << methodDesc.getParentType()->getName() << "."
                   << methodDesc.getName() << ::std::endl;
    }

    //
    // Scale block counts in the inlined region for this particular call site
    //
    if (callNode != NULL)
        scaleBlockCounts(callNode, inlinedIRM);
    
    //
    // Update priority queue with calls in this region
    //
    processRegion(inlineNode, dtree, ltree);
    
    //
    // If top level flowgraph 
    //
    if (inlineNode == _inlineTree.getRoot()) {
        Log::cat_opt_inline()->debug << "inlineNode is root" << ::std::endl;
        return;
    }

    // 
    // Splice region into top level flowgraph at call site
    //
    assert(callInst->getOpcode() == Op_DirectCall);
    Log::cat_opt_inline()->ir << "Inlining " << methodDesc.getParentType()->getName() 
        << "." << methodDesc.getName() << ::std::endl;

    Log::cat_opt_inline()->ir 
        << "callee = " << methodDesc.getParentType()->getName() << "." << methodDesc.getName() 
        << ::std::endl
        << "caller = " << inlinedIRM.getParent()->getMethodDesc().getParentType()->getName() << "."
        << inlinedIRM.getParent()->getMethodDesc().getName()
        << ::std::endl;

    //
    // update inlining-related information in all 'call' instructions of the inlined method
    //
    
    const CFGNodeDeque& nodes = inlinedFlowGraph.getNodes();
    CFGNodeDeque::const_iterator niter;
    uint16 count = 0;

    assert(callInst->isMethodCall());
    InlineInfo& call_ii = *callInst->asMethodCallInst()->getInlineInfoPtr();

    call_ii.printLevels(Log::cat_opt_inline()->ir);
    Log::cat_opt_inline()->ir << ::std::endl;

    for(niter = nodes.begin(); niter != nodes.end(); ++niter) {
        CFGNode* node = *niter;

        Inst* first = node->getFirstInst();
        Inst* i;
        for(i=first->next(); i != first; i=i->next()) {
            InlineInfo *ii = i->getCallInstInlineInfoPtr();
            // ii should be non-null for each call instruction
            if ( ii ) {
                //
                // InlineInfo order is parent-first 
                //     (ii might be non-empty when translation-level inlining is on)
                //
                ii->prependLevel(&methodDesc);
                ii->prependInlineChain(call_ii);
                // ii->inlineChain remains at the end
                
                Log::cat_opt_inline()->ir << "      call No." << count++ << " ";
                ii->printLevels(Log::cat_opt_inline()->ir);
            }
        }
    }

    //
    // Splice the inlined region into the top-level flowgraph
    //
    IRManager* parent = inlinedIRM.getParent();
    assert(parent);
    parent->getFlowGraph().spliceFlowGraphInline(callNode, callInst, inlinedFlowGraph);
}

InlineNode*
Inliner::getNextRegionToInline() {
    // If in DPGO profiling mode, don't inline if profile information is not available.
    if(_doProfileOnlyInlining && !_toplevelIRM.getFlowGraph().hasEdgeProfile()) {
        return NULL;
    }
    
    CFGNode* callNode = 0;
    InlineNode* inlineParentNode = 0;
    MethodCallInst* call = 0;
    MethodDesc* methodDesc = 0;
    uint32 newByteSize = 0;
    
    //
    // Search priority queue for next inline candidate
    //
    bool found = false;
    while(!_inlineCandidates.empty() && !found) {
        // Find new candidate.
        callNode = _inlineCandidates.top().callNode;
        inlineParentNode = _inlineCandidates.top().inlineNode;
        _inlineCandidates.pop();
        
        call = callNode->getLastInst()->asMethodCallInst();
        assert(call != NULL);
        methodDesc = call->getMethodDesc();
        
        // If candidate would cause top level method to exceed size threshold, throw away.
        
        uint32 methodByteSize = methodDesc->getByteCodeSize();
        methodByteSize = (methodByteSize <= CALL_COST) ? 1 : methodByteSize-CALL_COST;
        newByteSize = _currentByteSize + methodByteSize;
        double factor = ((double) newByteSize) / ((double) _initByteSize);
        if(newByteSize < _minInlineStop || factor <= _maxInlineGrowthFactor
           || (methodByteSize < _inlineSmallMaxByteSize)) {
            found = true;
        } else {
            Log::cat_opt_inline()->debug << "Skip inlining " << methodDesc->getParentType()->getName() << "." << methodDesc->getName() << ::std::endl;
            Log::cat_opt_inline()->debug << "methodByteSize=" 
                                   << (int)methodByteSize
                                   << ", newByteSize = " << (int)newByteSize
                                   << ", factor=" << factor
                                   << ::std::endl;
        }
    }
    
    if(!found) {
        // No more candidates.  Done with inlining.
        assert(_inlineCandidates.empty());
        Log::cat_opt_inline()->debug << "Done inlining " << ::std::endl;
        return NULL;
    }
    
    //
    // Set candidate as current inlined region
    //
    Log::cat_opt_inline()->info2 << "Opt:   Inline " << methodDesc->getParentType()->getName() << "." << methodDesc->getName() << 
        methodDesc->getSignatureString() << ::std::endl;
    
    // Generate flowgraph for new region
    oldMethodId = _instFactory.getMethodId();
    uint32 id = methodDesc->getUniqueId();
    _instFactory.setMethodId(((uint64)id)<<32);
    
    Opnd *returnOpnd = 0;
    if (_useInliningTranslatorCall) {
        if(call->getDst()->isNull())
            returnOpnd = _opndManager.getNullOpnd();
        else 
            returnOpnd = _opndManager.createSsaTmpOpnd(call->getDst()->getType());
    }        
    IRManager* inlinedIRM = new (_mm) IRManager(_toplevelIRM, *methodDesc, returnOpnd);
    FlowGraph* inlinedFG = &inlinedIRM->getFlowGraph();
    
    // Augment inline tree
    InlineNode *inlineNode = new (_mm) InlineNode(*inlinedIRM, call, callNode);
    
    inlineParentNode->addChild(inlineNode);
    
    if(_useInliningTranslatorCall) {
        // Use inlining translator.       
        
        uint32 numArgs = call->getNumSrcOperands() - 2; // pass taus separately
        MemoryManager localMemManager(numArgs*sizeof(Opnd*), "FlowGraph::inlineMethod.localMemManager");
#ifndef NDEBUG
        Opnd* tauNullChecked = call->getSrc(0);
        assert(tauNullChecked->getType()->tag == Type::Tau);
        Opnd* tauTypesChecked = call->getSrc(1);
        assert(tauTypesChecked->getType()->tag == Type::Tau);
#endif
		Opnd** argOpnds = new (localMemManager) Opnd*[numArgs];
        for(uint32 j = 0; j < numArgs; ++j) {
            Opnd *arg = call->getSrc(j+2); // skip taus
            assert(arg->getType()->tag != Type::Tau);
            argOpnds[j] = arg;
        }


        TranslatorIntfc::translateByteCodesInline(*inlinedIRM,
                                                  numArgs, argOpnds, 0);
        returnOpnd = inlinedIRM->getReturnOpnd();
        if(returnOpnd && !returnOpnd->isNull()) {
            Inst *copyInst = _instFactory.makeCopy(call->getDst(), returnOpnd);
            CFGNode *fgReturnNode = inlinedFG->getReturn();
            assert(fgReturnNode != NULL);
            if (Log::cat_opt_inline()->isDebugEnabled()) {
                Log::out() << "Adding copy instruction: ";
                copyInst->print(Log::out());
                Log::out() << ::std::endl << " to block" << ::std::endl;
                fgReturnNode->print(Log::out());
                Log::out() << ::std::endl;
            }
            fgReturnNode->append(copyInst);
            inlinedIRM->setReturnOpnd(call->getDst());
            if (Log::cat_opt_inline()->isDebugEnabled()) {
                Log::out() << "After block changed: " << ::std::endl;
                fgReturnNode->print(Log::out());
                Log::out() << ::std::endl;
            }
        }
        
        if ((methodDesc->isStatic() || methodDesc->isInstanceInitializer()) &&
            methodDesc->getParentType()->needsInitialization()) {
            // initialize type for static methods
            Inst* initType = _instFactory.makeInitType(methodDesc->getParentType());
            inlinedFG->getEntry()->prepend(initType);
            inlinedFG->splitNodeAtInstruction(initType);
            inlinedFG->addEdge(inlinedFG->getEntry(), inlinedFG->getUnwind());
        }
    } else {
        // Call regular translator - this must be used to annotate profile information onto the 
        // inlined region
        TranslatorIntfc::translateByteCodes(*inlinedIRM);
    }

    // Cleanup
    inlinedFG->cleanupPhase();

    // Save state.
    _currentByteSize = newByteSize;
    assert(inlineNode);
    return inlineNode;
}

void
Inliner::processDominatorNode(InlineNode *inlineNode, DominatorNode* dnode, LoopTree* ltree) {
    if(dnode == NULL)
        return;

    //
    // Process this node for inline candidates
    //
    CFGNode* node = dnode->getNode();
    if(node->isBlockNode()) {
        // Search for call site
        Inst* last = node->getLastInst();
        if(last->getOpcode() == Op_DirectCall) {
            // Process call site
            MethodCallInst* call = last->asMethodCallInst();
            assert(call != NULL);
            MethodDesc* methodDesc = call->getMethodDesc();

            Log::cat_opt_inline()->debug << "Considering inlining instruction I"
                                   << (int)call->getId() << ::std::endl;
            if(canInlineInto(*methodDesc)) {
                uint32 size = methodDesc->getByteCodeSize();
                int32 benefit = computeInlineBenefit(node, *methodDesc, inlineNode, ltree->getLoopDepth(node));
                assert(size > 0);
                Log::cat_opt_inline()->debug << "Inline benefit " << methodDesc->getParentType()->getName() << "." << methodDesc->getName() << " == " << (int) benefit << ::std::endl;
                if(0 < size && benefit > _minBenefitThreshold) {
                    // Inline candidate
                    Log::cat_opt_inline()->debug << "Add to queue" << ::std::endl;
                    _inlineCandidates.push(CallSite(benefit, node, inlineNode));
                } else {
                    Log::cat_opt_inline()->debug << "Will not inline" << ::std::endl;
                }
            }
        }
    }

    //
    // Skip exception control flow unless directed
    //
    if(node->isBlockNode() || !_inlineSkipExceptionPath)
        processDominatorNode(inlineNode, dnode->getChild(), ltree);

    // 
    // Process siblings in dominator tree
    //
    processDominatorNode(inlineNode, dnode->getSiblings(), ltree);
}

void
Inliner::processRegion(InlineNode* inlineNode, DominatorTree* dtree, LoopTree* ltree) {
    if(!canInlineFrom(inlineNode->getIRManager().getMethodDesc()))
        return;

    //
    // Process region for inline candidates
    //
    processDominatorNode(inlineNode, dtree->getDominatorRoot(), ltree);
}

void 
Inliner::scaleBlockCounts(CFGNode* callSite, IRManager& inlinedIRM) {
    Log::cat_opt_inline()->debug << "Scaling block counts for callsite in block "
                           << (int) callSite->getId() << ::std::endl;
    if(!_toplevelIRM.getFlowGraph().hasEdgeProfile()) {
        // No profile information to scale
        Log::cat_opt_inline()->debug << "No profile information to scale!" << ::std::endl;
        return;
    }

    //
    // Compute scale from call site count and inlined method count
    //
    double callFreq = callSite->getFreq();
    FlowGraph &inlinedRegion = inlinedIRM.getFlowGraph();
    double methodFreq = inlinedRegion.getEntry()->getFreq();
    double scale = callFreq / ((methodFreq != 0.0) ? methodFreq : 1.0);

    Log::cat_opt_inline()->debug << "callFreq=" << callFreq
                           << ", methodFreq=" << methodFreq
                           << ", scale=" << scale << ::std::endl;
    //
    // Apply scale to each block in inlined region
    //
    const CFGNodeDeque& nodes = inlinedRegion.getNodes();
    CFGNodeDeque::const_iterator i;
    for(i = nodes.begin(); i != nodes.end(); ++i) {
        CFGNode* node = *i;
        node->setFreq(node->getFreq()*scale);
    }
}


double 
Inliner::getProfileMethodCount(CompilationInterface& compileIntf, MethodDesc& methodDesc) {
    //
    // Get the entry count for a given method 
    //
    if ( compileIntf.isDynamicProfiling() ) {
        // Online
        MemoryManager mm(1024, "Inliner::getProfileMethodCount.mm");
        EdgeProfile* profile = new (mm) EdgeProfile(mm, methodDesc);

        return profile->getEntryFreq();
    } else {
        // Offline
        EdgeProfile* profile =  profileCtrl.readProfileFromFile(methodDesc);
        if(profile == NULL)
            return 0;

        return profile->getEntryFreq();
    }
}


void 
InlineNode::print(::std::ostream& os) {
    MethodDesc& _methodDesc = getIRManager().getMethodDesc();
    os << _methodDesc.getParentType()->getName() << "." << _methodDesc.getName() << _methodDesc.getSignatureString();
}

void 
InlineNode::printTag(::std::ostream& os) {
    MethodDesc& _methodDesc = getIRManager().getMethodDesc();
    os << "M" << (int) _methodDesc.getId(); 

}



uint32
InlineTree::computeCheckSum(InlineNode* node) {
    if(node == NULL)
        return 0;

    IRManager& irm = node->getIRManager();
    MethodDesc& desc = irm.getMethodDesc();
#ifdef PLATFORM_POSIX
    __gnu_cxx::hash<const char*> hash;
#else
    stdext::hash_compare<const char*> hash; 
#endif
    size_t sum = hash(desc.getParentType()->getName());
    sum += hash(desc.getName());
    sum += computeCheckSum(node->getSiblings());
    sum += computeCheckSum(node->getChild());
    return (uint32) sum;
}

} //namespace Jitrino
 
