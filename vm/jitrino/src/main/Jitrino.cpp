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
#include "PropertyTable.h"
#include "Log.h"
#include "Profiler.h"
//TODO remove
#include "Timer.h"
#include "CountWriters.h"
#include "XTimer.h"
#ifndef PLATFORM_POSIX
#include <windows.h>
#endif
#include "CGSupport.h"
#include "PlatformDependant.h"

#if defined(_IPF_)
	#include "ipf/IpfRuntimeInterface.h"
#else
	#include "ia32/Ia32RuntimeInterface.h"
	#include "ia32/Ia32InternalProfiler.h"
#endif

#include <ostream>

namespace Jitrino {


// the JIT runtime interface
RuntimeInterface* Jitrino::runtimeInterface = NULL;

JITModes Jitrino::jitModes;

struct Jitrino::Flags Jitrino::flags;


void Jitrino::readFlagsFromCommandLine(const JitrinoParameterTable *globalTable, const JitrinoParameterTable *methodTable)
{
    //const char* cg = params->lookup("CG");

#if defined(_IPF_)
	flags.codegen = CG_IPF;
#else
	flags.codegen = CG_IA32;
#endif

    flags.debugging_level = globalTable->lookupInt("debugging_level", -1);
#ifdef SUPPRESS_TOP_SKIP
    flags.skip = false;
#else
    flags.skip = methodTable->lookupBool("top::skip", false);
#endif
    flags.gen_code = methodTable->lookupBool("top::gen_code", true);
    flags.optimize = methodTable->lookupBool("top::optimize", true);
    flags.time = methodTable->lookupBool("time", false);
    flags.code_mapping_supported = methodTable->lookupBool("bc_maps", false);
}

void Jitrino::showFlagsFromCommandLine()
{
//    Log::out() << "    CG={IA32|ipf} = codegen target, currently ignored" << ::std::endl;
    Log::out() << "    debugging_level=int   = ?" << ::std::endl;
    Log::out() << "    top::optimize[={ON,off}] = do optimizations" << ::std::endl;
    Log::out() << "    top::gen_code[={ON,off}] = do codegen" << ::std::endl;
    Log::out() << "    top::skip[={on,OFF}] = do not jit this method" << ::std::endl;
    Log::out() << "    time[={on,OFF}] = dump phase timing at end of run" << ::std::endl;
}

// some demo parameters
bool print_hashtable_info = false;
char which_char = 'a';

// read in some flags to test mechanism
bool initialized_parameters = false;
void initialize_parameters(CompilationInterface* ci, MethodDesc &md)
{

    JitrinoParameterTable* parameterTable = Jitrino::getParameterTable(ci->getJitHandle());
    JitrinoParameterTable *thisPT = parameterTable->getTableForMethod(md);

    CompilationContext* compilationContext = ci->getCompilationContext();
    compilationContext->setThisParameterTable(thisPT);

    Jitrino::readFlagsFromCommandLine(parameterTable, thisPT); // must be first

    TranslatorIntfc::readFlagsFromCommandLine(compilationContext, Jitrino::flags.codegen != Jitrino::CG_IPF);
    IRBuilder::readFlagsFromCommandLine(compilationContext);
    readOptimizerFlagsFromCommandLine(compilationContext);
    CodeGenerator::readFlagsFromCommandLine(compilationContext, Jitrino::flags.codegen != Jitrino::CG_IPF);
    
    // set logging levels
    Log::initializeCategories();
    Log::initializeThresholds(thisPT->lookup("LOG"));

    // do onetime things
    if (!initialized_parameters) {
        initialized_parameters = true;
        

        if(Log::cat_root()->isDebugEnabled()) {
            ::std::cerr << "\n***\nJitrino parameters:\n";
            parameterTable->print(::std::cerr);
            ::std::cerr << "***\n";
        }
        

        if (parameterTable->lookup("help")) {
            Log::out() << ::std::endl 
                       << "Jitrino command line parameters are:" << ::std::endl;
            Jitrino::showFlagsFromCommandLine();
            TranslatorIntfc::showFlagsFromCommandLine();
            showOptimizerFlagsFromCommandLine();
            IRBuilder::showFlagsFromCommandLine();
            CodeGenerator::showFlagsFromCommandLine(Jitrino::flags.codegen != Jitrino::CG_IPF);
        }
    }
}

extern bool genCode(IRManager& irManager,
                    CompilationInterface& compilationInterface,
                    MethodDesc*    methodDesc,
                    FlowGraph*,
                    OpndManager&,
                    Jitrino::Backend cgFlag,
                    bool sinkConstants,
                    bool sinkConstantsOne);

MemoryManager* Jitrino::global_mm = 0; 

Timer* Jitrino::timers = 0; //TODO remove
static CountWriter* countWriter = 0;
static CountTime globalTimer("timer:Total-Compilation"),
				 codegenTimer("timer:Code-generator"),
				 optimizerTimer("timer:Optimizer"),
				 translatorTimer("timer:Translator"),
				 typecheckerTimer("timer:Typechecker");

void Jitrino::crash (const char* msg)
{
	std::cerr << std::endl << "Jitrino crashed" << std::endl;
	if (msg != 0)
		std::cerr << msg << std::endl;

	exit(11);
}
bool Jitrino::Init(JIT_Handle jh, const char* name)
{
    //check for duplicate initialization
    JitrinoParameterTable* parameterTable = getParameterTable(jh);
    if (parameterTable!=NULL) {
        assert(0);
        return false;
    }
    // check if modeName is already in use
    for (JITModes::const_iterator it = jitModes.begin(), end = jitModes.end(); it!=end; ++it) {
        JITModeData* modeData = *it;
        if (modeData->getModeName() == name) {
            assert(0);
            return false;
        }
    }

    bool firstJITInit = global_mm == 0;
    if (firstJITInit) {
        global_mm = new MemoryManager(0,"Jitrino::Init.global_mm"); 
#if defined(_IPF_)
        runtimeInterface = new IPF::IpfRuntimeInterface;
#else
        runtimeInterface = new Ia32::RuntimeInterface;

#endif
        Log::initializeCategories();
    }

    parameterTable = new (*global_mm) JitrinoParameterTable(*global_mm, 1024);
    JITModeData*  modeData = new (*global_mm)JITModeData(jh, name, parameterTable);
    jitModes.push_back(modeData);

    return true;
}

void Jitrino::DeInit()
{
//TODO remove
    //deinits all modes..
//    if( flags.time )
//        dumpTimers();

	if (countWriter != 0)
	{
		delete countWriter;
		countWriter = 0;
	}

#ifndef _IPF_
	Ia32::InternalProfiler::deinit();
#endif
}

void Jitrino::NextCommandLineArgument(JIT_Handle jit, const char *name, const char *arg)
{
    if (strnicmp(name, "-xjit", 5) == 0) {
        size_t argLen = strlen(arg);
        JITModeData* paramMode = NULL;
        for (JITModes::const_iterator it = jitModes.begin(), end = jitModes.end(); it!=end; ++it) {
            JITModeData* mode = *it;
            std::string modeName = mode ->getModeName();
            if (argLen + 2 < modeName.length() && !strncmp(modeName.c_str(), arg, argLen)
                && !strncmp(modeName.c_str() + argLen, "::", 2))
            {
                paramMode = mode;
                arg = arg + argLen + 2;
                break;
            }
        }
        JITModeData* currentJitMode = getJITModeData(jit);
        if (paramMode == NULL || paramMode == currentJitMode) {
            currentJitMode->setParameterTable(currentJitMode->getParameterTable()->insertFromCommandLine(arg));
        }
    }
    return;
}

static void postTranslator(IRManager& irManager)
{
    FlowGraph& flowGraph = irManager.getFlowGraph();
    MethodDesc& methodDesc = irManager.getMethodDesc();
    if (Log::cat_fe()->isIREnabled()) {
        Log::out() << "PRINTING LOG: After Translator" << ::std::endl;
        flowGraph.printInsts(Log::out(), methodDesc);
    }
    flowGraph.cleanupPhase();
    if (Log::cat_fe()->isIR2Enabled()) {
        Log::out() << "PRINTING LOG: After Cleanup" << ::std::endl;
        flowGraph.printInsts(Log::out(), methodDesc);
    }
}

bool compileMethod(CompilationInterface& compilationInterface) {
	
    if(Jitrino::flags.skip) {
        return false;
	}

    //
    // IRManager contains the method's IR for the global optimizer.
    // It contains a memory manager that is live during optimization
    //
    JitrinoParameterTable* parameterTable = Jitrino::getParameterTable(compilationInterface.getJitHandle());
    JitrinoParameterTable* methodParameterTable = parameterTable->getTableForMethod(*compilationInterface.getMethodToCompile());
    IRManager   irManager(compilationInterface, *methodParameterTable);
    
    MethodDesc* methDesc = compilationInterface.getMethodToCompile();
    MemoryManager& ir_mmgr = irManager.getMemoryManager();
    
    // init global map for handlers
    initHandleMap(ir_mmgr, methDesc);
    // 
    // add bc <-> HIR code map handler
    StlVector<uint64> *bc2HIRMap;
    
    if (compilationInterface.isBCMapInfoRequired()) {
        bc2HIRMap = new(ir_mmgr) StlVector<uint64>(ir_mmgr, methDesc->getByteCodeSize() 
                * (ESTIMATED_HIR_SIZE_PER_BYTECODE) + 5);
        addContainerHandler(bc2HIRMap, bcOffset2HIRHandlerName, methDesc);
    }

	if (countWriter == 0)
	{
		const char* arg = parameterTable->lookup("counters");
		if (arg != 0)  // can be empty string ""
		{
#ifdef _WIN32
			if (strnicmp(arg, "mail:", 5) == 0)
				countWriter = new CountWriterMail(arg+5);
			else
#endif
				countWriter = new CountWriterFile(arg);
		}
		else if (Jitrino::flags.time)
			countWriter = new CountWriterFile(0);

		XTimer::initialize(countWriter != 0);
	}

    //
    // translate the method
    //
    globalTimer.start();
    translatorTimer.start();
    TranslatorIntfc::translateByteCodes(irManager);

    translatorTimer.stop();
    postTranslator(irManager);
    //
    // optimize it
    //
    optimizerTimer.start();
	
    if (Jitrino::flags.optimize) {
        if (!optimize(irManager)) {
            globalTimer.stop();
            optimizerTimer.stop();
            return FALSE; // failure optimizing
        }
    }

	optimizerTimer.stop();

    //
    // generate code
    //
    bool success = false;
    codegenTimer.start();
    if (Jitrino::flags.gen_code) {
        // Modification of data only happens during the codegen stage.
        // The lock must not surround a code where managed code execution 
        // may happen. 
        // It's not expected that code gen phase will lead to a resolution 
        // (and thus to execution of managed code), so it should be safe to 
        // lock here.
        // Though there is a very little chance that that the resolution (and 
        // managed code execution) happen in code gen. In this case will
        // need to move the lock into code gen code.
#if !defined(_IPF_) // on IPF, the whole compilation session protected by lock
        compilationInterface.lockMethodData();
#endif

        if (methDesc->getCodeBlockSize(0) > 0 || methDesc->getCodeBlockSize(1)){
            success = true;
        }
        else {
            OptimizerFlags& optimizerFlags = *irManager.getCompilationContext()->getOptimizerFlags();
            success = genCode(irManager, 
                              compilationInterface, 
                              &irManager.getMethodDesc(), 
                              &irManager.getFlowGraph(),
                              irManager.getOpndManager(),
                              Jitrino::flags.codegen,
                              optimizerFlags.sink_constants,
                              optimizerFlags.sink_constants1);
        }
#if !defined(_IPF_) // on IPF, the whole compilation session protected by lock
        compilationInterface.unlockMethodData();
#endif
    }
    codegenTimer.stop();
    globalTimer.stop();

    // remove bc <-> HIR code map handler
    if (compilationInterface.isBCMapInfoRequired()) {
        removeContainerHandler(bcOffset2HIRHandlerName, methDesc);
    }
    return success;
}


bool Jitrino::CompileMethod(CompilationInterface* compilationInterface) {
#ifdef _IPF_ //IPF CG params are not safe -> add them to CompilationContext and remove this lock
    compilationInterface->lockMethodData();
#endif
    bool success = false;
    MethodDesc& methodDesc = *compilationInterface->getMethodToCompile();
    const char* methodName = methodDesc.getName();
    const char* methodTypeName = methodDesc.getParentType()->getName();
    const char* methodSignature=methodDesc.getSignatureString();

    Log::pushSettings();

    initialize_parameters(compilationInterface, methodDesc);
	Log::setMethodToCompile(methodTypeName, methodName, methodSignature, methodDesc.getByteCodeSize());
    
    if (methodDesc.getByteCodeSize() <= 0) {
        Log::cat_root()->error << " ... Skipping because of 0 byte codes ..." << ::std::endl;
        assert(0);
    } else {
		success = compileMethod(*compilationInterface);
    }
	Log::clearMethodToCompile(success, compilationInterface);
	Log::popSettings(); // discard current and restore previous log settings
#ifdef _IPF_
    compilationInterface->unlockMethodData();
#endif
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

//TODO remove
Timer *
Jitrino::addTimer(const char *name)
{
    Timer *newTimer = new (*global_mm) Timer(name, timers);
    timers = newTimer;
    return newTimer;    
}

//TODO remove
void 
Jitrino::dumpTimers()
{
    Timer *timer = timers;
    if (timer) {
        Log::out() << ::std::endl;
        Log::out() << "Timers: " << ::std::endl;
        while (timer) {
            timer->print(Log::out());
            timer = timer->getNext();
        }
    }
}

JITModeData* Jitrino::getJITModeData(JIT_Handle jit) {
    for (JITModes::const_iterator it = jitModes.begin(), end = jitModes.end(); it!=end; ++it) {
        JITModeData* modeData = *it;
        if (modeData->getJitHandle() == jit) {
            return modeData;
        }
    }
    return NULL;
}

JitrinoParameterTable* Jitrino::getParameterTable(JIT_Handle jit) {
    JITModeData* modeData = getJITModeData(jit);
    return modeData!=NULL?modeData->getParameterTable(): NULL;
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
		if ((Jitrino::tlsLogKey = TlsAlloc()) == TLS_OUT_OF_INDEXES) {
			assert(false);
			return false;
			}
	// fall through, the new process creates a new thread

	case DLL_THREAD_ATTACH: 
		// notify interested parties (only one now)
		Jitrino::Log::notifyThreadStart(&Jitrino::tlsLogKey);
		break; 

	case DLL_THREAD_DETACH: 
		// notify interested parties (only one now)
		Jitrino::Log::notifyThreadFinish(&Jitrino::tlsLogKey);
		break; 

	case DLL_PROCESS_DETACH: 
		// notify interested parties (only one now)
		Jitrino::Log::notifyThreadFinish(&Jitrino::tlsLogKey);
		// release the TLS index
		TlsFree(Jitrino::tlsLogKey); 
		Jitrino::tlsLogKey = TLS_OUT_OF_INDEXES;
		break; 

	default:
		break; 
		} 
return TRUE; 
	}

#endif // defined(_WIN32) || defined(_WIN64)
