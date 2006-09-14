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
 * @author Intel, Konstantin M. Anisimov, Igor V. Chebykin
 * @version $Revision$
 *
 */

#ifndef IPFLIVEANALYZER_H_
#define IPFLIVEANALYZER_H_

#include "IpfCfg.h"

using namespace std;

namespace Jitrino {
namespace IPF {

//========================================================================================//
// LiveAnalyzer
//========================================================================================//

class LiveAnalyzer {
public:
                LiveAnalyzer(Cfg&);
    void        makeLiveSets(bool);

    static void updateLiveSet(RegOpndSet&, Inst*);
    static void defOpnds(RegOpndSet&, Inst*);
    static void useOpnds(RegOpndSet&, Inst*);

protected:
    bool        analyzeNode(Node*);

    Cfg         &cfg;
    bool        verify;
};

} // IPF
} // Jitrino

#endif /*IPFLIVEANALYZER_H_*/
