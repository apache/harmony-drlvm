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
 * @author Vyacheslav P. Shakin
 * @version $Revision$
 */

#ifndef _IA32_CODE_SELECTOR_H_
#define _IA32_CODE_SELECTOR_H_

#include "Stl.h"
#include "CodeGenIntfc.h"
#include "Ia32IRManager.h"
#include "Ia32CFG.h"

namespace Jitrino
{
class CompilationInterface;
class EdgeMethodProfile;

namespace Ia32{


//========================================================================================================
//  Forward declarations
//========================================================================================================

class Inst;


//========================================================================================================
//  Variable or value type operand allocated on the stack
//========================================================================================================

#define NO_STACK7_LOC 0xFFFFFFFF

class MethodCodeSelector;

class CodeSelectionFlags {
public:
    CodeSelectionFlags() {
        useInternalHelpersForInteger2FloatConv = false;
        slowLdString = false;
    }

    bool useInternalHelpersForInteger2FloatConv;
    bool slowLdString; 

};



//========================================================================================================
//  Cfg code selector -- builds IA32 flow graph with instructions
//========================================================================================================
/**
class CfgCodeSelector is the Ia32 CG implementation of the 
::CFGCodeSelector::Callback interface 

It is responsible for creation of the LIR control flow graph 
*/
class CfgCodeSelector : public ::Jitrino::CFGCodeSelector::Callback {
public:
    //
    //  CFGCodeSelector::Callback methods
    //
    CfgCodeSelector(::Jitrino::SessionAction* sa, CompilationInterface& compIntfc,
                         MethodCodeSelector& methodCodeSel, 
                         MemoryManager& codeSelectorMM, uint32 nNodes, 
                         IRManager& irM);

    uint32  genDispatchNode(uint32 numInEdges, uint32 numOutEdges, const StlVector<MethodDesc*>& inlineEndMarkers, double cnt);
    uint32  genBlock(uint32 numInEdges, uint32 numOutEdges, BlockKind blockKind,
                     BlockCodeSelector& codeSelector, double cnt);
    uint32  genUnwindNode(uint32 numInEdges, uint32 numOutEdges,double cnt);
    uint32  genExitNode(uint32 numInEdges, double cnt);
    void    genUnconditionalEdge(uint32 tailNodeId,uint32 headNodeId, double prob);
    void    genTrueEdge(uint32 tailNodeId,uint32 headNodeId, double prob);
    void    genTrueEdge(Node* tailNode, Node* headNode, double prob);
    void    genFalseEdge(uint32 tailNodeId,uint32 headNodeId, double prob);
    void    genFalseEdge(Node* tailNode, Node* headNode, double prob);
    void    genSwitchEdges(uint32 tailNodeId, uint32 numTargets, uint32 *targets, 
                           double *probs, uint32 defaultTarget);
    void    genExceptionEdge(uint32 tailNodeId, uint32 headNodeId, double prob);
    void    genCatchEdge(uint32 headNodeId,uint32 tailNodeId,
                         uint32 priority,Type* exceptionType, double prob);
    
    void    setPersistentId(uint32 nodeId, uint32 persistentId) {
        ((CGNode*)nodes[nodeId])->setPersistentId(persistentId);
    }

    Opnd * getMethodReturnOpnd(){
        return returnOperand;
    }

    void setMethodReturnOpnd(Opnd * retOp){
        returnOperand=retOp;
    }
    
    void    fixNodeInfo();

    //
    // Used to differenciate between instrumented and uninstrumented cfgs
    //
    void markAsInstrumented() {
    }

    //    Callbacks for the instruction code selector
    void    methodHasCalls(bool nonExceptionCall);

    const CodeSelectionFlags& getFlags() const {return flags;}
private:
    //
    //    Methods
    //
    void    genSwitchBlock(Node *originalBlock, uint32 numTargets, 
                           Opnd *switchSrc);
    void    genEpilogNode();
    Inst *  findExceptionInst(Node* block);
    Node* getCurrentBlock() {return currBlock;}
    //
    //  Fields
    //
    Node**                  nodes;
    uint32                  numNodes;
    uint32                  nextNodeId;
    CompilationInterface&   compilationInterface;
    MethodCodeSelector&     methodCodeSelector;
    MemoryManager&          irMemManager;           // for data live after code selection
    MemoryManager&          codeSelectorMemManager; // for data dead after code selection
    IRManager&              irManager;
    bool                    hasDispatchNodes;
    Node*                   currBlock;

    Opnd *                  returnOperand;

    CodeSelectionFlags      flags;

    friend class InstCodeSelector;
};

//========================================================================================================
//  Method code selector -- selects IA32 instructions for a method
//========================================================================================================
/**
class MethodCodeSelector is the Ia32 CG implementation of the 
::MethodCodeSelector::Callback interface 

This is the top-level agent in HIR->LIR lowering hierarchy and 
drives IR lowering for a whole method
*/

class MethodCodeSelector : public ::Jitrino::MethodCodeSelector::Callback {
public:
    MethodCodeSelector(::Jitrino::SessionAction* sa, 
                            CompilationInterface&    compIntfc,
                            MemoryManager&      irMM,
                            MemoryManager&      codeSelectorMM,
                            IRManager&          irM);
     

      void                genVars(uint32 numVars, ::Jitrino::VarCodeSelector& varCodeSelector);
    void                setMethodDesc(MethodDesc * desc) {methodDesc = desc;}
    void                genCFG(uint32 numNodes, ::Jitrino::CFGCodeSelector& codeSelector, bool useDynamicProfile);
    
    MethodDesc *        getMethodDesc() {return methodDesc;}
private:
    //
    //    Methods
    //
    void                updateRegUsage();
    void                genHeapBase();
    //
    //    Fields
    //
    ::Jitrino::SessionAction* sa;
    int                 numVarOpnds;
    CompilationInterface& compilationInterface;
    MemoryManager&      irMemManager;           // for data live after code selection
    MemoryManager&      codeSelectorMemManager; // for data dead after code selection 
    IRManager&          irManager;
    
    MethodDesc *        methodDesc;

    EdgeMethodProfile*  edgeProfile;

    friend class CFGCodeSelector;
    friend class VarGenerator;
    friend class InstCodeSelector;
};

//========================================================================================================
//  class VarGenerator: Generator of variable operands
//========================================================================================================
/**
class VarGenerator is the Ia32 CG implementation of the 
::VarCodeSelector::Callback interface 

It is responsible for conversion of operands with multiple definitions 
(Variables in the HIR terminology)
*/


class VarGenerator : public VarCodeSelector::Callback {
public:
    VarGenerator(IRManager& irM, MethodCodeSelector& methodCodeSel) 
     : nextVarId(0), irManager(irM), methodCodeSelector(methodCodeSel) {
    }
    uint32    defVar(Type* varType, bool isAddressTaken, bool isPinned);
    void      setManagedPointerBase(uint32 managedPtrVarNum, uint32 baseVarNum);
private:
    uint32                  nextVarId;
    IRManager&              irManager;
    MethodCodeSelector&     methodCodeSelector;
};




}; //namespace Ia32
}

#endif // _IA32_CODE_SELECTOR_H_
