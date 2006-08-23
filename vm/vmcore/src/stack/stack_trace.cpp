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
 * @author Intel, Pavel Afremov
 * @version $Revision: 1.1.2.1.4.3 $
 */  


#include "m2n.h"
#include "stack_iterator.h"
#include "stack_trace.h"
#include "interpreter.h"
#include "jit_intf_cpp.h"

#include "method_lookup.h"

Method_Handle get_method(StackIterator* si)
{
    ASSERT_NO_INTERPRETER
    CodeChunkInfo* cci = si_get_code_chunk_info(si);
    if (cci)
        return cci->get_method();
    else
        return m2n_get_method(si_get_m2n(si));
}

uint32 si_get_inline_depth(StackIterator* si)
{
    //
    // Here we assume that JIT data blocks can store only InlineInfo
    // A better idea is to extend JIT_Data_Block with some type information
    // Example:
    //
    // enum JIT_Data_Block_Type { InlineInfo, Empty }
    //
    // struct JIT_Data_Block {
    //     JIT_Data_Block *next;
    //     JIT_Data_Block_Type type;
    //     char bytes[1];
    // };
    //
    // void *Method::allocate_JIT_data_block(size_t size, JIT *jit, JIT_Data_Block_Type)
    //

    ASSERT_NO_INTERPRETER
    CodeChunkInfo* cci = si_get_code_chunk_info(si);
    if ( cci != NULL && cci->has_inline_info()) {
        return cci->get_jit()->get_inline_depth(
                cci->get_inline_info(), 
                (POINTER_SIZE_INT)si_get_ip(si) - (POINTER_SIZE_INT)cci->get_code_block_addr());
    }
    return 0;
}

void get_file_and_line(Method_Handle mh, void *ip, const char **file, int *line) {
    Method *method = (Method*)mh;
    *file = class_get_source_file_name(method_get_class(method));

    if (method_is_native(method)) {
        *line = -2;
        return;
    }

    *line = -1;
    if (interpreter_enabled()) {
        int bc = (uint8*)ip - (uint8*)method->get_byte_code_addr();
        *line = method->get_line_number((uint16)bc);
        return;
    }

    // inlined method will not have line numbers for now
    // they are marked with zero ip address.
    if (ip == NULL) {
        return;
    }


#if !defined(_IPF_) // appropriate callLength should be put here when IPF bc maping will be implemented
    uint16 bcOffset;
    POINTER_SIZE_INT callLength = 5;

    CodeChunkInfo* jit_info = vm_methods->find((unsigned char*)ip - callLength);
    if (jit_info->get_jit()->get_bc_location_for_native(
        method,
        (NativeCodePtr) ((POINTER_SIZE_INT) ip - callLength),
        &bcOffset) != EXE_ERROR_NONE) {
        //
        return;
    }
    *line = method->get_line_number(bcOffset);
#endif        
}

unsigned st_get_depth()
{
    ASSERT_NO_INTERPRETER
    StackIterator* si = si_create_from_native();
    unsigned depth = 0;
    while (!si_is_past_end(si)) {
        if (get_method(si)) {
            depth += 1 + si_get_inline_depth(si);
        }
        si_goto_previous(si);
    }
    si_free(si);
    return depth;
}

bool st_get_frame(unsigned target_depth, StackTraceFrame* stf)
{
    if (interpreter_enabled()) {
        return interpreter.interpreter_st_get_frame(target_depth, stf);
    }

    StackIterator* si = si_create_from_native();
    unsigned depth = 0;
    while (!si_is_past_end(si)) {
        stf->method = get_method(si);
        if (stf->method) {
            uint32 inlined_depth = si_get_inline_depth(si);
            if ( (target_depth >= depth) && 
                 (target_depth <= depth + inlined_depth) ) {
                stf->ip = si_get_ip(si);

                if (target_depth < depth + inlined_depth) {
                    CodeChunkInfo* cci = si_get_code_chunk_info(si);
                    uint32 offset = (POINTER_SIZE_INT)stf->ip - (POINTER_SIZE_INT)cci->get_code_block_addr();
                    stf->method = cci->get_jit()->get_inlined_method(
                            cci->get_inline_info(), offset, target_depth - depth);
                }

                si_free(si);
                return true;
            }
            depth += inlined_depth + 1;
        }
        si_goto_previous(si);
    }
    si_free(si);
    return false;
}

VMEXPORT StackTraceFrame* 
st_alloc_frames(int num) {
    return (StackTraceFrame*) STD_MALLOC(sizeof(StackTraceFrame)*num);
}

