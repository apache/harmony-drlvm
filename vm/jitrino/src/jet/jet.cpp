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
 * @author Alexander V. Astapchuk
 * @version $Revision: 1.7.12.3.4.4 $
 */
#include "jet.h"
#include "compiler.h"
#include "stats.h"

#include <jit_export_jpda.h>

#include <assert.h>

/**
 * @file
 * @brief Main interface provided by Jitrino.JET.
 */
 
/**
 * @mainpage
 * @section sec_intro Introduction
 * Jitrino.Jet: Jitrino's Express compilation paTh
 * 
 * Jitrino.JET is a simble baseline compiler for Jitrino JIT.
 *
 * The main goal is fast compilation (to provide quick start for client
 * applications) and to produce neccessary support for optimising engine of
 * mian Jitrino (instrumentation, profile generation, etc.).
 *
 */

namespace Jitrino {
namespace Jet {

void setup(JIT_Handle jit)
{
#if defined(_DEBUG) && defined(JET_PROTO)
//    shared_jit_handle = jit;
#endif
    Timer::init();
    Timers::totalExec.start();
}

void cleanup(void)
{
    Timers::totalExec.stop();
    //Timer::dump();
    //Stats::dump();
}

void get_id_string(char * buf, unsigned len)
{
    const char revision[] = "$Revision: 1.7.12.3.4.4 $";
    const char branch[] = "$Name:  $";

#ifdef PROJECT_JET
    #define ALONE_STR   ", alone"
#else
    #define ALONE_STR   ""
#endif

#ifdef _DEBUG
    #define DBG_STR ", dbg"
#else
    #define DBG_STR ""
#endif

#ifdef __INTEL_COMPILER
    #define COMP_STR    ", icl"
#else
    #define COMP_STR    ""
#endif

    char revision_buf[80] = {0}, branch_buf[80] = {0};
    if (revision[0] != '$') {
        snprintf(revision_buf, sizeof(revision_buf)-1, " Rev.: %s.",
                 revision);
    }
    if (branch[0] != '$') {
        snprintf(branch_buf, sizeof(branch_buf)-1, " Br.: %s.",
                 branch);
    }

    snprintf(buf, len, 
        "Jitrino.JET" DBG_STR COMP_STR ALONE_STR ": "
        "Built: " __DATE__ " " __TIME__ 
        ".%s%s", branch_buf, revision_buf);
}

static bool id_done = false;

void cmd_line_arg(JIT_Handle jit, const char* name, const char* arg)
{
    static const char help_data[] = {
"                                                                         \n"
"  jet::help                - prints out this text                        \n"
"  jet::info, jet::id       - prints out build info                       \n"
"  jet::lazyres, jet::lr    - generate code with lazy (runtime) resolution\n"
#ifdef JET_PROTO
"  jet::stackalign, jet::sa - generate methods with stack alignment       \n"
#endif
"  jet::no_bbpooling, jet::no_bbp - do not generate back branches pooling \n"
"                             code                                        \n"
    };
    
    const char
        key_invalid[] = "jet::",
        key_help[] = "jet::help",
        key_info[] = "jet::info",
        key_id[] = "jet::id",
        key_lazyres[] = "jet::lazyres", key_lr[] = "jet::lr",
#ifdef JET_PROTO
        key_stackalign[] = "jet::stackalign", key_sa[] = "jet::sa",
#endif
        key_no_bbpooling[] = "jet::no_bbpooling", key_no_bbp[] = "jet::no_bbp";

    if (!strncmp(arg, key_help, sizeof(key_help)-1)) {
		if (!id_done) {
	        char buf[81];
    	    get_id_string(buf, sizeof(buf)-1);
	        puts(buf);
			id_done = true;
		}
        puts(help_data);
    }
    else if (!strncmp(arg, key_id, sizeof(key_id)-1) || 
             !strncmp(arg, key_info, sizeof(key_info)-1)) {
		if (!id_done) {
	        char buf[81];
    	    get_id_string(buf, sizeof(buf)-1);
	        puts(buf);
			id_done = true;
		}
    }
    else if (!strncmp(arg, key_lazyres, sizeof(key_lazyres)-1) || 
             !strncmp(arg, key_lr, sizeof(key_lr)-1)) {
        Compiler::defaultFlags |= JMF_LAZY_RESOLUTION;
    }
#ifdef JET_PROTO
    else if (!strncmp(arg, key_stackalign, sizeof(key_stackalign)-1) || 
             !strncmp(arg, key_sa, sizeof(key_sa)-1)) {
        Compiler::defaultFlags |= JMF_ALIGN_STACK;
    }
#endif
    else if (!strncmp(arg, key_no_bbpooling, sizeof(key_no_bbpooling)-1) || 
             !strncmp(arg, key_no_bbp, sizeof(key_no_bbp)-1)) {
        Compiler::defaultFlags &= ~JMF_BBPOOLING;
    }
    else if (!strncmp(arg, key_invalid, sizeof(key_invalid)-1)) {
        printf("Warning: unknown flag - %s\n", arg);
    }
}

OpenMethodExecutionParams get_exe_capabilities()
{
    static const OpenMethodExecutionParams supported = {
        true,  // exe_notify_method_entry
        true,  // exe_notify_method_exit
        
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
        
        true,  // exe_do_code_mapping
        true,  // exe_do_local_var_mapping
        
        false, // exe_insert_write_barriers
    };
    return supported;
}

JIT_Result compile(JIT_Handle jitHandle, Compile_Handle ch, Method_Handle method, 
                   JIT_Flags flags)
{
    static const OpenMethodExecutionParams compileArgs = {
        false,  // exe_notify_method_entry
        false,  // exe_notify_method_exit
        
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
        
        true,  // exe_do_code_mapping
        true,  // exe_do_local_var_mapping
        
        false, // exe_insert_write_barriers
    };

    ::Jitrino::Jet::Compiler jit(jitHandle, compileArgs);
    return jit.compile(ch, method);
}

JIT_Result compile_with_params(JIT_Handle jit_handle, Compile_Handle ch, 
                               Method_Handle method, 
                               OpenMethodExecutionParams params)
{
    ::Jitrino::Jet::Compiler jit(jit_handle, params);
    return jit.compile(ch, method);
}


}}; // ~ namespace Jitrino::Jet

