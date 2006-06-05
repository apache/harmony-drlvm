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
 * @author Intel, Vyacheslav P. Shakin
 * @version $Revision: 1.10.12.1.4.4 $
 */


#include <stdlib.h>
#include "Ia32CodeGenerator.h"
#include "Ia32CodeSelector.h"
#include "Ia32CFG.h"
#include "Ia32InstCodeSelector.h"
namespace Jitrino
{
namespace Ia32{

Timer *		MethodCodeSelector::selectionTimer=NULL;
Timer *		MethodCodeSelector::blockMergingTimer=NULL;
Timer *		MethodCodeSelector::fixNodeInfoTimer=NULL;
Timer *		MethodCodeSelector::varTimer=NULL;

Timer *		CfgCodeSelector::instTimer=NULL;
Timer *		CfgCodeSelector::blockTimer=NULL;


//_______________________________________________________________________________________________________________
// FP conversion internal helpers (temp solution to be optimized)

//========================================================================================================
//                     class CfgCodeSelector
//========================================================================================================
//_______________________________________________________________________________________________
/**  Construct CFG builder */

CfgCodeSelector::CfgCodeSelector(CompilationInterface&		compIntfc,
											MethodCodeSelector& methodCodeSel,
											MemoryManager&			codeSelectorMM, 
											uint32					nNodes, 
											IRManager&			irM
										)
    : numNodes(nNodes), nextNodeId(0), compilationInterface(compIntfc), methodCodeSelector(methodCodeSel),
      irMemManager(irM.getMemoryManager()), 
      codeSelectorMemManager(codeSelectorMM),  irManager(irM),
      hasDispatchNodes(false), currBlock(NULL), returnOperand(0) 
{
    nextNodeId = 0;
    nodes = new (codeSelectorMemManager) Node*[numNodes];
    uint32 i;
    for (i = 0; i < numNodes; i++) 
        nodes[i] = NULL;

    if (compIntfc.isBCMapInfoRequired()) {
        MethodDesc* meth = compIntfc.getMethodToCompile();
        bc2HIRmapHandler = new(irMemManager) VectorHandler(bcOffset2HIRHandlerName, meth);
        bc2LIRmapHandler = new(irMemManager) VectorHandler(bcOffset2LIRHandlerName, meth);
   }
    InstCodeSelector::onCFGInit(irManager);
}

//_______________________________________________________________________________________________
/**  Create an exception handling (dispatching) node */

uint32 CfgCodeSelector::genDispatchNode(uint32 numInEdges,uint32 numOutEdges, double cnt) 
{
    assert(nextNodeId < numNodes);
    uint32 nodeId = nextNodeId++;
    nodes[nodeId] = irManager.newDispatchNode(cnt);
    hasDispatchNodes = true;
    return nodeId;
}

//_______________________________________________________________________________________________
/**  Create a basic block */

uint32 CfgCodeSelector::genBlock(uint32              numInEdges,
                                    uint32              numOutEdges,
                                    BlockKind           blockKind,
                                    BlockCodeSelector&  codeSelector,
                                    double              cnt) 
{
    assert(nextNodeId < numNodes);
    uint32 nodeId = nextNodeId++;
    BasicBlock *bb = irManager.newBasicBlock(cnt);
    nodes[nodeId] = bb;
    InstCodeSelector instCodeSelector(compilationInterface, *this, irManager, bb);
    currBlock = bb;
	{ 
//		PhaseTimer tm(instTimer, "ia32::selector::blocks::insts");
		codeSelector.genCode(instCodeSelector);
	}

	currBlock = NULL;
    //  Set prolog or epilog node
    switch (blockKind) {
    case Prolog:
        {
        //  Copy execution count into IA32 CFG prolog node and
        //  create an edge from IA32 CFG prolog node to optimizer's prolog node
		BasicBlock * prolog = irManager.getPrologNode();
        prolog->setExecCnt(cnt);
        irManager.newFallThroughEdge(prolog,bb,1.0);
        break;
        }
    case Epilog:
        {
        //irManager.newEdge(bb, irManager.getExitNode(), 1.0);
        assert(bb->isEmpty());
        break;
        }
    case InnerBlock:
        break;  // nothing to do
    }

    if (instCodeSelector.endsWithSwitch()) {
        // Generate an additional node that contains switch dispatch
        uint32      numTargets = instCodeSelector.getSwitchNumTargets(); 
        Opnd * switchSrc = instCodeSelector.getSwitchSrc();
        genSwitchBlock(bb, numTargets, switchSrc);
    }

    return nodeId;
}

//_______________________________________________________________________________________________
/**
	Create unwind node.
	This is a temporary node that exists only during code selection.
	We create it using code selector memory manager and insert it into its own CFG.
*/

uint32  CfgCodeSelector::genUnwindNode(uint32 numInEdges, 
                                          uint32 numOutEdges,
                                          double cnt) 
{
    assert(nextNodeId < numNodes);
    uint32 nodeId = nextNodeId++;
    nodes[nodeId] = irManager.newUnwindNode(cnt);
    return nodeId;
}

//_______________________________________________________________________________________________
/**  Create exit node */

uint32 CfgCodeSelector::genExitNode(uint32 numInEdges, double cnt) 
{
    assert(nextNodeId < numNodes);
    uint32 nodeId = nextNodeId++;
    nodes[nodeId] = irManager.getExitNode();
    return nodeId;
}

//_______________________________________________________________________________________________
/**  Create a block for a switch statement */

void CfgCodeSelector::genSwitchBlock(BasicBlock *originalBlock,
                                        uint32         numTargets, 
                                        Opnd *      switchSrc) 
{
    BasicBlock *bb = irManager.newBasicBlock(originalBlock->getExecCnt());
    InstCodeSelector instSelector(compilationInterface, *this, irManager, bb);
	{ 
		instSelector.genSwitchDispatch(numTargets,switchSrc);
	}
    // Create an edge from the original block to bb
    irManager.newFallThroughEdge(originalBlock,bb,1.0);
}

//_______________________________________________________________________________________________
/**  Create true edge (i.e., edge that corresponds to a taken conditional branch) */

void CfgCodeSelector::genTrueEdge(uint32 tailNodeId,uint32 headNodeId, double prob) 
{
	assert(nodes[tailNodeId]->hasKind(Node::Kind_BasicBlock));
    assert(nodes[headNodeId]->hasKind(Node::Kind_BasicBlock));
    BasicBlock * tailBlock = (BasicBlock *)nodes[tailNodeId];
    BasicBlock * headBlock = (BasicBlock *)nodes[headNodeId];
    irManager.newDirectBranchEdge(tailBlock, headBlock, prob);
}

//_______________________________________________________________________________________________
/**  Create false edge (i.e., edge that corresponds to a fallthrough after untaken conditional branch) */

void CfgCodeSelector::genFalseEdge(uint32 tailNodeId,uint32 headNodeId, double prob) 
{
    assert(nodes[tailNodeId]->hasKind(Node::Kind_BasicBlock));
    assert(nodes[headNodeId]->hasKind(Node::Kind_BasicBlock));
    irManager.newFallThroughEdge((BasicBlock*)nodes[tailNodeId], (BasicBlock*)nodes[headNodeId], prob);
}

//_______________________________________________________________________________________________
/**  Create unconditional edge (i.e., edge that corresponds to fallthrough) */

void CfgCodeSelector::genUnconditionalEdge(uint32 tailNodeId,uint32 headNodeId, double prob) 
{
    Node * tailNode = nodes[tailNodeId];
    Node * headNode = nodes[headNodeId];
    assert(tailNode->hasKind(Node::Kind_BasicBlock));
    assert(headNode->hasKind(Node::Kind_BasicBlock) || headNode == irManager.getExitNode());
	if (headNode == irManager.getExitNode()){
		irManager.newEdge(tailNode, headNode, 1.0);
	}else{
		irManager.newFallThroughEdge((BasicBlock*)tailNode,(BasicBlock*)headNode, prob);
	}
}

//_______________________________________________________________________________________________
/**  Create switch edges */

void CfgCodeSelector::genSwitchEdges(uint32 tailNodeId, uint32 numTargets, 
                                        uint32 *targets, double *probs, 
                                        uint32 defaultTarget) 
{
	// 
	//  Switch structure:
    //                              
    //      origBlock                                       switchBlock
    //     ===========                  Fallthrough        =============          
    //        ....                       =======>            .......
    //      if (switchVar >= numTargets)                    swTarget= jmp [switchVar + swTableBase]  
    //         jmp defaultTarget                            
    //  
    Node * origBlock = nodes[tailNodeId];
	const Edges& outEdges=origBlock->getEdges(Direction_Out);
    assert(outEdges.getCount() == 1);
    Node * switchNode = outEdges.getFirst()->getNode(Direction_Head);
	assert(switchNode->hasKind(Node::Kind_BasicBlock));
    BasicBlock * switchBlock = (BasicBlock *)switchNode;
	assert(switchBlock->getInsts().getLast()->hasKind(Inst::Kind_SwitchInst));
    SwitchInst * swInst = (SwitchInst *)switchBlock->getInsts().getLast();

	double    defaultEdgeProb = 1.0;
    defaultEdgeProb = 1.0;
    for (uint32 i = 0; i < numTargets; i++) {
        uint32 targetId = targets[i];
        if ( targetId == defaultTarget) {
            defaultEdgeProb = probs[i];
            break;
        }
        if (std::find(targets, targets+i, targetId)!=targets+i) {
            continue; //repeated target
        }
        if (probs[i] == EdgeProbValue_Unknown) {
            defaultEdgeProb = EdgeProbValue_Unknown;
            break;
        } 
        defaultEdgeProb -= 1.0/(numTargets+1);
    }

	genTrueEdge(tailNodeId, defaultTarget, defaultEdgeProb);
    //  Fix probability of fallthrough edge
    if (defaultEdgeProb!=EdgeProbValue_Unknown) {
        origBlock->getEdges(Direction_Out).getFirst()->setProbability(1.0 - defaultEdgeProb);
    }
    //  Generate edges from switchBlock to switch targets
    for (uint32 i = 0; i < numTargets; i++) {
        Node * targetNode = nodes[targets[i]];
        // Avoid generating duplicate edges. Jump table however needs all entries
        if (! switchBlock->isConnectedTo(Direction_Out, targetNode)) 
            irManager.newEdge(switchBlock, targetNode, probs[i]);
        assert(targetNode->hasKind(Node::Kind_BasicBlock));
        swInst->setTarget(i, (BasicBlock *)targetNode);
	}
}

//_______________________________________________________________________________________________
/**  Create an edge to the exception dispatch node or unwind node  */

void CfgCodeSelector::genExceptionEdge(uint32 tailNodeId, uint32 headNodeId, double prob) 
{
    Node * headNode = nodes[headNodeId];
    Node * tailNode = nodes[tailNodeId];
	assert(headNode->hasKind(Node::Kind_DispatchNode) || headNode->hasKind(Node::Kind_UnwindNode) ); 
    irManager.newEdge(tailNode, headNode, prob);
}

//_______________________________________________________________________________________________
/**  Create catch edge */

void CfgCodeSelector::genCatchEdge(uint32 tailNodeId, 
                                      uint32 headNodeId,
                                      uint32 priority,
                                      Type*  exceptionType, 
                                      double prob) 
{
    Node * headNode = nodes[headNodeId];
    Node * tailNode = nodes[tailNodeId];
    assert(tailNode->hasKind(Node::Kind_DispatchNode));
    assert(headNode->hasKind(Node::Kind_BasicBlock));
    irManager.newCatchEdge((DispatchNode*)tailNode, (BasicBlock*)headNode, exceptionType, priority, prob);
}

//_______________________________________________________________________________________________
/**  Create an edge to the exit node */

void CfgCodeSelector::genExitEdge(uint32 tailNodeId,
                                     uint32 headNodeId,
                                     double prob) 
{
    Node * headNode = nodes[headNodeId];
    Node * tailNode = nodes[tailNodeId];
    assert(headNode == irManager.getExitNode());
    if (tailNode!=irManager.getUnwindNode()) { // unwind->exit edge is auto-generated 
        irManager.newEdge(tailNode, headNode, prob);
    }
}

//_______________________________________________________________________________________________
/**  Set node loop info */

void CfgCodeSelector::setLoopInfo(uint32 nodeId, 
                                     bool   isLoopHeader, 
                                     bool   hasContainingLoopHeader, 
                                     uint32 headerId) 
{
    Node * node = nodes[nodeId];
    Node * header = hasContainingLoopHeader ? nodes[headerId] : NULL;
    node->setLoopInfo(isLoopHeader,header);
}

//_______________________________________________________________________________________________
/**  Cfg code selector is notified that method contains calls */
void CfgCodeSelector::methodHasCalls(bool nonExceptionCall) 
{
    irManager.setHasCalls();
    if (nonExceptionCall)
        irManager.setHasNonExceptionCalls();
}


///////////////////////////////////////////////////////////////////////////////////
//
//                     class VarGenerator
//
///////////////////////////////////////////////////////////////////////////////////

//_______________________________________________________________________________________________
uint32 VarGenerator::defVar(Type* varType, bool isAddressTaken, bool isPinned) 
{
	Opnd * opnd=irManager.newOpnd(varType);
	return opnd->getId(); 
}

//_______________________________________________________________________________________________
void VarGenerator::setManagedPointerBase(uint32 managedPtrVarNum, uint32 baseVarNum) 
{
}


///////////////////////////////////////////////////////////////////////////////////
//
//                     class MethodCodeSelector
//
///////////////////////////////////////////////////////////////////////////////////

//_______________________________________________________________________________________________
/**  Generate variable operands */

void MethodCodeSelector::genVars(uint32           numVars, VarCodeSelector& varCodeSelector) 
{
    numVarOpnds = numVars;
    VarGenerator varCodeSelectorCallback(irManager,*this);
    varCodeSelector.genCode(varCodeSelectorCallback);
}

//_______________________________________________________________________________________________
/** Update register usage */

void MethodCodeSelector::updateRegUsage() 
{
}

//_______________________________________________________________________________________________
/** Set loop info, persistent ids, and others for nodes that exist only in the code generator CFG */

void CfgCodeSelector::fixNodeInfo() 
{
// connect throw nodes added during inst code selection to corresponding dispatch or unwind nodes
	for (CFG::NodeIterator it(irManager); it!=NULL; ++it){
		Node * node=it;
		if (node->hasKind(Node::Kind_BasicBlock)){
			BasicBlock * bb=(BasicBlock*)node;

			const Insts & insts = bb->getInsts();
			Inst * lastInst = insts.getLast();
			if (lastInst) {
				Inst * prevInst = insts.getPrev(lastInst);
				if(prevInst && prevInst->getKind() == Inst::Kind_BranchInst) {
					Edge * ftEdge = bb->getFallThroughEdge();
					Edge * dbEdge = bb->getDirectBranchEdge();
					if (ftEdge && dbEdge) {
						BasicBlock * newBB = irManager.newBasicBlock(0);
						BasicBlock * nextFT = (BasicBlock *)ftEdge->getNode(Direction_Head);
						irManager.removeEdge(ftEdge);
						irManager.newFallThroughEdge(newBB, nextFT, 0.001);
						newBB->appendInsts(irManager.newBranchInst(lastInst->getMnemonic()));
						bb->removeInst(lastInst);
						BasicBlock * nextDB = (BasicBlock *)dbEdge->getNode(Direction_Head);
						irManager.removeEdge(dbEdge);
						irManager.newDirectBranchEdge(bb, nextDB, 0.001);
						irManager.newDirectBranchEdge(newBB, nextDB, 0.001);
						irManager.newFallThroughEdge(bb, newBB, 0.001);
					} else {
						assert(0);
					}
				}
			}
			if (bb->getEdges(Direction_Out).getCount()==0){ // throw node
				assert(bb->getEdges(Direction_In).getCount()==1);
				BasicBlock * bbIn=(BasicBlock*)bb->getNode(Direction_In, Node::Kind_BasicBlock);
				assert(bbIn!=NULL);
				Node * target=bbIn->getNode(Direction_Out, (Node::Kind)(Node::Kind_DispatchNode|Node::Kind_UnwindNode));
				assert(target!=NULL);
				irManager.newEdge(bb, target, 1.0);
			}
		}
	}
	irManager.updateLoopInfo();
}

//_______________________________________________________________________________________________
/**  Generate heap base initialization */

void MethodCodeSelector::genHeapBase() 
{
}

//_______________________________________________________________________________________________
/** Generate control flow graph */

void MethodCodeSelector::genCFG(uint32 numNodes, CFGCodeSelector& codeSelector, 
                                   bool useEdgeProfile) 
{
	irManager.setHasEdgeProfile(useEdgeProfile);

    CfgCodeSelector cfgCodeSelector(compilationInterface, *this,
						codeSelectorMemManager,numNodes,
						irManager);
	{ 
		PhaseTimer tm(selectionTimer, "ia32::selector::selection"); 
		if( NULL == irManager.getEntryPointInst() ) {
			irManager.newEntryPointPseudoInst( irManager.getDefaultManagedCallingConvention() );
		}
		codeSelector.genCode(cfgCodeSelector);
	}
	{
		PhaseTimer tm(fixNodeInfoTimer, "ia32::selector::fixNodeInfo"); 
		irManager.expandSystemExceptions(0);
		cfgCodeSelector.fixNodeInfo();
	}
	{
		PhaseTimer tm(blockMergingTimer, "ia32::selector::blockMerging"); 
		irManager.mergeSequentialBlocks();
		irManager.purgeEmptyBlocks();
	}
}

//_______________________________________________________________________________________________



}; // namespace Ia32
};
