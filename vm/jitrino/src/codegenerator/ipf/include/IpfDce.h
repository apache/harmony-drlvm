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

#ifndef IPFDCE_H_
#define IPFDCE_H_

#include "IpfCfg.h"

using namespace std;

namespace Jitrino {
namespace IPF {

//========================================================================================//
// Dce
//========================================================================================//

class Dce {
public:
               Dce(Cfg &cfg) : cfg(cfg), currLiveSet(cfg.getMM()) {}
    void       eliminate();

protected:
    bool       isInstDead(Inst*);

    Cfg        &cfg;
    RegOpndSet currLiveSet;
};

} // IPF
} // Jitrino

#endif /*IPFDCE_H_*/