// standalone interface ?
#ifdef PROJECT_JET

/**
 * @defgroup JITRINO_JET_STANDALONE Standalone interface
 * 
 * Jitrino.JET may be compiled as a small separate JIT, mostly for 
 * debugging/testing purposes.
 *
 * To do this, a preprocessor macro PROJECT_JET must be defined. Also, some 
 * files are used from src/share - namely, mkernel.* and PlatformDependant.h.
 * 
 * These exported functions represent inetrface required to interact with VM.
 *
 * @{
 */

/**
 * @see setup
 */
extern "C" JITEXPORT
void JIT_init(JIT_Handle jit, const char* name) {
    Jitrino::Jet::setup(jit);
}

/**
 * @see cleanup
 */
extern "C" JITEXPORT
void JIT_deinit(JIT_Handle jit) {
    Jitrino::Jet::cleanup();
}

/**
 * @see cmd_line_arg
 */
extern "C" JITEXPORT
void JIT_next_command_line_argument(JIT_Handle jit, const char *name, const char *arg) {
    Jitrino::Jet::cmd_line_arg(jit, name, arg);
}

/**
 * Noop.
 */
extern "C" JITEXPORT
void JIT_init_with_data(JIT_Handle jit, void *jit_data)
{
    // noop
}

/**
 * @see get_exe_capabilities
 */
extern "C" JITEXPORT
OpenMethodExecutionParams JIT_get_exe_capabilities(JIT_Handle jit)
{
    return Jitrino::Jet::get_exe_capabilities();
}

extern "C" JITEXPORT
JIT_Result JIT_gen_method_info(JIT_Handle jit, Compile_Handle compilation,
                               Method_Handle method, JIT_Flags flags)
{
    assert(0);
    return JIT_FAILURE;
}


extern "C" JITEXPORT
JIT_Result JIT_compile_method(JIT_Handle jit, Compile_Handle ch,
                              Method_Handle method,
                              JIT_Flags flags)
{
    return Jitrino::Jet::compile(jit, ch, method, flags);
}

extern "C" JITEXPORT 
JIT_Result JIT_compile_method_with_params(JIT_Handle hjit, 
                                          Compile_Handle compilation, 
                                          Method_Handle method, 
                                          OpenMethodExecutionParams params)
{
    ::Jitrino::Jet::Compiler jit(hjit, params);
    JIT_Flags flags;
    flags.insert_write_barriers = false;
    return jit.compile(compilation, method);
}


/**
 * Noop in Jitrino.JET.
 * @return FALSE
 */
extern "C" JITEXPORT Boolean JIT_recompiled_method_callback(
        JIT_Handle jit, Method_Handle  method, void * callback_data)
{
    return FALSE;
}

extern "C" JITEXPORT
Boolean JIT_extended_class_callback(JIT_Handle jit,
                                    Class_Handle extended_class,
                                    Class_Handle new_class,
                                    void *callback_data)
{
    return FALSE;
}

extern "C" JITEXPORT
Boolean JIT_overridden_method_callback(JIT_Handle jit,
                                       Method_Handle overridden_method,
                                       Method_Handle new_method,
                                       void *callback_data)
{
    return FALSE;
}

/**
 * @see rt_unwind
 */
extern "C" JITEXPORT
void JIT_unwind_stack_frame(JIT_Handle jit, Method_Handle method,
                            ::JitFrameContext *context)
{
    Jitrino::Jet::rt_unwind(jit, method, context);
}

/**
 * @see rt_enum
 */
