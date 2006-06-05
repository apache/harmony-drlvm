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
 * @version $Revision: 1.30.8.3.4.4 $
 *
 */

#ifndef PLATFORM_POSIX
#include <crtdbg.h>
#endif


#include "Jitrino.h"
#include "DrlVMInterface.h"
#include "MemoryEstimates.h"
#include "PropertyTable.h"
#include "Log.h"
#include "Profiler.h"
#include "CompilationContext.h"

#include "jit_export.h"
#include "jit_export_jpda.h"
#include "open/types.h"
#include "cxxlog.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#if !defined(_IPF_) // No .JET on IPF yet
	#define USE_FAST_PATH
#endif

#ifdef USE_FAST_PATH
	#include "../../jet/jet.h"
#endif

namespace Jitrino {

//
// exported DLL functions for the DRL VM
//
////////////////////////////////////////////////////////
// Optional functions that don't have to be provided.
////////////////////////////////////////////////////////

// Called once at the end of the constructor.
extern "C"
JITEXPORT void 
JIT_init(JIT_Handle jit, const char* name)
{
    std::string initMessage = std::string("Initializing Jitrino.") + name + 
                              " -> ";
    std::string mode = "OPT";
#ifdef USE_FAST_PATH
    if (JITModeData::isJetModeName(name)) mode = "JET";
#endif 
    initMessage = initMessage + mode + " compiler mode";
    INFO(initMessage.c_str());
    Jitrino::Init(jit, name);
#ifndef PLATFORM_POSIX
    //
    // Suppress dialog boxes for regression runs.
    //
#ifdef _DEBUG
    if (!vm_get_boolean_property_value_with_default("vm.assert_dialog")) {

        _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
        _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDOUT);
        _CrtSetReportMode(_CRT_ERROR,  _CRTDBG_MODE_FILE);
        _CrtSetReportFile(_CRT_ERROR,  _CRTDBG_FILE_STDOUT);
        _CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE);
        _CrtSetReportFile(_CRT_WARN, _CRTDBG_FILE_STDOUT);
        _set_error_mode(_OUT_TO_STDERR);
    }
#endif
#endif

#ifdef USE_FAST_PATH
	Jet::setup(jit);
#endif
}

// Called once at the end of the destructor.
extern "C"
JITEXPORT void 
JIT_deinit(JIT_Handle jit)
{
#ifdef USE_FAST_PATH
	Jet::cleanup();
#endif
    profileCtrl.deInit();
    Jitrino::DeInit();
}

extern "C"
JITEXPORT void 
JIT_next_command_line_argument(JIT_Handle jit, const char *name,
                               const char *arg)
{
    Jitrino::NextCommandLineArgument(jit, name, arg);
#ifdef USE_FAST_PATH
	Jet::cmd_line_arg(jit, name, arg);
#endif
}

extern "C"
JITEXPORT void 
JIT_set_profile_access_interface(JIT_Handle jit, EM_Handle em,
                                 EM_ProfileAccessInterface* pc_interface) 
{
    JITModeData* modeData = Jitrino::getJITModeData(jit);
    MemoryManager& mm = Jitrino::getGlobalMM();
    DrlProfilingInterface* pi = new (mm) DrlProfilingInterface(em, jit,
                                                               pc_interface);
    modeData->setProfilingInterface(pi);
}


//Optional
extern "C"
JITEXPORT bool 
JIT_enable_profiling(JIT_Handle jit, PC_Handle pc, EM_JIT_PC_Role role) 
{
    JITModeData* modeData = Jitrino::getJITModeData(jit);
    ProfilingInterface* pi = modeData->getProfilingInterface();
    return pi->enableProfiling(pc, role == EM_JIT_PROFILE_ROLE_GEN ? 
                                JITProfilingRole_GEN: JITProfilingRole_USE);
}

extern "C"
JITEXPORT void 
JIT_gc_start(JIT_Handle jit)
{
}

extern "C"
JITEXPORT void 
JIT_gc_end(JIT_Handle jit)
{
}

extern "C"
JITEXPORT void 
JIT_gc_object_died(JIT_Handle jit, void *java_ref)
{
}

// Called if JIT registered itself to be notified when the class is extended
// Returns TRUE if any code was modified and FALSE otherwise.
extern "C"
JITEXPORT Boolean
JIT_extended_class_callback(JIT_Handle jit, Class_Handle extended_class,
                            Class_Handle new_class, void *callback_data)
{
    return FALSE;
}

