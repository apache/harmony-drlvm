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
 * @author Intel, Alexei Fedotov
 * @version $Revision: 1.1.2.1.4.4 $
 */  


//
// These are the functions that a JIT built as a DLL must export.
// Some functions may be optional and are marked as such.
//

#ifndef _JIT_EXPORT_H
#define _JIT_EXPORT_H


#include "open/types.h"
#include "open/ee_em_intf.h"

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus


typedef void *Compile_Handle;

#include "jit_export_rt.h"

////////////////////////////////////////////////////////
// Optional functions that don't have to be provided.
////////////////////////////////////////////////////////

JITEXPORT void JIT_next_command_line_argument(JIT_Handle jit, const char *name, const char *arg);


////////////////////////////////////////////////////////
// Required functions.
////////////////////////////////////////////////////////

//
// Flags passed from the VM to the JIT.
//
// Max 32 bits, so that it fits in one word.
typedef
struct JIT_Flags {

    // The JIT should generate write barriers.
    Boolean insert_write_barriers : 1;

    // out of date flags -> to be removed
    unsigned    opt_level : 4;
    Boolean     dynamic_profile : 1;


} JIT_Flags; //JIT_Flags

/**
* Elements of this struct correspond to certain requirements
* of how a managed method is executed (what it additionally does
* during execution). Most of them correspond to requirements to
* call a certain VM helpers at certain places in the code. For JIT,
* in particular, this means that it will have to generate additional
* code which will perform these calls.
* <p>
* Each of the requirement is associated with a corresponding ability of
* the EE to satisfy this requirement. So, elements of the struct should also
* be used to denote EE capabilities related to method execution.
* <p>
* If an element corresponds to a certain VM helper, concrete contract
* of calling this helper (arguments, etc.) can be found at the place of
* definition of this helper (or its ID) within present OPEN specification.
*/
typedef struct OpenMethodExecutionParams {
    /** call corresponding VM helper upon entry to the managed method */
    Boolean  exe_notify_method_entry : 1;

    /** call corresponding VM helper upon exit from the managed method */
    Boolean  exe_notify_method_exit : 1;

    /** call corresponding VM helper upon reading a value of a field which has <field access mask> set */
    Boolean  exe_notify_field_access  : 1;

    /** call corresponding VM helper upon setting a value of a field which has <field modification mask> set */
    Boolean  exe_notify_field_modification : 1;

    /**
    * call corresponding VM helper upon exception throw,
    * if by default the throw code does not enter any VM helper
    * (for example, in case of JIT optimizations)
    */
    Boolean  exe_notify_exception_throw : 1;

    /**
    * call corresponding VM helper upon exception catch,
    * if by default the exception propagation code does not enter any VM helper
    * (for example, in case of JIT optimizations)
    */
    Boolean  exe_notify_exception_catch : 1;

    /**
    * call corresponding VM helper upon entering a monitor,
    * if by default the monitor enter code does not enter any VM helper
    * (for example, in case of JIT optimizations)
    */
    Boolean  exe_notify_monitor_enter : 1;

    /**
    * call corresponding VM helper upon exiting a monitor,
    * if by default the monitor exit code does not enter any VM helper
    * (for example, in case of JIT optimizations)
    */
    Boolean  exe_notify_monitor_exit : 1;

    /**
    * call corresponding VM helper upon entering a contended monitor,
    * if by default the contended monitor enter code does not enter any VM helper
    * (for example, in case of JIT optimizations)
    */
    Boolean  exe_notify_contended_monitor_enter : 1;

    /**
    * call corresponding VM helper upon exiting a contended monitor,
    * if by default the contended monitor exit code does not enter any VM helper
    * (for example, in case of JIT optimizations)
    */
    Boolean  exe_notify_contended_monitor_exit : 1;

    /** perform method in-lining during compilation (JIT-specific) */
    Boolean  exe_do_method_inlining : 1;

    /**
    * Keep correspondence between bytecode offsets and native instruction IPs (JIT-specific).
    * For a JIT this, in particular, means that it should not do any optimizations which
    * may hinder this mapping. It should also store the map after method compilation so that
    * later VM could use appropriate ExeJPDA interfaces to retrieve the mapping.
    */
    Boolean  exe_do_code_mapping : 1;

    /**
    * Keep correspondence between bytecode local variables and locations of the
    * native operands (JIT-specific) in relevant locations within method code.
    * For a JIT this, in particular, means that it should not do any optimizations
    * which may hinder this mapping. It should also store the map after method compilation
    * so that later VM could use appropriate ExeJPDA interfaces to retrieve the mapping.
    */
    Boolean  exe_do_local_var_mapping : 1;

    /** call corresponding VM helper upon setting a value of any field of reference type */
    Boolean  exe_insert_write_barriers : 1;

   /**
    * Provide possibility to obtain reference to the current 'this' object by
    * means of get_address_of_this method. Used for JVMTI debug support.
    */
    Boolean  exe_provide_access_to_this : 1;

   /**
    * Provide restoring of arguments in the stack after the call
    * of the unwind_frame method so that method could be called again
    * with the same arguments. Used for JVMTI debug support.
    */
    Boolean  exe_restore_context_after_unwind : 1;

    /**
    * Sent CompileMethodLoad event when a method is compiled and loaded into memory 
    */
    Boolean  exe_notify_compiled_method_load : 1;

} OpenMethodExecutionParams;





// This function is deprecated.
JITEXPORT JIT_Result 
JIT_gen_method_info(JIT_Handle jit, 
                    Compile_Handle     compilation,
                    Method_Handle      method,
                    JIT_Flags          flags
                    );

JITEXPORT JIT_Result 
JIT_compile_method(JIT_Handle jit,
                   Compile_Handle     compilation, 
                   Method_Handle      method,
                   JIT_Flags          flags
                   );

/** 
    * Performs compilation of given method.
    *
    * @param method_handle      - handle of the method to be compiled
    * @param compilation_params - compilation parameters. If NULL, default compilation parameters
    *     should be used by the JIT (passed in the initialize function). If either of parameters is
    *     not supported by the JIT, the function should return compilation failure.
    * @return compilation status
    */
JITEXPORT JIT_Result JIT_compile_method_with_params(
    JIT_Handle jit,
    Compile_Handle     compile_handle, 
    Method_Handle       method_handle,
    OpenMethodExecutionParams compilation_params
    );


/**
    * Retrieves method execution-related capabilities supported by the EE.
    *
    * @return the set of supported capabilities
    */
JITEXPORT OpenMethodExecutionParams JIT_get_exe_capabilities (JIT_Handle jit);


#ifdef __cplusplus
}
#endif // __cplusplus


#endif // _JIT_EXPORT_H
