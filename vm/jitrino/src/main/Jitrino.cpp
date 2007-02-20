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
 * @author Intel, Mikhail Y. Fursov
 * @version $Revision: 1.29.8.3.4.4 $
 *
 */
#include "Jitrino.h"
#include "optimizer.h"
#include "IRBuilder.h"
#include "irmanager.h"
#include "FlowGraph.h"
#include "MemoryEstimates.h"
#include "TranslatorIntfc.h"
#include "CodeGenIntfc.h"
#include "Log.h"
#include "CountWriters.h"
#include "XTimer.h"
#include "DrlVMInterface.h"

#ifndef PLATFORM_POSIX
    #pragma pack(push)
    #include <windows.h>
    #define vsnprintf _vsnprintf
    #pragma pack(pop)
#else
    #include <stdarg.h>
    #include <sys/stat.h>
    #include <sys/types.h>
#endif //PLATFORM_POSIX
#include "CGSupport.h"
#include "PlatformDependant.h"
#include "JITInstanceContext.h"
#include "PMF.h"
#include "PMFAction.h"

#if defined(_IPF_)
    #include "IpfRuntimeInterface.h"
#else
    #include "ia32/Ia32RuntimeInterface.h"
#endif

#include <ostream>

namespace Jitrino {


// the JIT runtime interface
RuntimeInterface* Jitrino::runtimeInterface = NULL;

JITInstances* Jitrino::jitInstances = NULL;

struct Jitrino::Flags Jitrino::flags;



// some demo parameters
bool print_hashtable_info = false;
char which_char = 'a';

// read in some flags to test mechanism
bool initialized_parameters = false;
void initialize_parameters(CompilationContext* compilationContext, MethodDesc &md)
{
    // BCMap Info Required
    ((DrlVMCompilationInterface*)(compilationContext->getVMCompilationInterface()))->setBCMapInfoRequired(true);

    // do onetime things
    if (!initialized_parameters) {
        initialized_parameters = true;
    }
}

MemoryManager* Jitrino::global_mm = 0; 

static CountWriter* countWriter = 0;
static CountTime globalTimer("total-compilation time");
static SummTimes summtimes("action times");

void Jitrino::crash (const char* msg)
{
    std::cerr << std::endl << "Jitrino crashed" << std::endl;
    if (msg != 0)
        std::cerr << msg << std::endl;

    exit(11);
}
void crash (const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    char buff[1024];
    vsnprintf(buff, sizeof(buff), fmt, args);

    std::cerr << buff;
    exit(11);
}


bool Jitrino::Init(JIT_Handle jh, const char* name)
{
    //check for duplicate initialization
    JITInstanceContext* jitInstance = getJITInstanceContext(jh);
    if (jitInstance!=NULL) {
        assert(0);
        return false;
    }
    // check if jitName is already in use

    if (jitInstances) {
        for (JITInstances::const_iterator it = jitInstances->begin(), end = jitInstances->end(); it!=end; ++it) {
            JITInstanceContext* jitContext = *it;
            if (jitContext->getJITName() == name) {
                assert(0);
                return false;
            }
        }
    }else {
        global_mm = new MemoryManager(0,"Jitrino::Init.global_mm"); 
#if defined(_IPF_)
        runtimeInterface = new IPF::RuntimeInterface;
        flags.codegen = CG_IPF;
#else
        runtimeInterface = new Ia32::RuntimeInterface;
        flags.codegen = CG_IA32;
#endif
        jitInstances = new (*global_mm) JITInstances(*global_mm);

        flags.time=false;
    }

    jitInstance = new (*global_mm) JITInstanceContext(*global_mm, jh, name);
    jitInstances->push_back(jitInstance);

    jitInstance->getPMF().init(jitInstances->size() == 1);

    if (countWriter == 0 && jitInstance->getPMF().getBoolArg(0, "time", false)) {
        countWriter = new CountWriterFile(0);
        XTimer::initialize(true);
    }

    return true;
}

void Jitrino::DeInit(JIT_Handle jh)
{
    JITInstanceContext* jitInstance = getJITInstanceContext(jh);
    if (jitInstance==NULL) {
        assert(0);
        return;
    }

    if (countWriter != 0) {
        jitInstance->getPMF().summTimes(summtimes);
    }
        jitInstance->getPMF().deinit();

    killJITInstanceContext(jitInstance);

    if (jitInstances->empty()) {
        if (countWriter != 0)  {
        delete countWriter;
        countWriter = 0;
    }
    }
}

class FalseSessionAction: public SessionAction {
public:
    virtual void run () {getCompilationContext()->setCompilationFailed(true);}

};
static ActionFactory<FalseSessionAction> _false("false");

class LockMethodSessionAction : public SessionAction {
public:
    virtual void run () {
        CompilationContext* cc = getCompilationContext();
        CompilationInterface* ci = cc->getVMCompilationInterface();
        ci->lockMethodData();
        MethodDesc* methDesc = ci->getMethodToCompile();
        if (methDesc->getCodeBlockSize(0) > 0 || methDesc->getCodeBlockSize(1) > 0){
            cc->setCompilationFinished(true);
            ci->unlockMethodData();
        }
    }
};
static ActionFactory<LockMethodSessionAction> _lock_method("lock_method");

class UnlockMethodSessionAction : public SessionAction {
public:
    virtual void run () {
        getCompilationContext()->getVMCompilationInterface()->unlockMethodData();
    }
};
static ActionFactory<UnlockMethodSessionAction> _unlock_method("unlock_method");


void runPipeline(CompilationContext* c) {

    globalTimer.start();

    PMF::PipelineIterator pit((PMF::Pipeline*)c->getPipeline());
    while (pit.next()) {
        SessionAction* sa = pit.getSessionAction();
        sa->setCompilationContext(c);
        c->setCurrentSessionAction(sa);
        c->stageId++;
        sa->start();
        sa->run();
        sa->stop();
        c->setCurrentSessionAction(0);
        if (c->isCompilationFailed() || c->isCompilationFinished()) {
            break;
        }
    }

    globalTimer.stop();
}

bool compileMethod(CompilationContext* cc) {
    
    if(Jitrino::flags.skip) {
        return false;
    }

    //
    // IRManager contains the method's IR for the global optimizer.
    // It contains a memory manager that is live during optimization
    //
    
    MethodDesc* methDesc = cc->getVMCompilationInterface()->getMethodToCompile();
    MemoryManager& ir_mmgr = cc->getCompilationLevelMemoryManager();
    
    initHandleMap(ir_mmgr, methDesc);

    // add bc <-> HIR code map handler
    StlVector<uint64> *bc2HIRMap;
    if (cc->getVMCompilationInterface()->isBCMapInfoRequired()) {
        bc2HIRMap = new(ir_mmgr) StlVector<uint64>(ir_mmgr, methDesc->getByteCodeSize() 
                * (ESTIMATED_HIR_SIZE_PER_BYTECODE) + 5, ILLEGAL_VALUE);
        addContainerHandler(bc2HIRMap, bcOffset2HIRHandlerName, methDesc);
    }

    runPipeline(cc);
    
    // remove bc <-> HIR code map handler
    if (cc->getVMCompilationInterface()->isBCMapInfoRequired()) {
        removeContainerHandler(bcOffset2HIRHandlerName, methDesc);
    }
    
    bool success = !cc->isCompilationFailed();
    return success;
}


bool Jitrino::CompileMethod(CompilationContext* cc) {
    CompilationInterface* compilationInterface = cc->getVMCompilationInterface();
//#ifdef _IPF_ //IPF CG params are not safe -> add them to CompilationContext and remove this lock
//    compilationInterface->lockMethodData();
//#endif
    bool success = false;
    MethodDesc& methodDesc = *compilationInterface->getMethodToCompile();
    initialize_parameters(cc, methodDesc);
    
    if (methodDesc.getByteCodeSize() <= 0) {
        Log::out() << " ... Skipping because of 0 byte codes ..." << ::std::endl;
        assert(0);
    } else {
        success = compileMethod(cc);
    }
//#ifdef _IPF_
//    compilationInterface->unlockMethodData();
//#endif
    return success;
}

void
Jitrino::UnwindStack(MethodDesc* methodDesc, ::JitFrameContext* context, bool isFirst)
{
    runtimeInterface->unwindStack(methodDesc, context, isFirst);
}

void 
Jitrino::GetGCRootSet(MethodDesc* methodDesc, GCInterface* gcInterface, 
                      const ::JitFrameContext* context, bool isFirst)
{
    runtimeInterface->getGCRootSet(methodDesc, gcInterface, context, isFirst);
}

uint32
Jitrino::GetInlineDepth(InlineInfoPtr ptr, uint32 offset)
{
    return runtimeInterface->getInlineDepth(ptr, offset);
}

Method_Handle
Jitrino::GetInlinedMethod(InlineInfoPtr ptr, uint32 offset, uint32 inline_depth)
{
    return runtimeInterface->getInlinedMethod(ptr, offset, inline_depth);
}

uint16
Jitrino::GetInlinedBc(InlineInfoPtr ptr, uint32 offset, uint32 inline_depth)
{
    return runtimeInterface->getInlinedBc(ptr, offset, inline_depth);
}

bool
Jitrino::CanEnumerate(MethodDesc* methodDesc, NativeCodePtr eip)
{
    return runtimeInterface->canEnumerate(methodDesc, eip);
}

void
Jitrino::FixHandlerContext(MethodDesc* methodDesc, ::JitFrameContext* context, bool isFirst)
{
    runtimeInterface->fixHandlerContext(methodDesc, context, isFirst);
}

void *
Jitrino::GetAddressOfThis(MethodDesc* methodDesc, const ::JitFrameContext* context, bool isFirst) {
    return runtimeInterface->getAddressOfThis(methodDesc, context, isFirst);
}

#ifdef USE_SECURITY_OBJECT
void *
Jitrino::GetAddressOfSecurityObject(MethodDesc* methodDesc, const ::JitFrameContext* context) {
    return runtimeInterface->getAddressOfSecurityObject(methodDesc, context);
}
#endif

bool  
Jitrino::RecompiledMethodEvent(BinaryRewritingInterface & binaryRewritingInterface,
                               MethodDesc *               recompiledMethodDesc, 
                               void *                     data) {
    return runtimeInterface->recompiledMethodEvent(binaryRewritingInterface, recompiledMethodDesc, data);
}

bool 
Jitrino::GetBcLocationForNative(MethodDesc* method, uint64 native_pc, uint16 *bc_pc) { 
    return runtimeInterface->getBcLocationForNative(method, native_pc, bc_pc);
}

bool
Jitrino::GetNativeLocationForBc(MethodDesc* method, uint16 bc_pc, uint64 *native_pc) { 
    return runtimeInterface->getNativeLocationForBc(method, bc_pc, native_pc);
}

JITInstanceContext* Jitrino::getJITInstanceContext(JIT_Handle jitHandle) {
    if (jitInstances)
        for (JITInstances::const_iterator it = jitInstances->begin(), end = jitInstances->end(); it!=end; ++it) {
            JITInstanceContext* jit= *it;
            if (jit->getJitHandle() == jitHandle) {
                return jit;
            }
        }
    return NULL;
}

void Jitrino::killJITInstanceContext(JITInstanceContext* jit) {
    for (JITInstances::iterator it = jitInstances->begin(), end = jitInstances->end(); it!=end; ++it) {
        if (*it == jit) {
            jitInstances->erase(it);
            return;
        }
    }
}



} //namespace Jitrino 
/*--------------------------------------------------------------------
* DllMain definition (Windows only) - DLL entry point function which
* is called whenever a new thread/process which executes the DLL code
* is started/terminated.
* Currently it is used to notify log system only.
*--------------------------------------------------------------------
*/

#if defined(_WIN32) || defined(_WIN64)

extern "C" bool __stdcall DllMain(void *dll_handle, uint32 reason, void *reserved) {

    switch (reason) { 
    case DLL_PROCESS_ATTACH: 
        // allocate a TLS index.
        // fall through, the new process creates a new thread

    case DLL_THREAD_ATTACH: 
        // notify interested parties (only one now)
        break; 

    case DLL_THREAD_DETACH: 
        // notify interested parties (only one now)
        Jitrino::Tls::threadDetach();
        break; 

    case DLL_PROCESS_DETACH: 
        // notify interested parties (only one now)
        // release the TLS index
        Jitrino::Tls::threadDetach();
        break; 

    default:
        break; 
    } 
    return TRUE; 
}

#endif // defined(_WIN32) || defined(_WIN64)
