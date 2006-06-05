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
 * @version $Revision: 1.9.24.4 $
 *
 */

#ifndef _HASHVALUENUMBER_H_
#define _HASHVALUENUMBER_H_

#include "irmanager.h"
#include "optpass.h"

namespace Jitrino {

class IRManager;
class Inst;
class DominatorTree;
class MemoryOpt;
class FlowGraph;

DEFINE_OPTPASS(HashValueNumberingPass)

class HashValueNumberer {
public:
    HashValueNumberer(IRManager& irm, DominatorTree& dom,
                      bool useBranchConditions = true,
                      bool ignoreAllFlow = false) 
        : irManager(irm), dominators(dom),
          fg(irm.getFlowGraph()),
          useBranches(useBranchConditions)
    {
    }
    void doValueNumbering(MemoryOpt *mopt=0);
    void doGlobalValueNumbering(MemoryOpt *mopt=0);
private:
    IRManager&      irManager;
    DominatorTree&  dominators;
    FlowGraph&  fg;
    bool  useBranches; // do we try to take account of in-edge conditions in a block?
};

} //namespace Jitrino 

#endif // _HASHVALUENUMBER_H_
