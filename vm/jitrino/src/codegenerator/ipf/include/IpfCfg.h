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
 *
 */

#ifndef IPFCFG_H_
#define IPFCFG_H_

#include "Type.h"
#include "CodeGenIntfc.h"
#include "IpfType.h"
#include "IpfEncoder.h"
#include "MemoryManager.h"

using namespace std;

namespace Jitrino {
namespace IPF {

//========================================================================================//
// Forward declarations
//========================================================================================//

class Opnd;
class Inst;
class Node;
class BbNode;
class Edge;
class OpndManager;

//========================================================================================//
// Constant
//========================================================================================//

class Constant {
public:
                 Constant(DataKind);
    void         setOffset(int32 offset_)    { offset = offset_; }
    int32        getOffset()                 { return offset; }
    void         setAddress(void *address_)  { address = address_; }
    void         *getAddress()               { return address; }
    void         setSize(int16 size_)        { size = size_; }
    int16        getSize()                   { return size; }
    DataKind     getDataKind()               { return dataKind; }
    virtual void *getData()                  { return NULL; }

protected:
    void         *address;
    int32        offset;
    int16        size;
    DataKind     dataKind;
};

//========================================================================================//
// SwitchConstant
//========================================================================================//

class SwitchConstant : public Constant {
public:
               SwitchConstant(MemoryManager&);
    void       addEdge(Edge*);
    Edge       *getEdge(int16 choice) { return edgeList[choice]; };
    uint16     getChoice(Edge*);
    uint16     getChoiceCount()       { return edgeList.size(); };
    void       *getData(void*);
    int16      getSize();
    virtual    ~SwitchConstant() {}

protected:
    EdgeVector edgeList;
};

//========================================================================================//
// int64 Constants
//========================================================================================//

class Int64Constant : public Constant {
public:
           Int64Constant(int64 value) : Constant(DATA_I64), value(value) { setSize(sizeof(int64)); }
    void   *getData() { return NULL; };
    int64  getValue() { return value; };

protected:
    int64  value;
};

//========================================================================================//
// Float Constants
//========================================================================================//

class FloatConstant : public Constant {
public:
           FloatConstant(float value) : Constant(DATA_S), value(value) { setSize(sizeof(float)); }
    void   *getData() { return NULL; }
    double getValue() { return value; };

protected:
    float  value;
};

//========================================================================================//
// Double Constants
//========================================================================================//

class DoubleConstant : public Constant {
public:
           DoubleConstant(double value) : Constant(DATA_D), value(value) { setSize(sizeof(double)); }
    void   *getData() { return NULL; }
    double getValue() { return value; };

protected:
    double value;
};

//========================================================================================//
// Opnd
//========================================================================================//

class Opnd : public CG_OpndHandle {
public:
                    Opnd(uint32, OpndKind=OPND_INVALID, DataKind=DATA_INVALID, int64=0);

    uint16          getId()                         { return id; }
    OpndKind        getOpndKind()                   { return opndKind; }
    DataKind        getDataKind()                   { return dataKind; }
    void            setValue(int64 value_)          { value = value_; }
    virtual int64   getValue()                      { return value; }

    bool            isReg()                         { return IpfType::isReg(opndKind); }
    bool            isGReg()                        { return IpfType::isGReg(opndKind); }
    bool            isFReg()                        { return IpfType::isFReg(opndKind); }
    bool            isImm()                         { return IpfType::isImm(opndKind); }
    bool            isQp()                          { return opndKind == OPND_P_REG; }
    bool            isFloating()                    { return IpfType::isFloating(dataKind); }
    bool            isSigned()                      { return IpfType::isSigned(dataKind); }
    int16           getSize()                       { return IpfType::getSize(dataKind); }
    bool            isWritable();
    bool            isConstant();
    bool            isMem();
    bool            isImm(int);

    bool            isFoldableImm(int16 size)       { return isFoldableImm(value, size); }
    static bool     isFoldableImm(int64 value, int16 size);
    
protected:
    uint16          id;
    OpndKind        opndKind;
    DataKind        dataKind;
    int64           value;
};

//========================================================================================//
// RegOpnd
//========================================================================================//

class RegOpnd : public Opnd {
public:
                RegOpnd(MemoryManager&, uint32, OpndKind, DataKind, int32=LOCATION_INVALID);
    int64       getValue();
    void        setLocation(int32 value_)             { value = value_; }
    int32       getLocation()                         { return value; }

    QpMask      getQpMask()                           { return qpMask; }
    void        andQpMask(QpMask mask)                { qpMask &= mask; }
    void        orQpMask(QpMask mask)                 { qpMask |= mask; }
    bool        isAlive(QpMask mask)                  { return mask & qpMask; }

    void        incSpillCost(uint32 spillCost_)       { spillCost += spillCost_; }
    uint32      getSpillCost()                        { return spillCost; }
    RegOpndSet  &getDepOpnds()                        { return depOpnds; }
    void        insertDepOpnds(RegOpndSet &opnds)     { depOpnds.insert(opnds.begin(), opnds.end()); }
    void        insertDepOpnd(RegOpnd*);
    void        setCrossCallSite(bool crossCallSite_) { crossCallSite = crossCallSite_; }
    bool        isCrossCallSite()                     { return crossCallSite; }

