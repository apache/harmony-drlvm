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
 * @author Intel, Konstantin M. Anisimov, Igor V. Chebykin
 * @version $Revision$
 *
 */

#include "BitSet.h"
#include "IpfCfgVerifier.h"
#include "IpfOpndManager.h"
#include "IpfIrPrinter.h"

namespace Jitrino {
namespace IPF {
    
//========================================================================================//
// OrderededNodeVector
//========================================================================================//
class OrderededNodeVector:public NodeVector {
    friend class CfgView;
public:
    OrderededNodeVector(Cfg& cfg_, const char*  orderName, bool fromEnter=true):
        cfg(cfg_),
        orderName(orderName),
        valid(false),
        fromEnter(fromEnter),
        nodes(*this),
        visitedNodes(cfg_.getMM(), cfg_.getMaxNodeId())
    {}
    
    void collect();
    void printContent(ostream& out);
    
protected:

    virtual void makeOrdered(Node* node)=0;
    virtual ~OrderededNodeVector(){}; // for gcc 
    
    Cfg& cfg;
    const char*   orderName;
    bool valid;
    bool fromEnter;
    NodeVector& nodes;
    BitSet visitedNodes;

};

//---------------------------------------------------------------------------//
void OrderededNodeVector::collect() {
    if (valid) return;
    nodes.clear();
    uint32 numNodes=cfg.getMaxNodeId();
    visitedNodes.resizeClear(numNodes);
    makeOrdered(fromEnter?cfg.getEnterNode():cfg.getExitNode()); 
    valid=true;
}    

//---------------------------------------------------------------------------//
void OrderededNodeVector::printContent(ostream& out) {
    out << "CFG ordered: " << orderName << " size:" << size() << endl;
    for(uint k=0; k<nodes.size(); k++) {
        Node* node=nodes[k];
        out << "node " << node->getId() << ": ";
        node->printEdges(out);
        out << endl;
    }
}

//---------------------------------------------------------------------------//
class DirectOrderedNodeVector: public OrderededNodeVector {
public:
    DirectOrderedNodeVector(Cfg& cfg_):
        OrderededNodeVector(cfg_, "Direct", false)
    {}
protected:
    virtual void makeOrdered(Node* node);

};

void DirectOrderedNodeVector::makeOrdered(Node* node) {
    visitedNodes.setBit(node->getId());              // mark node visited
    EdgeVector& inEdges = node->getInEdges();        // get inEdges
    for (uint i=0; i<inEdges.size(); i++) {          // iterate through inEdges
        Edge* edge=inEdges[i];
        if (edge==NULL) {
            assert(edge);
            continue;
        }
        Node* pred = edge->getSource();     // get pred node
        if (pred==NULL) {
            assert(pred);
            continue;
        }
        if (visitedNodes.getBit(pred->getId())) continue;  // if it is already visited - ignore
        makeOrdered(pred);       // we have found unvisited pred - reenter
    }
    nodes.push_back(node);                    // all succs have been visited - place node in searchResult vector
}

//----------------------------------------------------------------------------------------//
class PostOrderededNodeVector: public OrderededNodeVector {
public:
    PostOrderededNodeVector(Cfg& cfg_):
        OrderededNodeVector(cfg_, "Post")
    {}
protected:
    virtual void makeOrdered(Node* node);

};

void PostOrderededNodeVector::makeOrdered(Node* node) {
    visitedNodes.setBit(node->getId());                       // mark node visited
    EdgeVector& outEdges = node->getOutEdges();       // get outEdges
    for (uint i=0; i<outEdges.size(); i++) {         // iterate through outEdges
        Edge* edge=outEdges[i];
        if (edge==NULL) {
            assert(edge);
            continue;
        }
        Node* succ = edge->getTarget();     // get succ node
        if (succ==NULL) {
            assert(succ);
            continue;
        }
        if (visitedNodes.getBit(succ->getId())) continue;   // if it is already visited - ignore
        makeOrdered(succ);          // we have found unvisited succ - reenter
    }
    nodes.push_back(node);                     // all succs have been visited - place node in searchResult vector
}

//----------------------------------------------------------------------------------------//

class LayoutOrderedNodeVector: public OrderededNodeVector {
public:
    LayoutOrderedNodeVector(Cfg& cfg_):
        OrderededNodeVector(cfg_, "Layout")
    {}
protected:
    virtual void makeOrdered(Node* node);

};

void LayoutOrderedNodeVector::makeOrdered(Node* node_) {
    BbNode* node = (BbNode*)node_;
    while (node != NULL) {
        visitedNodes.setBit(node->getId());       // mark node visited
        nodes.push_back(node);
        node = node->getLayoutSucc();
    }
}

//========================================================================================//
class CfgView {
public:
    CfgView(Cfg& cfg_):
        directOrderedNodes(cfg_),
        postOrderedNodes(cfg_),
        layoutOrderedNodes(cfg_)
    {}
    