extern "C" JITEXPORT
void JIT_get_root_set_from_stack_frame(JIT_Handle jit, Method_Handle method,
                                       GC_Enumeration_Handle henum,
                                       ::JitFrameContext *context)
{
    Jitrino::Jet::rt_enum(jit, method, henum, context);
}

/**
 * @see rt_fix_handler_context
 */
extern "C" JITEXPORT
void JIT_fix_handler_context(JIT_Handle jit, Method_Handle method,
                             ::JitFrameContext   *context)
{
    Jitrino::Jet::rt_fix_handler_context(jit, method, context);
}

/**
 * @see rt_get_address_of_this
 */
extern "C" JITEXPORT
void * JIT_get_address_of_this(JIT_Handle jit, Method_Handle method,
                               const ::JitFrameContext *context)
{
    return Jitrino::Jet::rt_get_address_of_this(jit, method, context);
}

/**
 * Inlining is unsupported by Jitrino.JET.
 * @return 0
 */
extern "C" JITEXPORT
uint32 JIT_get_inline_depth(JIT_Handle jit, InlineInfoPtr ptr, uint32 offset)
{
    return 0;
}

/**
 * Inlining is unsupported by Jitrino.JET.
 * @return 0
 */
extern "C" JITEXPORT
Method_Handle JIT_get_inlined_method(JIT_Handle jit, InlineInfoPtr ptr,
                                     uint32 offset, uint32 inline_depth)
{
    return 0;
}

extern "C" JITEXPORT
Boolean JIT_can_enumerate(JIT_Handle jit, Method_Handle method,
                          NativeCodePtr eip)
{
    return false;
}


extern "C" JITEXPORT
unsigned JIT_num_breakpoints(JIT_Handle jit, Method_Handle method, uint32 eip)
{
    assert(false);
    return 0;
}

extern "C" JITEXPORT
void JIT_get_breakpoints(JIT_Handle jit, Method_Handle method, uint32 *bp,
                         ::JitFrameContext *context)
{
    assert(false);
}

/**
 * @deprecated
 */
extern "C" JITEXPORT
Boolean JIT_call_returns_a_reference(JIT_Handle jit, Method_Handle method,
                                     const ::JitFrameContext *context)
{
    assert(false);
    return false;
}

extern "C" JITEXPORT
int32 JIT_get_break_point_offset(JIT_Handle jit, Compile_Handle compilation,
                                 Method_Handle meth, JIT_Flags flags,
                                 unsigned bc_location)
{
    assert(false);
    return false;
}

extern "C" JITEXPORT
void * JIT_get_address_of_var(JIT_Handle jit, ::JitFrameContext *context,
                              Boolean is_first, unsigned var_no)
{
    assert(false);
    return NULL;
}

/**
 * @returns \b false
 */
extern "C" JITEXPORT
Boolean JIT_supports_compressed_references(JIT_Handle jit)
{
    return false;
}


/**
 * @todo need to be implemented
 */
extern "C" JITEXPORT
void JIT_get_root_set_for_thread_dump(JIT_Handle jit, Method_Handle method,
                                      GC_Enumeration_Handle henum,
                                      ::JitFrameContext *context )
{
    assert(false);
}


//**********************************************************************
//* exported routines for JVMTI
//**********************************************************************

extern "C" JITEXPORT
OpenExeJpdaError get_native_location_for_bc(JIT_Handle jit, 
                                            Method_Handle method, 
                                            uint16  bc_pc, 
                                            NativeCodePtr *pnative_pc)
{
    Jitrino::Jet::rt_bc2native(jit, method, bc_pc, pnative_pc);
    return EXE_ERROR_NONE;
}

extern "C" JITEXPORT
OpenExeJpdaError get_bc_location_for_native(JIT_Handle jit,
                                            Method_Handle method,
                                            NativeCodePtr native_pc,
                                            uint16 *pbc_pc)
{
    Jitrino::Jet::rt_native2bc(jit, method, native_pc, pbc_pc);
    return EXE_ERROR_NONE;
}

extern "C" JITEXPORT
OpenExeJpdaError get_local_var(JIT_Handle jit,
                               Method_Handle method,
                               const ::JitFrameContext *context,
                               uint16  var_num,
                               VM_Data_Type var_type,
                               void *value_ptr)
{
    return Jitrino::Jet::rt_get_local_var(jit, method, context,
                                          (unsigned)var_num, var_type,
                                          value_ptr);
}

extern "C" JITEXPORT
OpenExeJpdaError set_local_var(JIT_Handle jit,
                               Method_Handle method,
                               const ::JitFrameContext *context,
                               uint16 var_num, VM_Data_Type var_type,
                               void *value_ptr)
{
    return Jitrino::Jet::rt_set_local_var(jit, method, context,
                                          (unsigned)var_num, var_type,
                                          value_ptr);
}

/// @} // ~ defgroup JITRINO_JET_STANDALONE


#endif	// ~ifdef PROJECT_JET	// standalone interface