static inline void *get_this(JIT *jit, Method *method, StackIterator *si) {
    if (method->is_static()) return 0;
    void **address_of_this = (void**)jit->get_address_of_this(method, si_get_jit_context(si));
    if (address_of_this) {
        return *address_of_this;
    }
    return NULL;
}

void st_get_trace(unsigned* res_depth, StackTraceFrame** stfs)
{
    tmn_suspend_disable();
    if (interpreter_enabled()) {
        interpreter.interpreter_st_get_trace(res_depth, stfs);
        tmn_suspend_enable();
        return;
    }

    unsigned depth = st_get_depth();
    StackTraceFrame* stf = st_alloc_frames(depth);
    assert(stf);
    *res_depth = depth;
    *stfs = stf;
    StackIterator* si = si_create_from_native();

    depth = 0;
    while (!si_is_past_end(si)) {
        Method_Handle method = get_method(si);

        if (method) {
            NativeCodePtr ip = si_get_ip(si);
            CodeChunkInfo* cci = si_get_code_chunk_info(si);
            if ( cci == NULL ) {
                stf->outdated_this = 0;
            } else {
                JIT *jit = cci->get_jit();
                uint32 inlined_depth = si_get_inline_depth(si);
                uint32 offset = (POINTER_SIZE_INT)ip - (POINTER_SIZE_INT)cci->get_code_block_addr();

                for (uint32 i = 0; i < inlined_depth; i++) {
                    stf->method = jit->get_inlined_method(cci->get_inline_info(), offset, i);
                    stf->ip = NULL;
                    stf->outdated_this = get_this(jit, method, si);
                    stf++;
                    depth++;
                }
                stf->outdated_this = get_this(jit, method, si);
            }
            stf->method = method;
            stf->ip = ip;
            stf++;
            depth++;
        }
        si_goto_previous(si);
    }
    assert(depth==*res_depth);
    si_free(si);
    tmn_suspend_enable();
}

void st_print_frame(ExpandableMemBlock* buf, StackTraceFrame* stf)
{
    const char* cname = class_get_name(method_get_class(stf->method));
    const char* mname = method_get_name(stf->method);
    const char* dname = method_get_descriptor(stf->method);
    buf->AppendFormatBlock("\tat %s.%s%s", cname, mname, dname);
    const char *file;
    int line;
    get_file_and_line(stf->method, stf->ip, &file, &line);

    if (line==-2)
        // Native method
        buf->AppendBlock(" (Native Method)");
    else if (file)
        if (line==-1)
            buf->AppendFormatBlock(" (%s)", file);
        else
            buf->AppendFormatBlock(" (%s:%d)", file, line);
    else if (stf->ip)
        buf->AppendFormatBlock(" (ip=%p)", stf->ip);
    buf->AppendBlock("\n");
}

void st_print(FILE* f)
{
    fprintf(f, "Stack Trace (%p):\n", p_TLS_vmthread);
    StackIterator* si = si_create_from_native();
    unsigned depth = 0;
    while (!si_is_past_end(si)) {
        fprintf(f, "  [%p] %p(%c): ", p_TLS_vmthread, si_get_ip(si), (si_is_native(si) ? 'n' : 'm'));
        Method_Handle m = get_method(si);

        if (m) {
            CodeChunkInfo* cci = si_get_code_chunk_info(si);
            if ( cci != NULL ) {
                uint32 inlined_depth = si_get_inline_depth(si);
                uint32 offset = (POINTER_SIZE_INT)si_get_ip(si) - (POINTER_SIZE_INT)cci->get_code_block_addr();
                
                for (uint32 i = 0; i < inlined_depth; i++) {
                    Method *real_method = cci->get_jit()->get_inlined_method(cci->get_inline_info(), offset, i);
                    fprintf(f, "%s.%s%s\n", class_get_name(method_get_class(real_method)), 
                            method_get_name(real_method), method_get_descriptor(real_method));
                    depth++;
                }
            }
            fprintf(f, "%s.%s%s\n", class_get_name(method_get_class(m)), method_get_name(m), method_get_descriptor(m));
        }else{
            fprintf(f, "*null*\n");
        }
        depth++;
        si_goto_previous(si);
    }
    si_free(si);
    fprintf(f, "End Stack Trace (%p, depth=%d)\n", p_TLS_vmthread, depth);

}

void st_print() {
    st_print(stderr);
}