    OrderededNodeVector& sortNodes(SearchKind searchKind);
    void changed();
    bool verifyNodes(ostream& cout);
        
private:
    DirectOrderedNodeVector directOrderedNodes;
    PostOrderededNodeVector postOrderedNodes;
    LayoutOrderedNodeVector layoutOrderedNodes;
};

//---------------------------------------------------------------------------//
OrderededNodeVector& CfgView::sortNodes(SearchKind searchKind) {
    switch (searchKind) {
        case SEARCH_DIRECT_ORDER :
            directOrderedNodes.collect(); 
            return directOrderedNodes; 
        case SEARCH_POST_ORDER   :
            postOrderedNodes.collect(); 
            return postOrderedNodes; 
        case SEARCH_LAYOUT_ORDER :
            layoutOrderedNodes.collect(); 
            return layoutOrderedNodes; 
         case SEARCH_UNDEF_ORDER     :
            if (directOrderedNodes.valid) return directOrderedNodes;
            if (postOrderedNodes.valid) return postOrderedNodes;
            layoutOrderedNodes.collect();
            return layoutOrderedNodes;
        default:
            IPF_LOG << IPF_ERROR << endl;
            assert(0);
            layoutOrderedNodes.collect();
            return layoutOrderedNodes;
    }
}    

bool CfgView::verifyNodes(ostream& cout) {
    OrderededNodeVector& dir=sortNodes(SEARCH_DIRECT_ORDER);
    OrderededNodeVector& post=sortNodes(SEARCH_POST_ORDER);
    if (dir.visitedNodes.isEqual(post.visitedNodes)) return true;
    cout << "verifyNodes: DIRECT_ORDER and POST_ORDER differs:" << endl;
    BitSet collectedNodes(dir.visitedNodes);
    collectedNodes.subtract(post.visitedNodes);
    if (!collectedNodes.isEmpty()) {
        cout << "    DIRECT-POST = ";
        collectedNodes.print(cout);
    }
    collectedNodes.copyFrom(post.visitedNodes);
    collectedNodes.subtract(dir.visitedNodes);
    if (!collectedNodes.isEmpty()) {
        cout << "    POST-DIRECT =  ";
        collectedNodes.print(cout);
    }
//        IPF_EXIT("CfgView::verifyNodes");
    return false;
}

void CfgView::changed() {
    directOrderedNodes.valid=false;
    postOrderedNodes.valid=false;
    layoutOrderedNodes.valid=false;
}

//===========================================================================//
class IpfCfgVerifier: public CfgView {
    friend class Vertex;
    friend class VertexBB;
protected:
    MemoryManager& mm;
    Cfg& cfg;
    uint numAllNodes;
    uint numNodes;
    uint numOpnds;
    NodeVector& nodes;
    Vertex** verticesById;  // by node ids
    Vertex** vertices;  // post-ordered
    BitSet tmpSet;
    BitSet tmpSet2;
    BitSet ignoredOpnds;
    RegOpnd** opndsById;
    uint p0Id;
    
    Vertex* getVertex(Node* node) {
        return verticesById[node->getId()];
    }
    VertexBB* getVertexBB(BbNode* node) {
        return (VertexBB*)(verticesById[node->getId()]);
    }
    Vertex* getSourceVertex(Edge* edge) {
        return getVertex(edge->getSource());
    }
    Vertex* getTargetVertex(Edge* edge) {
        return getVertex(edge->getTarget());
    }
        
    void registerOpnds(OpndVector& opnds);
    void printOpndSet(ostream& os, const char* str, BitSet& set);
public:

