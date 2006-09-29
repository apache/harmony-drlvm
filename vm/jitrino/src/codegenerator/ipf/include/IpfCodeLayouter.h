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

#ifndef IPFCODELAYOUTER_H_
#define IPFCODELAYOUTER_H_

#include "IpfCfg.h"

namespace Jitrino {
namespace IPF {

//========================================================================================//
// Typedefs
//========================================================================================//

typedef vector< NodeVector* > ChainVector;

//========================================================================================//
// CodeLayouter
//========================================================================================//

class CodeLayouter {
public:
                  CodeLayouter(Cfg &cfg_);
    void          layout();

protected:
    void          mergeNodes();
    bool          mergeNode(BbNode *pred, Edge *pred2succ);
    void          makeChains();
    void          fixBranches();
    void          fixConditionalBranch(BbNode *node);
    void          fixSwitch(BbNode *node);
    void          fixUnconditionalBranch(BbNode *node);
    
    MemoryManager &mm;
    Cfg           &cfg;
    ChainVector   chains;
};

} // IPF
} // Jitrino
#endif /*IPFCODELAYOUTER_H_*/