    Int2OpndMap &getCoalesceCands()                   { return coalesceCands; }
    void        addCoalesceCand(uint32 execCnt, RegOpnd *opnd) { coalesceCands.insert(make_pair(execCnt, opnd)); }

    virtual     ~RegOpnd() {}

protected:
    QpMask      qpMask;             // mask of predicate spaces opnd alive in
    // These fields are for register allocation algorithm
    uint32      spillCost;          // number of opnd uses
    RegOpndSet  depOpnds;           // opnds which can not be placed in the same reg with the opnd
    bool        crossCallSite;      // opnd live range crosses call site
    Int2OpndMap coalesceCands;      // 
};

//========================================================================================//
// ConstantRef
//========================================================================================//

class ConstantRef : public Opnd {
public:
    ConstantRef::ConstantRef(uint32 id, Constant *constant, DataKind dataKind = DATA_CONST_REF) :
        Opnd(id, OPND_IMM, dataKind, LOCATION_INVALID), constant(constant) {}

    int64     getValue()     { return (int64)constant->getAddress(); }
    Constant  *getConstant() { return constant; }

protected:
    Constant  *constant;
};

//========================================================================================//
// NodeRef
//========================================================================================//

class NodeRef : public Opnd {
public:
    NodeRef(uint32 id, BbNode *node = NULL) 
    : Opnd(id, OPND_IMM, DATA_NODE_REF, LOCATION_INVALID), node(node) {}
    
    int64    getValue();
    void     setNode(BbNode *node_)  { node = node_; }
    BbNode   *getNode()              { return node; }

protected:
    BbNode   *node;
};

//========================================================================================//
// MethodRef
//========================================================================================//

class MethodRef : public Opnd {
public:
    MethodRef(uint32 id, MethodDesc *method = NULL) 
    : Opnd(id, OPND_IMM, DATA_METHOD_REF, LOCATION_INVALID), method(method) {}
    
    int64       getValue();
    void        setMethod(MethodDesc *method_) { method = method_; }
    MethodDesc  *getMethod()                   { return method; }

protected:
    MethodDesc  *method;
};

//========================================================================================//
// Inst
//========================================================================================//

class Inst {
public:
    Inst(MemoryManager&, InstCode, 
         Opnd* =NULL, Opnd* =NULL, Opnd* =NULL, Opnd* =NULL, Opnd* =NULL, Opnd* =NULL);

    Inst(MemoryManager&, InstCode, Completer, 
         Opnd* =NULL, Opnd* =NULL, Opnd* =NULL, Opnd* =NULL, Opnd* =NULL, Opnd* =NULL);

    Inst(MemoryManager&, InstCode, Completer, Completer, 
         Opnd* =NULL, Opnd* =NULL, Opnd* =NULL, Opnd* =NULL, Opnd* =NULL, Opnd* =NULL);

    Inst(MemoryManager&, InstCode, Completer, Completer, Completer,
         Opnd* =NULL, Opnd* =NULL, Opnd* =NULL, Opnd* =NULL, Opnd* =NULL, Opnd* =NULL);

    InstCode    getInstCode()                        { return instCode; }
    void        setInstCode(InstCode instCode_)      { instCode = instCode_; }

    CompVector  &getComps()                          { return compList; }
    Completer   getComp(uint16 num)                  { return compList[num]; }
    void        addComp(Completer comp_)             { compList.push_back(comp_); }
    void        setComp(uint32 num, Completer comp_) { compList[num] = comp_; }

    void        addOpnd(Opnd *opnd_)                 { opndList.push_back(opnd_); }
    void        removeLastOpnd()                     { opndList.pop_back(); }
    OpndVector  &getOpnds()                          { return opndList; }
    void        setOpnd(uint32 num, Opnd *opnd_)     { opndList[num] = opnd_; }
    Opnd        *getOpnd(uint32 num)                 { return opndList[num]; }
    uint16      getNumDst()                          { return Encoder::getNumDst(instCode); }
    uint16      getNumOpnd()                         { return Encoder::getNumOpnd(instCode); }

    char        *getInstMnemonic()                   { return Encoder::getMnemonic(instCode); }
    char        *getCompMnemonic(Completer comp)     { return Encoder::getMnemonic(comp); }

    uint32      getAddr()                            { return addr; }
    void        setAddr(uint32 addr_)                { addr = addr_; }
    
