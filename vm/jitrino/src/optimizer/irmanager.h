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
 * @version $Revision: 1.15.8.2.4.4 $
 *
 */

#ifndef _IRMANAGER_H_
#define _IRMANAGER_H_

#include "CompilationContext.h"
#include "MemoryManager.h"
#include "MemoryEstimates.h"
#include "Opnd.h"
#include "Inst.h"
#include "FlowGraph.h"
#include "VMInterface.h"
#include "CGSupport.h"

namespace Jitrino {

class CodeProfiler;
class MethodDesc;
class TypeManager;
class JitrinoParameterTable;
class CompilationContext;
typedef StlHashMap<VarOpnd*, VarOpnd*> GCBasePointerMap;

class IRManager {
public:
    // Top-level IRManager
    IRManager(CompilationInterface& compilationInterface, JitrinoParameterTable& parameterTable) 
        : _parent(0),
          _memoryManagerBase(compilationInterface.getMethodToCompile()->getByteCodeSize()*
                             ESTIMATED_MEMORY_PER_BYTECODE,
                             "IRManager::memoryManager"),
          _memoryManager(_memoryManagerBase),
          _opndManager(*(new (_memoryManager) OpndManager(compilationInterface.getTypeManager(),_memoryManager))), 
          _instFactory(*(new (_memoryManager) InstFactory(_memoryManager,
                         *compilationInterface.getMethodToCompile()))), 
          _flowGraph(*(new (_memoryManager) FlowGraph(_memoryManager,_instFactory,_opndManager))),
          _dominatorTree(NULL),
          _loopTree(NULL),
          _inlinedReturnOpnd(0),
          _inlineOptPath(NULL),
          _gcBasePointerMap(*(new (_memoryManager) GCBasePointerMap(_memoryManager))),
          _inSsa(false),
          _lastSsaFixupTraversalNum(0),
          _lastCriticalEdgeSplitTraversalNum(0),
          _minRegionInstId(_instFactory.getNumInsts()),
          _heatThreshold(0),
          _abort(false),
          _compilationInterface(compilationInterface),
          _parameterTable(parameterTable),
          _codeProfiler(NULL),
          _typeManager(compilationInterface.getTypeManager()), 
          _methodDesc(*compilationInterface.getMethodToCompile()),
          _jsrEntryMap(NULL)
    {
        _flowGraph.setIRManager(this);
    }

    // Nested IRManager for inlined region
    IRManager(IRManager& containingIRManager, MethodDesc& regionMethodDesc, 
              Opnd *returnOpnd) 
        : _parent(&containingIRManager),
          _memoryManagerBase(0, "IRManager::memoryManagerBase"),
          _memoryManager(containingIRManager.getMemoryManager()),
          _opndManager(containingIRManager.getOpndManager()), 
          _instFactory(containingIRManager.getInstFactory()), 
          _flowGraph(*(new (_memoryManagerBase) FlowGraph(_memoryManager, _instFactory, _opndManager))),
          _dominatorTree(NULL),
          _loopTree(NULL),
          _inlinedReturnOpnd(returnOpnd),
          _inlineOptPath(_parent->getInlineOptPath()),
          _gcBasePointerMap(_memoryManagerBase),
          _inSsa(false),
          _lastSsaFixupTraversalNum(0),
          _lastCriticalEdgeSplitTraversalNum(0),
          _minRegionInstId(_instFactory.getNumInsts()),
          _heatThreshold(containingIRManager.getHeatThreshold()),
          _abort(_parent->getAbort()),
          _compilationInterface(containingIRManager.getCompilationInterface()),
          _parameterTable(containingIRManager.getParameterTable()),
          _codeProfiler(NULL),
          _typeManager(_compilationInterface.getTypeManager()), 
          _methodDesc(regionMethodDesc),
          _jsrEntryMap(NULL)
    {
        _flowGraph.setIRManager(this);
    }

    // The compilation interface to the VM
    CompilationInterface&   getCompilationInterface() {return _compilationInterface;}

    // The parameter table for the top-level method
    JitrinoParameterTable&  getParameterTable() {return _parameterTable;}

    // The memory manager for the top-level HIR
    MemoryManager&  getMemoryManager()  {return _memoryManager; }

    // The memory manager for this region - if an inlined region, this is 
    // deallocated after splicing into top-level HIR
    MemoryManager&  getNestedMemoryManager() { return _memoryManagerBase; } 

    // The method for this region
    MethodDesc&     getMethodDesc()     {return _methodDesc;}

    // The type manager for the top-level method
    TypeManager&    getTypeManager()    {return _typeManager;}

    // The operand manager for the top-level method
    OpndManager&    getOpndManager()    {return _opndManager;}

    // The instruction factory for the top-level method
    InstFactory&    getInstFactory()    {return _instFactory;}

    // The flowgraph for this region
    FlowGraph&      getFlowGraph()      {return _flowGraph;}

    // The dominator tree for this region - may be NULL or invalid
    DominatorTree*  getDominatorTree()  {return _dominatorTree;}
    void            setDominatorTree(DominatorTree* tree) { _dominatorTree = tree; }

