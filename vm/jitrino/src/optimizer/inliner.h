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
 * @version $Revision: 1.9.22.4 $
 *
 */

#ifndef _INLINER_H_
#define _INLINER_H_

#include "StlPriorityQueue.h"
#include "Tree.h"
#include "irmanager.h"

#define PRAGMA_INLINE "org/vmmagic/pragma/Inline"

namespace Jitrino {

class MemoryManager;
class IRManager;
class TypeManager;
class InstFactory;
class OpndManager;
class Node;
class MethodInst;
class Inst;
class Opnd;
class FlowGraph;
class CompilationInterface;
class MethodDesc;
class DominatorTree;
class DominatorNode;
class LoopTree;
class Method_Table;

class InliningContext {
public:
    InliningContext(uint32 _nArgs, Type** _argTypes) : nArgs(_nArgs), argTypes(_argTypes) {}
    uint32 getNumArgs() const {return nArgs;}
    Type** getArgTypes() const {return argTypes;}
private:
    uint32 nArgs;
    Type** argTypes;
};

class InlineNode : public TreeNode {
public:
    InlineNode(IRManager& irm, Inst *callInst, Node *callNode) : _irm(irm), _callInst(callInst), _callNode(callNode) {}
    InlineNode* getChild()    {return (InlineNode*) child;}
    InlineNode* getSiblings() {return (InlineNode*) siblings;}
    InlineNode* getParent()   {return (InlineNode*) parent;}
    IRManager&  getIRManager()      { return _irm; }
    Inst*       getCallInst() { return _callInst; }
    Node*    getCallNode() { return _callNode; }
    void print(::std::ostream& os);
    void printTag(::std::ostream& os);
private:
    IRManager&  _irm;
    Inst*       _callInst;
    Node*    _callNode;
};

class InlineTree : public Tree {
public:
    InlineTree(InlineNode* r) {
        root = r;
    }
    InlineNode *getRoot() { return (InlineNode*)root; }

    uint32 computeCheckSum() { return computeCheckSum(getRoot()); }
private:
    uint32 computeCheckSum(InlineNode* node);
};

class Inliner
{
public:
    Inliner(SessionAction* argSource, MemoryManager& mm, IRManager& irm,  
        bool doProfileOnly, bool usePriorityQueue, const char* inlinerPipelineName);

    // Inline this method into the current CFG and process it for further
    // inline candidates.  If the argument is the top level CFG, only processing
    // occurs.
    void inlineRegion(InlineNode* inlineNode);

    // Connect input and return operands of the region to the top-level method.  Do not yet splice.
    void connectRegion(InlineNode* inlineNode);

    void compileAndConnectRegion(InlineNode* inlineNode, CompilationContext& inlineCC);

    // Searches the flowgraph for the next inline candidate method.
    InlineNode* getNextRegionToInline(CompilationContext& inlineCC);

    InlineTree& getInlineTree() { return _inlineTree; } 

    void reset();

    InlineNode* createInlineNode(CompilationContext& inlineCC, MethodCallInst* call);

    static double getProfileMethodCount(CompilationInterface& compileIntf, MethodDesc& methodDesc); 
    
    static void runInlinerPipeline(CompilationContext& inlineCC, const char* pipeName);

    /**runs inliner for a specified top level call. 
     * If call==NULL runs inliner for all calls in a method: priority queue is used to find methods to inline
     */
    void runInliner(MethodCallInst* call);

    void setConnectEarly(bool early) {connectEarly = early;}

private:

    class CallSite {
    public:
        CallSite(int32 benefit, Node* callNode, InlineNode* inlineNode) : benefit(benefit), callNode(callNode), inlineNode(inlineNode) {}

        int32 benefit;
        Node* callNode;
        InlineNode* inlineNode;
    };

    class CallSiteCompare {
    public:
        bool operator()(const CallSite& site1, const CallSite& site2) { return site1.benefit < site2.benefit; }
    };

    void scaleBlockCounts(Node* callSite, IRManager& inlinedIRM);
    void processRegion(InlineNode *inlineNode, DominatorTree* dtree, LoopTree* ltree);
    void processDominatorNode(InlineNode *inlineNode, DominatorNode* dtree, LoopTree* ltree);

    void runTranslatorSession(CompilationContext& inlineCC);

    // True if this method should be processed for further inlining.  I.e., 
    // can we inline the calls in this method?
    bool canInlineFrom(MethodDesc& methodDesc);

    // True if this method may be inlined into a calling method.
    bool canInlineInto(MethodDesc& methodDesc);

    bool isLeafMethod(MethodDesc& methodDesc);

    int32 computeInlineBenefit(Node* node, MethodDesc& methodDesc, InlineNode* parentInlineNode, uint32 loopDepth);

    MemoryManager& _tmpMM;
    IRManager& _toplevelIRM;
    TypeManager& _typeManager;
    InstFactory& _instFactory;
    OpndManager& _opndManager;

    bool _hasProfileInfo;
    
    StlPriorityQueue<CallSite, StlVector<CallSite>, CallSiteCompare> _inlineCandidates;
    uint32 _initByteSize;
    uint32 _currentByteSize;

    InlineTree _inlineTree;

    bool _doProfileOnlyInlining;
    bool _useInliningTranslator;

    double _maxInlineGrowthFactor;
    uint32 _minInlineStop;
    int32 _minBenefitThreshold;
    
    uint32 _inlineSmallMaxByteSize;
    int32 _inlineSmallBonus;
    
    uint32 _inlineMediumMaxByteSize;
    int32 _inlineMediumBonus;

    uint32 _inlineLargeMinByteSize;
    int32 _inlineLargePenalty;

    int32 _inlineLoopBonus;
    int32 _inlineLeafBonus;
    int32 _inlineSynchBonus;
    int32 _inlineRecursionPenalty;
    int32 _inlineExactArgBonus;
    int32 _inlineExactAllBonus;
    
    bool _inlineSkipExceptionPath;
    bool _inlineSkipApiMagicMethods;
    Method_Table* _inlineSkipMethodTable;
    Method_Table* _inlineBonusMethodTable;

    bool _usesOptimisticBalancedSync;
    bool isBCmapRequired;
    void* bc2HIRMapHandler;
    TranslatorAction* translatorAction;
    NamedType* inlinePragma;
    bool usePriorityQueue;
    const char* inlinerPipelineName;
    bool connectEarly;
};


} //namespace Jitrino 

#endif // _INLINER_H_