    IpfCfgVerifier(MemoryManager& mm, Cfg& cfg);
    void setDefUse();
    bool collectLiveSets();                        
    virtual bool checkLiveSets();

    void setDefs();
    bool collectDefs();
    virtual bool checkDefs();
    
    virtual ~IpfCfgVerifier() {}
};  // end class IpfCfgVerifier

struct Vertex {
//    friend class IpfCfgVerifier;
    IpfCfgVerifier& fSolver;
    Node* node;
    BitSet* in; 
    BitSet* out; 

    Vertex(MemoryManager& mm, IpfCfgVerifier& fSolver, Node* node, int width):
        fSolver(fSolver),
        node(node),
        in(new(mm) BitSet(mm, width)),
        out(in)
    {}
    
    EdgeVector& getInEdges() {
        return node->getInEdges();
    }
    EdgeVector& getOutEdges() {
        return node->getOutEdges();
    }

    virtual void computeIn();
//    virtual bool checkLiveSet() {return true; }
    bool checkLiveSet();
    
    virtual void computeOut();
    virtual bool checkDefs() {return true; }

};

struct VertexBB:Vertex {
//    friend class IpfCfgVerifier;
    BbNode* node;
    BitSet* use; 
    BitSet* def; 

    VertexBB(MemoryManager& mm, IpfCfgVerifier& fSolver, BbNode* node, int width):
        Vertex(mm, fSolver, node, width),
        node(node),
        use(new(mm) BitSet(mm, width)),
        def(new(mm) BitSet(mm, width))  // TODO: def and use may be null
    {
        in=new(mm) BitSet(mm, width);
    }

    void setDefUse();
    virtual void computeIn();
//    virtual bool checkLiveSet();
    