    // The loop tree for this region - may be NULL or invalid    
    LoopTree*       getLoopTree()       {return _loopTree;}
    void            setLoopTree(LoopTree* tree) { _loopTree = tree; }

    // The return operand for this region
    Opnd*           getReturnOpnd()     { return _inlinedReturnOpnd; }
    void            setReturnOpnd(Opnd* newReturnOpnd)     { _inlinedReturnOpnd = newReturnOpnd; }

    // The optimization path inlined regions
    const char*     getInlineOptPath()  { return _inlineOptPath; }
    void            setInlineOptPath(const char* path) { /*assert(_parent == NULL); */ _inlineOptPath = path; }

    // The GCMap for region - must be set by GC Managed Pointer Analysis
    GCBasePointerMap& getGCBasePointerMap() {return _gcBasePointerMap;}

    // The SSA state of the region
    bool            getInSsa()          {return _inSsa;}
    void            setInSsa(bool inSsa){_inSsa = inSsa;} 

    // Is the SSA up-to-date - i.e. have phis been updated after edge removal
    bool            isSsaUpdated() {return getInSsa() && (_lastSsaFixupTraversalNum == _flowGraph.getEdgeRemovalTraversalNum());}
    void            setSsaUpdated() {_lastSsaFixupTraversalNum = _flowGraph.getEdgeRemovalTraversalNum();}

    // Are critical edges split?  This be explicitly updated by the optimizer.
    bool            areCriticalEdgesSplit() {return _lastCriticalEdgeSplitTraversalNum == _flowGraph.getModificationTraversalNum();}
    void            setCriticalEdgesSplit() {_lastCriticalEdgeSplitTraversalNum = _flowGraph.getModificationTraversalNum();}

    // The minimum Instruction Id in this region.
    uint32          getMinimumInstId() { return _minRegionInstId; }

    // The DPGO Code Profile
    CodeProfiler*   getCodeProfiler()   {return _codeProfiler;}
    void            setCodeProfiler(CodeProfiler* profiler) { _codeProfiler = profiler; }

    // The DPGO threshold for hotness - blocks with a execution count 
    // greater than this threshold should be considered hot
    double          getHeatThreshold()  {return _heatThreshold;}
    void            setHeatThreshold(double heatThreshold) {_heatThreshold = heatThreshold;}

    // Set if this compile should be aborted
    bool            getAbort() {return _abort;}
    void            setAbort() {_abort = true; if(_parent != NULL) _parent->setAbort();}

    // used for jsr-ret inlining, produced by translator
    void            setJsrEntryMap(JsrEntryInstToRetInstMap* mp) { _jsrEntryMap = mp; }
    JsrEntryInstToRetInstMap* getJsrEntryMap() { return _jsrEntryMap; }

    // The parent IRManager - NULL if this is the top-level IRManager
    IRManager*      getParent() { return _parent; }

    // The HIR typechecker main entry point
    enum OptimizerPhase { OP_FrontEnd, OP_Optimizer };

    CompilationContext* getCompilationContext()  {return getCompilationInterface().getCompilationContext();}

private:
    IRManager*       _parent;
    MemoryManager    _memoryManagerBase;
    MemoryManager&   _memoryManager;
    OpndManager&     _opndManager;
    InstFactory&     _instFactory;
    FlowGraph&       _flowGraph;
    DominatorTree*   _dominatorTree;
    LoopTree*        _loopTree;
    Opnd*            _inlinedReturnOpnd;
    const char*      _inlineOptPath;
    GCBasePointerMap _gcBasePointerMap;
    bool             _inSsa;
    uint32           _lastSsaFixupTraversalNum;
    uint32           _lastCriticalEdgeSplitTraversalNum;
    uint32           _minRegionInstId;
    double           _heatThreshold;
    bool             _abort;

    CompilationInterface&   _compilationInterface;
    JitrinoParameterTable&  _parameterTable;
    CodeProfiler*           _codeProfiler;
    TypeManager&            _typeManager;
    MethodDesc&             _methodDesc;
    JsrEntryInstToRetInstMap* _jsrEntryMap;
};

/** MapHandler is auxilary class to eliminate direct usage of map hanlers between HLO and codegenerator */

class MapHandler {
public:
    MapHandler(const char* handlerName, MethodDesc* meth) {
        handler = getContainerHandler(handlerName, meth);
        }
    uint64 getMapEntry(uint64 key) {
        return ::Jitrino::getMapEntry(handler, key);
    }
    void setMapEntry(uint64 key, uint64 value) {
        ::Jitrino::setMapEntry(handler, key, value);
    }
private:
    void* handler;
};

class VectorHandler {
public:
    VectorHandler(const char* handlerName, MethodDesc* meth) {
        handler = getContainerHandler(handlerName, meth);
    }
    uint64 getVectorEntry(uint64 key) {
        return ::Jitrino::getVectorEntry(handler, key);
    }
    void setVectorEntry(uint64 key, uint64 value) {
        ::Jitrino::setVectorEntry(handler, key, value);
    }
private:
    void* handler;
};

} //namespace Jitrino 

#endif
