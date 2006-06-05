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

#include <vector>
#include <string>
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

class JitrinoParameterTable;
class MemoryManager;
class Timer;
class RuntimeInterface;

class JITModeData;
typedef std::vector<JITModeData*> JITModes;

class Jitrino {
public:
	static void crash (const char* msg);
    static bool Init(JIT_Handle jit, const char* name);
    static void DeInit();
    static void  NextCommandLineArgument(JIT_Handle jit, const char *name, const char *arg);
    static bool  CompileMethod(CompilationInterface* compilationInterface);
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

    enum Backend {
        CG_IPF,
        CG_IA32,
        CG_IA
    };

    struct Flags {
        int debugging_level;

        bool skip;
        bool optimize;
        bool gen_code;
        Backend codegen;
        bool time;
        bool code_mapping_supported;
    };
    // Global Jitrino Flags (are set on initialization, not modified at runtime)
    static struct Flags flags;

    static void readFlagsFromCommandLine(const JitrinoParameterTable *globalTable, const JitrinoParameterTable *methodTable);
    static void showFlagsFromCommandLine();

    static Timer *addTimer(const char *);
    static void dumpTimers();
    static JITModeData* getJITModeData(JIT_Handle jit);
    static JitrinoParameterTable* getParameterTable(JIT_Handle jit);
private:
    static MemoryManager *global_mm; 
    static Timer *timers; 
    static RuntimeInterface* runtimeInterface;
	
    static JITModes jitModes;
};

class JITModeData {
public:
    
    JITModeData(JIT_Handle _jitHandle, const char* _modeName, JitrinoParameterTable* _params) 
        : jitHandle(_jitHandle), modeName(_modeName), parameterTable(_params), profInterface(NULL)
    {useJet = isJetModeName(_modeName);}

    JIT_Handle getJitHandle() const {return jitHandle;}
    
    const std::string& getModeName() const {return modeName;}

    JitrinoParameterTable* getParameterTable() const {return parameterTable;}
    void setParameterTable(JitrinoParameterTable* pTable) {parameterTable = pTable;}

    ProfilingInterface* getProfilingInterface() const {return profInterface;}
    void setProfilingInterface(ProfilingInterface* pi) {assert(profInterface == NULL); profInterface = pi;}
    
    bool isJet() const {return useJet;}
    static bool isJetModeName(const char* modeName) {return strlen(modeName)>=3 && !strncmp(modeName, "JET", 3);}
private:
    JIT_Handle      jitHandle;
    std::string     modeName;
    JitrinoParameterTable* parameterTable;
    ProfilingInterface* profInterface;
    bool useJet;
};

} //namespace Jitrino 

#endif // _JITRINO_H_