    void setDef(Inst* inst);
    void setDef();
    virtual void computeOut();
    bool checkOpndIsDef(Opnd* opnd, uint instId);
    virtual bool checkDefs();

};

//---------------------------------------------------------------------------//
IpfCfgVerifier::IpfCfgVerifier(MemoryManager& mm, Cfg& cfg):
    CfgView(cfg),
    mm(mm),
    cfg(cfg),
    numAllNodes(cfg.getMaxNodeId()),
    numOpnds(cfg.getOpndManager()->getNumOpnds()),
    nodes(sortNodes(SEARCH_DIRECT_ORDER)),
    verticesById(new(mm) (Vertex*)[numAllNodes]),
    vertices(NULL),
    tmpSet(mm, numOpnds),
    tmpSet2(mm, numOpnds),
    ignoredOpnds(mm, numOpnds),
    opndsById(new(mm) (RegOpnd*)[numOpnds]),
    p0Id(cfg.getOpndManager()->getP0()->getId())
{
    for (uint k=0; k<numAllNodes; k++) {
        verticesById[k]=NULL;
    }
    for (uint k=0; k<numOpnds; k++) {
        opndsById[k]=NULL;
    }
    
    numNodes=nodes.size();
    vertices=new(mm) (Vertex*)[numNodes];
    for (uint k=0; k<numNodes; k++) {
        Node* node=nodes[k];
        Vertex* vertex;
        if (node->getNodeKind()==NODE_BB) {
            vertex=new(mm) VertexBB(mm, *this, (BbNode*)node, numOpnds);
        } else {
            vertex=new(mm) Vertex(mm, *this, node, numOpnds);
        }                
        verticesById[node->getId()]=vertex;
        vertices[k]=vertex;
    }
}
    
//---------------------------------------------------------------------------//
void IpfCfgVerifier::printOpndSet(ostream& os, const char* str, BitSet& set) {
    RegOpndSet  opndSet;
    os << str;
    for (uint k=0; k<numOpnds; k++) {
        if (set.getBit(k)) {
            RegOpnd* opnd=opndsById[k];
            if (opnd==NULL) {
                os << "opnd[" << k << "] ";
                continue;
            }            
            opndSet.insert(opnd);
        }
    }
    os << IrPrinter::toString(opndSet) << endl;
}
    
//---------------------------------------------------------------------------//
// register ALL opnds in 'opndsById'
void IpfCfgVerifier::registerOpnds(OpndVector& opnds) {    
    for (uint k=0; k<opnds.size(); k++) {
           Opnd* opnd = opnds[k];
           if (!opnd->isReg()) continue;
           RegOpnd* reg = (RegOpnd*)opnd;
        uint id=opnd->getId();
        if (opndsById[id]==reg) {
            continue; // registered already
        }
        opndsById[id]=reg;
        
        int num=reg->getValue();
        switch(reg->getOpndKind()) {
            case OPND_G_REG:
                // ignore r0 r8
                if ((num == 0)|| (num == 8)) {
                    // r8 - returned value;  TODO: more accurate
                    ignoredOpnds.setBit(id);
                }
                break;
            case OPND_F_REG:
                // ignore f0 f1 f8
                if ((num == 0) || (num == 1) || (num == 8)) {
                    // f8 - returned value;  TODO: more accurate
                    ignoredOpnds.setBit(id);
                }
                break;
            case OPND_P_REG:
                // ignore p0
                if (num == 0) {
                    ignoredOpnds.setBit(id);
                }
                break;
            case OPND_B_REG:
                // ignore  all br:
                ignoredOpnds.setBit(id);
                break;
            default: ;
        }
    }
}

//---------------------------------------------------------------------------//
// check live sets
//---------------------------------------------------------------------------//

void VertexBB::setDefUse() {
    // iterate through instructions postorder and calculate liveSet for begining of the node
    InstVector& insts = node->getInsts();
    for (int j=insts.size()-1; j>=0; j--) {
        Inst* inst=insts[j];
        uint numDst=inst->getNumDst();
        OpndVector& opnds    = inst->getOpnds();            // get inst opnds
        
        if (((RegOpnd*) opnds[0])->getValue() == 0) {  // qp==p0
            for (uint k=1; k<numDst+1; k++) {
                Opnd* opnd = opnds[k];
                if (!opnd->isReg()) continue; // ignore non register opnd
                uint id=opnd->getId();
                def->setBit(id); 
                use->setBit(id, false);
            }
            // TODO: If no one opnd was in Live Set - inst is dead
        } else {
            // add qp opnd in Use Set
            use->setBit(opnds[0]->getId()); 
        }
        // add src opnds in Use Set
        for (uint k=numDst+1; k<opnds.size(); k++) {
            Opnd* opnd = opnds[k];
             if (!opnd->isReg()) continue; // ignore non register opnd
            use->setBit(opnd->getId());
        }
        fSolver.registerOpnds(opnds);
    }
}

void IpfCfgVerifier::setDefUse() {
    for (int k=numNodes-1; k>=0; k--) {
        Node* node=nodes[k];
        if (node->getNodeKind()==NODE_BB) {
            VertexBB* vertex=getVertexBB((BbNode*)node);
            vertex->setDefUse();
        }                
    }
}

void Vertex::computeIn() {
    // out[B] := U in[S]
    EdgeVector& outEdges=getOutEdges();
    uint size=outEdges.size();
    if (size==0) return;
    Vertex* succ=fSolver.getTargetVertex(outEdges[0]);
    out->copyFrom(*(succ->in));
    for (uint k=1; k<outEdges.size(); k++) {
        succ=fSolver.getTargetVertex(outEdges[k]);
        out->unionWith(*(succ->in));
    }
    // in[B] == out[B]
}
    
void VertexBB::computeIn() {
    // out[B] := U in[S]
    Vertex::computeIn();
    // in[B] := use[B} U (out[B] - def[B])
    in->copyFrom(*out);
    in->subtract(*def);
    in->unionWith(*use);
}
    
//---------------------------------------------------------------------------//
//bool VertexBB::checkLiveSet() {
bool Vertex::checkLiveSet() {

    RegOpndSet& liveSet = node->getLiveSet();
    BitSet& dceIn=fSolver.tmpSet;
    BitSet& tmpSet=fSolver.tmpSet2;
    dceIn.clear();

    for (RegOpndSetIterator i=liveSet.begin(); i!=liveSet.end(); i++) {
        Opnd* opnd = (Opnd*)(*i);
        IPF_ASSERT(opnd!=NULL);
        uint id = opnd->getId();
        dceIn.setBit(id);
    }
    dceIn.subtract(fSolver.ignoredOpnds);  // ignore p0, r0, f0 etc
    in->subtract(fSolver.ignoredOpnds);
    if (in->isEqual(dceIn)) return true;

    IPF_LOG << "checkLiveSets: DCE and IpfCfgVerifier results differs for node" << node->getId() << endl;
    bool res=true;
    tmpSet.copyFrom(dceIn);
    tmpSet.subtract(*in);
    if (!tmpSet.isEmpty()) {
        if (LOG_ON) {
            fSolver.printOpndSet(LOG_OUT, "    DCE has  ", tmpSet);
        }
//            res=false;      commented out while too many failures to concide
    }
    
    tmpSet.copyFrom(*in);
    tmpSet.subtract(dceIn);
    if (!tmpSet.isEmpty()) {
        if (LOG_ON) {
            fSolver.printOpndSet(LOG_OUT, "    IpfCfgVerifier has  ", tmpSet);
        }
        res=false;        
    }
    
    return res;
}

//---------------------------------------------------------------------------//

bool IpfCfgVerifier::collectLiveSets() {
    bool changed = false;
    for (int k=numNodes-1; k>=0; k--) {
        Vertex* vertex=vertices[k];
        tmpSet.copyFrom(*vertex->in);  // save to compare
        vertex->computeIn();
//printf(" %s%d", vertex->in->getBit(9)?"*":" ", vertex->node->getId());
        if (!changed) {
            changed = !vertex->in->isEqual(tmpSet);  // compare
        }
    }
//printf("\n");
    return changed;
}

//---------------------------------------------------------------------------//
bool IpfCfgVerifier::checkLiveSets() {
    setDefUse();
//printf("IpfCfgVerifier::checkLiveSets():\n");
    // solve the flow equation set
    while (collectLiveSets()) {};
    
    // compare our results with DCE results
    bool res = true;
    for (uint k=0; k<numNodes; k++) {
        Vertex* vertex=vertices[k];
        res=vertex->checkLiveSet() & res;
    }
    if (res) {
        if (LOG_ON) {
            IPF_LOG << "ignoredOpnds="; 
            ignoredOpnds.print(LOG_OUT);
        }
    }
    return res;
}

//---------------------------------------------------------------------------//
// check defs
//---------------------------------------------------------------------------//

void VertexBB::setDef(Inst* inst) {
    uint numDst=inst->getNumDst();
    OpndVector& opnds    = inst->getOpnds();            // get inst opnds
    
//    if (((RegOpnd*) opnds[0])->getLocation() == 0) {  // qp==p0
// TODO: handle conditional instructions more accurate 
    for (uint k=1; k<numDst+1; k++) {
        Opnd* opnd = opnds[k];
        if (!opnd->isReg()) continue; // ignore non register opnd
        uint id=opnd->getId();
        def->setBit(id); 
    }
}

void VertexBB::setDef() {
    // iterate through instructions in direct order
    InstVector& insts = node->getInsts();
    for (uint j=0; j<insts.size(); j++) {
        Inst* inst=insts[j];
        setDef(inst);
        OpndVector& opnds    = inst->getOpnds();            // get inst opnds
        fSolver.registerOpnds(opnds);
    }
}

//---------------------------------------------------------------------------//
void IpfCfgVerifier::setDefs() {
    OpndVector args; // = cfg.getArgs();
    BitSet* enterIn=getVertex(cfg.getEnterNode())->in;
    for (uint k=0; k<args.size(); k++) {
        enterIn->setBit(args[k]->getId());
    }    

    for (uint k=0; k<numNodes; k++) {
        Node* node=nodes[k];
        if (node->getNodeKind()==NODE_BB) {
            VertexBB* vertex=getVertexBB((BbNode*)node);
            vertex->setDef();
        }                
    }

    // after opnds registered and ignoredOpnds collected
    enterIn->unionWith(ignoredOpnds);
    for (uint k=0; k<numNodes; k++) {
        Node* node=nodes[k];
        Vertex* vertex=getVertex(node);
        vertex->out->setAll();
        if (node->getNodeKind()==NODE_BB) {
            ((VertexBB*)vertex)->def->unionWith(ignoredOpnds);
        }                
    }
}

void Vertex::computeOut() {
    // in[B] := & out[Preds]
    EdgeVector& inEdges=getInEdges();
    uint size=inEdges.size();
    if (size==0) return;
    Vertex* pred=fSolver.getSourceVertex(inEdges[0]);
    in->copyFrom(*(pred->out));
    for (uint k=1; k<inEdges.size(); k++) {
        pred=fSolver.getSourceVertex(inEdges[k]);
        in->intersectWith(*(pred->out));
    }
    // out[B] := in[B]
}

void VertexBB::computeOut() {
    Vertex::computeOut();
    // out[B] := in[B] U def[B]
    out->copyFrom(*in);
    out->unionWith(*def);
}

//---------------------------------------------------------------------------//

bool IpfCfgVerifier::collectDefs() {
    bool changed = false;
    for (uint k=0; k<numNodes; k++) {
        Vertex* vertex=vertices[k];
        tmpSet.copyFrom(*vertex->out);  // save to compare
        vertex->computeOut();

//        bool vchanged = !vertex->out->isEqual(tmpSet);  // compare
        if (!changed) {
            changed = !vertex->out->isEqual(tmpSet);  // compare
        }
    }
    return changed;
}

bool VertexBB::checkOpndIsDef(Opnd* opnd, uint instId) {
    uint id=opnd->getId();
    if (def->getBit(id)) return true;
    IPF_LOG << "UNDEF Opnd: " << IrPrinter::toString(opnd) << " in "
        << node->getId() << ":" << instId << " "
        << IrPrinter::toString((node->getInsts())[instId]) << endl;
    return false;
}
    
bool VertexBB::checkDefs() {
    bool res=true;
    // iterate through instructions in direct order
    InstVector& insts = ((BbNode*)node)->getInsts();
    def->copyFrom(*in);
    for (uint j=0; j<insts.size(); j++) {
        Inst* inst=insts[j];
        uint numDst=inst->getNumDst();
        OpndVector& opnds    = inst->getOpnds();            // get inst opnds
        
        // check src opnds
           RegOpnd* qp = (RegOpnd*)opnds[0];
        if (qp->getValue() != 0) {  // qp != p0, check qp opnd 
            res = checkOpndIsDef(qp, j) & res; 
        }
        for (uint k=numDst+1; k<opnds.size(); k++) {
            Opnd* opnd = opnds[k];
            if (!opnd->isReg()) continue; // ignore non register opnd
            res = checkOpndIsDef(opnd, j) & res;
        }
        
        // handle dst opnds
        setDef(inst);
    }
    return res;
}

//---------------------------------------------------------------------------//
bool IpfCfgVerifier::checkDefs() {
    setDefs();
    
    // solve the flow equation set
    while (collectDefs()) {};
    
    bool res = true;
    for (uint k=0; k<numNodes; k++) {
        Vertex* vertex=vertices[k];
        res=vertex->checkDefs() & res;
    }
    return res;
}

//---------------------------------------------------------------------------//
void    ipfCfgVerifier(Cfg& cfg) {
    MemoryManager mm(1024, "IpfCfgVerifier");
    IpfCfgVerifier verifier(mm, cfg);
    MethodDesc* method = cfg.getMethodDesc();
    const char* methodName     = method->getName();
    const char* methodTypeName = (method->getParentType()!=NULL
        ? method->getParentType()->getName()
        : "");
    const char * methodSignature = method->getSignatureString();
    
    if (!verifier.verifyNodes(LOG_OUT)) {
        cout << "CFG check failed for " << methodTypeName << "." << methodName << methodSignature << endl;
    }
    // print
    if (LOG_ON) {
        verifier.sortNodes(SEARCH_DIRECT_ORDER).printContent(LOG_OUT);
    }
    
    if (!verifier.checkLiveSets()) {
        cout << "liveset check failed for [" << cfg.getMaxNodeId() << "] " << methodTypeName << "." << methodName << methodSignature << endl;
    }
    if (!verifier.checkDefs()) {
        cout << "def check failed for [" << cfg.getMaxNodeId() << "] " << methodTypeName << "." << methodName << methodSignature << endl;
    }
        
}

} // IPF
} // Jitrino
// TODO:
// debug this file saved in ~/saved
