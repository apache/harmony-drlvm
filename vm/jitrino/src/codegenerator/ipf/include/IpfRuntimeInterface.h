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

#ifndef IPFRUNTIMEINTERFACE_H_
#define IPFRUNTIMEINTERFACE_H_

#include "RuntimeInterface.h"

namespace Jitrino {
namespace IPF {

//========================================================================================//
// RuntimeInterface
//========================================================================================//

class RuntimeInterface : public ::Jitrino::RuntimeInterface {
public:
    void           unwindStack(MethodDesc*, JitFrameContext*, bool) ;
    void           getGCRootSet(MethodDesc*, GCInterface*, const JitFrameContext*, bool);
    uint32         getInlineDepth(InlineInfoPtr, uint32);
    Method_Handle  getInlinedMethod(InlineInfoPtr, uint32, uint32);
    void           fixHandlerContext(MethodDesc*, JitFrameContext*, bool);
    void           *getAddressOfThis(MethodDesc*, const JitFrameContext*, bool);
    bool           recompiledMethodEvent(MethodDesc*, void*);
    bool           getBcLocationForNative(MethodDesc*, uint64, uint16*);
    bool           getNativeLocationForBc(MethodDesc*, uint16, uint64*);
    uint16         getInlinedBc(void *v, unsigned int i1, unsigned int i2) { return 0; } // TODO

protected:

    // getGCRootSet support
    U_8*           findSafePoint(U_8*, uint32, uint64);
    void           enumerateRootSet(GCInterface*, const JitFrameContext*, U_8*);
    void**         getContextValue(int32);
    void           reportMptr(int32, int32);
    void           reportBase(int32);
    bool           isMptr(int32);

    GCInterface    *gcInterface;
    const JitFrameContext *context;
};

} // IPF
} // Jitrino 

#endif /*IPFRUNTIMEINTERFACE_H_*/