// Called if JIT registered itself to be notified when the method is
// overridden
// Returns TRUE if any code was modified and FALSE otherwise
extern "C"
JITEXPORT Boolean
JIT_overridden_method_callback(JIT_Handle jit,
                               Method_Handle  overridden_method,
                               Method_Handle  new_method,
                               void *callback_data)
{
    return FALSE;
}

// Called if JIT registered itself to be notified when the method is
// recompiled
// Returns TRUE if any code was modified and FALSE otherwise 
extern "C"
JITEXPORT Boolean
JIT_recompiled_method_callback(JIT_Handle jit,
                               Method_Handle recompiled_method,
                               void *callback_data)
{
    DrlVMBinaryRewritingInterface binaryRewritingInterface;
    DrlVMMethodDesc methodDesc(recompiled_method);
    bool res = Jitrino::RecompiledMethodEvent(binaryRewritingInterface,
                                              &methodDesc,callback_data);
    return (res ? TRUE : FALSE);
}


////////////////////////////////////////////////////////
// Required functions.
////////////////////////////////////////////////////////

extern "C"
JITEXPORT JIT_Result 
JIT_gen_method_info(JIT_Handle jit, Compile_Handle compilation,
                    Method_Handle method, JIT_Flags flags)
{
    assert(0);
    return JIT_FAILURE;
}

#ifdef USE_FAST_PATH
static bool isJET(JIT_Handle jit)
{
    JITModeData* modeData = Jitrino::getJITModeData(jit);
    return modeData->isJet();
}
#endif


extern "C"
JITEXPORT JIT_Result 
JIT_compile_method(JIT_Handle jitHandle, Compile_Handle compilation,
                   Method_Handle method, JIT_Flags flags)
{
    assert(0);
    return JIT_FAILURE;
}

extern "C"
JITEXPORT JIT_Result 
JIT_compile_method_with_params(JIT_Handle jit, Compile_Handle compilation,
                               Method_Handle method_handle,
                               OpenMethodExecutionParams compilation_params)
{
    JIT_Flags flags;
    MemoryManager memManager(method_get_byte_code_size(method_handle)*
                             ESTIMATED_MEMORY_PER_BYTECODE, 
                             "JIT_compile_method.memManager");
    JIT_Handle jitHandle = method_get_JIT_id(compilation);
    JITModeData* modeData = Jitrino::getJITModeData(jitHandle);
    assert(modeData != NULL);
    flags.opt_level = 0;
    flags.insert_write_barriers = 
                                compilation_params.exe_insert_write_barriers;
    
#ifdef USE_FAST_PATH
    if (isJET(jit)){	
        return Jet::compile_with_params(jitHandle, compilation, method_handle,
                                        compilation_params);
    }
#endif	// USE_FAST_PATH

    DrlVMCompilationInterface  compilationInterface(compilation,
                                                    method_handle,
                                                    memManager, modeData,
                                                    flags);
    CompilationContext cs(memManager, &compilationInterface, modeData);
    compilationInterface.setCompilationContext(&cs);
    bool success = Jitrino::CompileMethod(&compilationInterface);
    return success ? JIT_SUCCESS : JIT_FAILURE;
}

extern "C" 
JITEXPORT OpenMethodExecutionParams JIT_get_exe_capabilities (JIT_Handle jit)
{
#ifdef USE_FAST_PATH
    if (isJET(jit)) {
        return Jet::get_exe_capabilities();
    }
#endif	// USE_FAST_PATH
    
    static const OpenMethodExecutionParams compilation_capabilities = {
        false, // exe_notify_method_entry
        false, // exe_notify_method_exit
        false, // exe_notify_instance_field_read 
        false, // exe_notify_instance_field_write
        false, // exe_notify_static_field_read
        false, // exe_notify_static_field_write
        false, // exe_notify_exception_throw
        false, // exe_notify_exception_catch
        false, // exe_notify_monitor_enter
        false, // exe_notify_monitor_exit
        false, // exe_notify_contended_monitor_enter
        false, // exe_notify_contended_monitor_exit
        false, // exe_do_method_inlining
        false, // exe_do_code_mapping
        false, // exe_do_local_var_mapping
        false, // exe_insert_write_barriers
    };        
    return compilation_capabilities;
}