    bool        isBr();
    bool        isCall();
    bool        isRet();
    bool        isConditionalBranch();
    
protected:
    InstCode    instCode;
    CompVector  compList;
    OpndVector  opndList;
    uint32      addr;       // addr == <bundle's offset in basic block> + <slot's index>
};

//========================================================================================//
// Edge
//========================================================================================//

class Edge {
public:
                Edge(Node*, Node*, double, EdgeKind);
    Node        *getSource()                { return source; }
    Node        *getTarget()                { return target; }
    double      getProb()                   { return prob; }
    void        setProb(double prob_)       { prob = prob_; }
    EdgeKind    getEdgeKind()               { return edgeKind; }
    void        setEdgeKind(EdgeKind kind_) { edgeKind = kind_; }
    void        remove();
    void        insert();
    void        changeSource(Node *source_);
    void        changeTarget(Node *target_);
    bool        isBackEdge();

protected:
    EdgeKind    edgeKind;
    Node        *source;
    Node        *target;
    double      prob;
};

//========================================================================================//
// ExceptionEdge
//========================================================================================//

class ExceptionEdge : public Edge {
public:
                ExceptionEdge(Node*, Node*, double, Type*, uint32);
    Type        *getExceptionType()  { return exceptionType; }
    uint32      getPriority()        { return priority; }

protected:
    Type        *exceptionType;
    uint32      priority;
};

//========================================================================================//
// Node
//========================================================================================//

class Node {
public:
                Node(MemoryManager&, uint32, uint32, NodeKind = NODE_INVALID);

    void        remove();
    void        addEdge(Edge *edge);
    void        removeEdge(Edge *edge);
    Edge        *getOutEdge(EdgeKind edgeKind);
    Edge        *getOutEdge(Node *targetNode);
    Edge        *getInEdge(EdgeKind edgeKind);
    Edge        *getInEdge(Node *targetNode);
    Node        *getDispatchNode();
    void        mergeOutLiveSets(RegOpndSet &resultSet);

    uint16      getId()                             { return id; }
    void        setExecCounter(uint32 execCounter_) { execCounter = execCounter_; }
    uint32      getExecCounter()                    { return execCounter; }
    void        setNodeKind(NodeKind kind_)         { nodeKind = kind_; }
    NodeKind    getNodeKind()                       { return nodeKind; }
    EdgeVector  &getInEdges()                       { return inEdges; }
    EdgeVector  &getOutEdges()                      { return outEdges; }
    void        setLiveSet(RegOpndSet& liveSet_)    { liveSet = liveSet_; }
    void        setLoopHeader(Node *loopHeader_)    { loopHeader = loopHeader_; }
    Node        *getLoopHeader()                    { return loopHeader; }
    RegOpndSet  &getLiveSet()                       { return liveSet; }
    void        clearLiveSet()                      { liveSet.clear(); }
    void        setVisited(bool visited_)           { visited = visited_; }
    bool        isVisited()                         { return visited; }
    bool        isBb()                              { return nodeKind == NODE_BB; }
    
protected:
    uint16      id;               // node unique Id
    uint32      execCounter;      // profile info (how many times the node executes)
    NodeKind    nodeKind;         // 
    EdgeVector  inEdges;          // in edges list
    EdgeVector  outEdges;         // out edges list
    Node        *loopHeader;      // header of loop containing this node, if NULL - node is not in loop
    RegOpndSet  liveSet;          // set of opnds alive on node enter
    bool        visited;          // flag used in node iterating algorithms (live analysis)
};

//========================================================================================//
// BbNode
//========================================================================================//

class BbNode : public Node {
public:
                BbNode(MemoryManager&, uint32, uint32);
    void        addInst(Inst *inst)                 { insts.push_back(inst); }
    void        removeInst(Inst *inst)              { insts.erase(find(insts.begin(),insts.end(),inst)); } 
    InstVector  &getInsts()                         { return insts; }
    void        setLayoutSucc(BbNode *layoutSucc_)  { layoutSucc = layoutSucc_; }
    BbNode      *getLayoutSucc()                    { return layoutSucc; }
    void        setAddress(uint64 address_)         { address = address_; }
    uint64      getAddress()                        { return address; }
    uint64      getInstAddr(Inst *inst)             { return ((uint64)address + inst->getAddr()); }

protected:
    InstVector  insts;
    BbNode      *layoutSucc;
    uint64      address;
};

//========================================================================================//
// Cfg
//========================================================================================//

class Cfg {
public:
                         Cfg(MemoryManager &mm, CompilationInterface &compilationInterface);
    NodeVector           &search(SearchKind searchKind);
    
    MemoryManager        &getMM()                   { return mm; }
    CompilationInterface &getCompilationInterface() { return compilationInterface; }
    void                 setEnterNode(BbNode *node) { enterNode = node; }
    void                 setExitNode(BbNode *node)  { exitNode = node; }
    BbNode               *getEnterNode()            { return enterNode; }
    BbNode               *getExitNode()             { return exitNode; }
    OpndManager          *getOpndManager()          { return opndManager; }
    MethodDesc           *getMethodDesc()           { return compilationInterface.getMethodToCompile(); }

protected:
    void                 makePostOrdered(Node *node, NodeSet &visitedNodes);
    void                 makeDirectOrdered(Node *node, NodeSet &visitedNodesd);
    void                 makeLayoutOrdered();

    MemoryManager        &mm;
    CompilationInterface &compilationInterface;

    OpndManager          *opndManager;
    BbNode               *enterNode;
    BbNode               *exitNode;
    NodeVector           searchResult;
    SearchKind           lastSearchKind;
};

} // IPF
} // Jitrino

#endif /*IPFCFG_H_*/
