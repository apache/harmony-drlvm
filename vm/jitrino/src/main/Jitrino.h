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
 * @author Intel, Mikhail Y. Fursov
 * @version $Revision: 1.21.8.2.4.4 $
 *
 */

#ifndef _JITRINO_H_
#define _JITRINO_H_

#include "open/types.h"
#include "jit_export.h"
#include "VMInterface.h"
#include "Stl.h"
#include <assert.h>

// this 'ifndef' makes Jitrino (in jitrino_dev branch) build successfully both on 
//   Q2-05 and jitrino_dev versions of VM headers
// to be removed as soon as Q2-05 VM headers are NOT needed
//
#ifndef INLINE_INFO_PTR
#define INLINE_INFO_PTR
typedef void * InlineInfoPtr;
#endif

namespace Jitrino {

void crash (const char* fmt, ...);

class MemoryManager;
class RuntimeInterface;
class ProfilingInterface;

class JITInstanceContext;
typedef StlVector<JITInstanceContext*> JITInstances;

class Jitrino {
public:
    static void crash (const char* msg);
    static bool Init(JIT_Handle jit, const char* name);
    static void DeInit(JIT_Handle jit);
    static bool  CompileMethod(CompilationContext* compilationContext);
    static void  UnwindStack(MethodDesc* methodDesc, ::JitFrameContext* context, bool isFirst);
    static void  GetGCRootSet(MethodDesc* methodDesc, GCInterface* gcInterface, const ::JitFrameContext* context, bool isFirst);
    static bool  CanEnumerate(MethodDesc* methodDesc, NativeCodePtr eip);
    static void  FixHandlerContext(MethodDesc* methodDesc, ::JitFrameContext* context, bool isFirst);
    static void* GetAddressOfThis(MethodDesc* methodDesc, const ::JitFrameContext* context, bool isFirst);
#ifdef USE_SECURITY_OBJECT
    static void* GetAddressOfSecurityObject(MethodDesc* methodDesc, const ::JitFrameContext* context);
#endif
    static bool  RecompiledMethodEvent(BinaryRewritingInterface&  binaryRewritingInterface,
                                       MethodDesc * recompiledMethodDesc, void * data);
    static MemoryManager& getGlobalMM() { return *global_mm; }

    static bool GetBcLocationForNative(MethodDesc* method, uint64 native_pc, uint16 *bc_pc);
    static bool GetNativeLocationForBc(MethodDesc* method, uint16 bc_pc, uint64 *native_pc);

    static uint32 GetInlineDepth(InlineInfoPtr ptr, uint32 offset);
    static Method_Handle GetInlinedMethod(InlineInfoPtr ptr, uint32 offset, uint32 inline_depth);
    static uint16 GetInlinedBc(InlineInfoPtr ptr, uint32 offset, uint32 inline_depth);

    enum Backend {
        CG_IPF,
        CG_IA32,
    };

    struct Flags {
        bool skip;
        Backend codegen;
        bool time;
    };
    // Global Jitrino Flags (are set on initialization, not modified at runtime)
    static struct Flags flags;
    static JITInstanceContext* getJITInstanceContext(JIT_Handle jitHandle);
    static void killJITInstanceContext(JITInstanceContext* jit);

private:
    static MemoryManager *global_mm; 
    static RuntimeInterface* runtimeInterface;
    
    static JITInstances* jitInstances;
};


} //namespace Jitrino 

#endif // _JITRINO_H_