extern "C"
JITEXPORT void
JIT_unwind_stack_frame(JIT_Handle jit, Method_Handle method,
                       ::JitFrameContext *context)
{
#ifdef _DEBUG
    if(Log::cat_rt()->isInfo2Enabled())
        Log::cat_rt()->out() << "UNWIND_STACK_FRAME(" <<
        class_get_name(method_get_class(method)) << "." <<
        method_get_name(method) << ")" << ::std::endl;
#endif

#ifdef USE_FAST_PATH
    if (isJET(jit)) {
	    Jet::rt_unwind(jit, method, context);
        return;
    }
#endif
    DrlVMMethodDesc methodDesc(method, jit);
    Jitrino::UnwindStack(&methodDesc, context, context->is_ip_past == FALSE);
}

extern "C"
JITEXPORT void 
JIT_get_root_set_from_stack_frame(JIT_Handle jit, Method_Handle method,
                                  GC_Enumeration_Handle enum_handle,
                                  ::JitFrameContext *context)
{
#ifdef _DEBUG
    if(Log::cat_rt()->isInfo2Enabled())
        Log::cat_rt()->out() << "GET_ROOT_SET_FROM_STACK_FRAME(" <<
        class_get_name(method_get_class(method)) << "." <<
        method_get_name(method) << ")" << ::std::endl;
#endif

#ifdef USE_FAST_PATH
    if (isJET(jit)) {
	    Jet::rt_enum(jit, method, enum_handle, context);
		return;
	}
#endif

    DrlVMMethodDesc methodDesc(method, jit);
    DrlVMGCInterface gcInterface(enum_handle);
    Jitrino::GetGCRootSet(&methodDesc, &gcInterface, context,
                          context->is_ip_past == FALSE);
}

extern "C"
JITEXPORT uint32 
JIT_get_inline_depth(JIT_Handle jit, InlineInfoPtr ptr, uint32 offset)
{
    if (Log::cat_rt()->isInfo2Enabled()) {
        Log::cat_rt()->out() << "GET_INLINE_DEPTH()" << ::std::endl;
    }
    return Jitrino::GetInlineDepth(ptr, offset);
}

extern "C"
JITEXPORT Method_Handle
JIT_get_inlined_method(JIT_Handle jit, InlineInfoPtr ptr, uint32 offset,
                       uint32 inline_depth)
{
    if (Log::cat_rt()->isInfo2Enabled()) {
        Log::cat_rt()->out() << "GET_INLINED_METHOD()" << ::std::endl;
    }
    return Jitrino::GetInlinedMethod(ptr, offset, inline_depth);
}

extern "C"
JITEXPORT Boolean
JIT_can_enumerate(JIT_Handle jit, Method_Handle method, NativeCodePtr eip)
{
    DrlVMMethodDesc methodDesc(method, jit);
    bool result = Jitrino::CanEnumerate(&methodDesc, eip);
    return (result ? TRUE : FALSE);
}


extern "C"
JITEXPORT unsigned
JIT_num_breakpoints(JIT_Handle jit, Method_Handle method, uint32 eip)
{
    assert(0);
    return 0;
}

extern "C"
JITEXPORT void
JIT_get_breakpoints(JIT_Handle jit, Method_Handle method, uint32 *bp,
                    ::JitFrameContext *context)
{
    assert(0);
}

extern "C"
JITEXPORT void
JIT_fix_handler_context(JIT_Handle jit, Method_Handle method,
                        ::JitFrameContext *context)
{
#ifdef _DEBUG
    if(Log::cat_rt()->isInfo2Enabled())
        Log::cat_rt()->out() << "FIX_HANDLER_CONTEXT(" <<
        class_get_name(method_get_class(method)) << "."
        << method_get_name(method) << ")" << ::std::endl;
#endif

#ifdef USE_FAST_PATH
    if (isJET(jit)) {
	    Jet::rt_fix_handler_context(jit, method, context);
		return;
	}
#endif

    DrlVMMethodDesc methodDesc(method, jit);
    Jitrino::FixHandlerContext(&methodDesc, context,
                               context->is_ip_past == FALSE);
}

extern "C"
JITEXPORT void *
JIT_get_address_of_this(JIT_Handle jit, Method_Handle method,
                        const ::JitFrameContext   *context)
{
#ifdef USE_FAST_PATH
    if (isJET(jit)) {
	    return Jet::rt_get_address_of_this(jit, method, context);
    }
#endif
    DrlVMMethodDesc methodDesc(method, jit);
    return Jitrino::GetAddressOfThis(&methodDesc, context,
                                     context->is_ip_past == FALSE);
}

extern "C"
JITEXPORT Boolean
JIT_call_returns_a_reference(JIT_Handle jit, Method_Handle method,
                             const ::JitFrameContext *context)
{
    assert(0);
    return false;
}

extern "C"
JITEXPORT int32 
JIT_get_break_point_offset(JIT_Handle jit, Compile_Handle compilation,
                           Method_Handle meth, JIT_Flags flags,
                           unsigned bc_location)
{
    assert(0);
    return false;
}

extern "C"
JITEXPORT void * 
JIT_get_address_of_var(JIT_Handle jit, ::JitFrameContext *context,
                       Boolean is_first, unsigned var_no)
{
    assert(0);
    return NULL;
}

extern "C"
JITEXPORT void
JIT_init_with_data(JIT_Handle jit, void *jit_data)
{
    // Ignore the message contained in jit_data
}

extern "C"
JITEXPORT Boolean
JIT_supports_compressed_references(JIT_Handle jit)
{
#ifdef USE_FAST_PATH
    if (isJET(jit)) {
        return false;
    }
#endif
    DrlVMDataInterface dataIntf;
    if (dataIntf.areReferencesCompressed())
        return true;
    else
        return false;
}

extern "C" 
JITEXPORT void
JIT_get_root_set_for_thread_dump(JIT_Handle jit, Method_Handle method,
                                 GC_Enumeration_Handle enum_handle,
                                 ::JitFrameContext *context)
{
    if(Log::cat_rt()->isInfo2Enabled()) {
        Log::cat_rt()->out() << "GET_ROOT_SET_FROM_STACK_FRAME(" << 
        class_get_name(method_get_class(method)) << "." << 
        method_get_name(method) << ")" << ::std::endl;
    }
    DrlVMMethodDesc methodDesc(method, jit);
    DrlVMThreadDumpEnumerator gcInterface;
    Jitrino::GetGCRootSet(&methodDesc, &gcInterface, context,
                          context->is_ip_past == FALSE);
}


////////////////////////////////////////////////////////
// JVMTI support functions.
////////////////////////////////////////////////////////

extern "C"
JITEXPORT OpenExeJpdaError
get_native_location_for_bc(JIT_Handle jit, Method_Handle method, 
                           uint16  bc_pc, NativeCodePtr  *native_pc)
{
#ifdef USE_FAST_PATH
    if (isJET(jit)) {
        Jet::rt_bc2native(jit, method, bc_pc, native_pc);
        return EXE_ERROR_NONE;
    }
#endif

    DrlVMMethodDesc methDesc(method, jit);
    uint64* ncAddr = (uint64*) native_pc;

    if (Jitrino::GetNativeLocationForBc(&methDesc, bc_pc, (uint64*)ncAddr)) {
        return EXE_ERROR_NONE;
    }
    return EXE_ERROR_UNSUPPORTED;
}

extern "C"
JITEXPORT OpenExeJpdaError
get_bc_location_for_native(JIT_Handle jit, Method_Handle method,
                           NativeCodePtr native_pc, uint16 *bc_pc)
{
#ifdef USE_FAST_PATH
    if (isJET(jit)) {
        Jet::rt_native2bc(jit, method, native_pc, bc_pc);
        return EXE_ERROR_NONE;
    }
#endif

    DrlVMMethodDesc methDesc(method, jit);
    POINTER_SIZE_INT ncAddr = (POINTER_SIZE_INT) native_pc;
    if (Jitrino::GetBcLocationForNative(&methDesc, (uint64)ncAddr, bc_pc)) {
        return EXE_ERROR_NONE;
    }
    return EXE_ERROR_INVALID_LOCATION;
}

extern "C"
JITEXPORT ::OpenExeJpdaError
get_local_var(JIT_Handle jit, Method_Handle method,
              const ::JitFrameContext *context, uint16  var_num,
              VM_Data_Type var_type, void *value_ptr)
{
#ifdef USE_FAST_PATH
    if (isJET(jit)) {
        return Jet::rt_get_local_var(jit, method, context, var_num, var_type,
                                     value_ptr);
    }
#endif
    return EXE_ERROR_UNSUPPORTED;
}

extern "C"
JITEXPORT ::OpenExeJpdaError
set_local_var(JIT_Handle jit, Method_Handle method,
              const ::JitFrameContext *context, uint16 var_num,
              VM_Data_Type var_type, void *value_ptr)
{
#ifdef USE_FAST_PATH
    if (isJET(jit)) {
        return Jet::rt_set_local_var(jit, method, context, var_num, var_type,
                                     value_ptr);
    }
#endif
    return EXE_ERROR_UNSUPPORTED;
}

} //namespace Jitrino

