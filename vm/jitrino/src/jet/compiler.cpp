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
 * @version $Revision: 1.7.12.4.4.5 $
 */
 
/**
 * @file
 * @brief Platform independent part of Compiler methods, mostly related to 
 *        basic blocks processing.
 */

#include <assert.h>
#include <algorithm>
#include <malloc.h>

#include "open/vm.h"
#include "jit_import.h"
#include "jit_runtime_support.h"
#include "jit_intf.h"

#include "../shared/mkernel.h"

#include "compiler.h"
#include "trace.h"
#include "stats.h"


#if !defined(PROJECT_JET)
    #include "Jitrino.h"
    #include "VMInterface.h"
    #include "DrlVMInterface.h"
#else
    /**
     * See DrlVMInterface.h/g_compileLock and 
     * DrlCompilationInterface::lockMethodData/unlockMethodData for details.
     */
    static Jitrino::Mutex g_compileLock;
#endif

namespace Jitrino {
namespace Jet {

#if defined(JET_PROTO)

// Only used for compilation's stress testing/performance tuning
static JIT_Handle shared_jit_handle = 0;

// Only used for compilation's stress testing/performance tuning
extern "C" __declspec(dllexport) void * get_jet_jit(void) {
    return shared_jit_handle;
}

#endif

const int       Compiler::g_iconst_m1 = -1;
const int       Compiler::g_iconst_0 = 0;
const int       Compiler::g_iconst_1 = 1;

const float     Compiler::g_fconst_0 = 0.;
const float     Compiler::g_fconst_1 = 1.;
const float     Compiler::g_fconst_2 = 2.;
const double    Compiler::g_dconst_0 = 0.;
const double    Compiler::g_dconst_1 = 1.;

#if defined(JIT_STATS) || defined(JIT_LOGS) || \
    defined(JET_PROTO) || defined(_DEBUG)
unsigned Compiler::g_methodsSeen = 0;
#else
const char Compiler::m_fname[1] = {0};
#endif

unsigned Compiler::defaultFlags = JMF_BBPOOLING;

JIT_Result Compiler::compile(Compile_Handle ch, Method_Handle method)
{
    m_compileHandle = ch;
    m_method = method;
    m_klass = method_get_class(method);

#if defined(_DEBUG) || defined(JIT_STATS) || defined(JIT_LOGS)
    m_fname[0] = 0;
    const char * mname = method_get_name(method);
    const char * msig = method_get_descriptor(method); msig = msig;
    const char * kname = class_get_name(m_klass);
    snprintf(m_fname, sizeof(m_fname)-1, "%s::%s", kname, mname);
    ++g_methodsSeen;
#else
    const char * msig = ""; msig = msig;
#endif

    STATS_SET_NAME_FILER(NULL);

    unsigned compile_flags = defaultFlags; //JMF_LAZY_RESOLUTION;

    initProfilingData(&compile_flags);

#if defined(_DEBUG) || defined(JET_PROTO)
    dbg_break_pc = NOTHING;
#endif

#if defined(_DEBUG) && defined(JET_PROTO)
    compile_flags |= DBG_TRACE_SUMM; 
#endif

    if (compile_flags & DBG_TRACE_SUMM) {
        dbg_trace_comp_start();
    }

    if (NULL == rt_helper_throw) {
        // The very first call of ::compile(), initialize
        // runtime constants - addresses of helpers, offsets, etc
        initStatics();
    }

    Timers::compTotal.start();
    Timers::compInit.start();
    
    m_bc = (unsigned char*)method_get_byte_code_addr(m_method);
    unsigned bc_size = method_get_byte_code_size(m_method);
    unsigned num_locals = method_vars_get_number(m_method);
    unsigned max_stack = method_get_max_stack(m_method);

    get_args_info(m_method, m_args, &m_retType);
    unsigned num_input_slots = count_slots(m_args);

    m_java_meth_flags = method_get_flags(m_method);

    m_patchID = -1;
    if (compile_flags & JMF_LAZY_RESOLUTION) {
        m_codeStream.init((size_t)(bc_size*
                                       NATIVE_CODE_SIZE_2_BC_SIZE_RATIO_LAZY));
        m_patchItems.reserve((size_t)(bc_size*
                                            PATCH_COUNT_2_BC_SIZE_RATIO_LAZY));
    }
    else {
        m_codeStream.init((size_t)(bc_size*NATIVE_CODE_SIZE_2_BC_SIZE_RATIO));
        m_patchItems.reserve((size_t)(bc_size*PATCH_COUNT_2_BC_SIZE_RATIO));
    }

    m_stack.init(num_locals, max_stack, num_input_slots);
    // We need to report 'this' additionally for the following cases:
    // - non-static sync methods - to allow VM to call monitor_exit() for
    //      abrupt exit
    // - constructors of classes with class_hint_is_exceptiontype == true
    //      to allow correct handling of stack trace in VM (see 
    //      stack_trace.cpp + com_openintel_drl_vm_VMStack.cpp:
    //      Java_com_openintel_drl_vm_VMStack_getStackState.
    //
    if (!method_is_static(m_method)) {
        if (method_is_synchronized(m_method) ||
            (class_hint_is_exceptiontype(m_klass) &&
            !strcmp(method_get_name(m_method), "<init>"))) {
            compile_flags |= JMF_REPORT_THIS;
        }
    }
    m_infoBlock.init(bc_size, max_stack, num_locals, num_input_slots, 
                     compile_flags);

    bool eh_ok = bbs_ehandlers_resolve();    
    Timers::compInit.stop();
    
    if (!eh_ok) {
        // At least on of the exception handlers classes was not resolved:
        // unable to resolve class of Exception => will be unable to 
		// register exception handlers => can't generate code at
		// all => stop here
        // Might need/want to [re]consider: seems good for eager 
		// resolution. Might want to generate LinkageError and throw it at
		// runtime for lazy resolution.
        if (m_infoBlock.get_flags() & DBG_TRACE_SUMM) {
            dbg_trace_comp_end(false, "ehandler.resolve");
        }
    	m_infoBlock.release();
        return JIT_FAILURE;
    }
    
    //
    // Initialization done, collect statistics
    STATS_INC(Stats::methodsCompiled, 1);
    if (m_handlers.size() == 0) {
        STATS_INC(Stats::methodsWOCatchHandlers, 1);
    }
    
    //
    STATS_MEASURE_MIN_MAX_VALUE(bc_size, m_infoBlock.get_bc_size(), m_fname);
    STATS_MEASURE_MIN_MAX_VALUE(jstack, max_stack, m_fname);
    STATS_MEASURE_MIN_MAX_VALUE(locals, num_locals, m_fname);
    //
    // ~end of Stats
    //

    m_insts.alloc(bc_size);
    
    Timers::compMarkBBs.start();
        bbs_mark_all();
    Timers::compMarkBBs.stop();

    // Statistics:: number of basic blocks
    STATS_MEASURE_MIN_MAX_VALUE(bbs, m_bbs.size(), m_fname);

    if (m_infoBlock.get_flags() & DBG_DUMP_BBS) {
        dbg_dump_bbs();
    }
    
    Timers::compCodeGen.start();
        // generate code - will recursively generate all the reachable 
        // code, except the exception handlers
        bbs_gen_code(0, NULL, NOTHING);
        // generate exception handlers
        for (unsigned i=0; i<m_handlers.size(); i++) {
            bbs_gen_code(m_handlers[i].handler, NULL, NOTHING);
        }
    Timers::compCodeGen.stop();
    
    // *************
    // * LOCK HERE *
    // *************
    g_compileLock.lock();

    if (method_get_code_block_size_jit(m_method, jit_handle) != 0) {
        // the code generated already
        Stats::methodsCompiledSeveralTimes++;
        g_compileLock.unlock();
    	m_infoBlock.release();
        return JIT_SUCCESS;
    }

    Timers::compCodeLayout.start();
        const unsigned total_code_size = m_codeStream.size();
        m_vmCode = (char*)method_allocate_code_block(m_method, jit_handle,
                                                     total_code_size, 
                                                     16/*fixme aligment*/, 
                                                     CODE_BLOCK_HEAT_DEFAULT,
                                                     0, CAA_Allocate);
        bbs_layout_code();
    Timers::compCodeLayout.stop();
    
    STATS_MEASURE_MIN_MAX_VALUE(code_size, total_code_size, m_fname);
    STATS_MEASURE_MIN_MAX_VALUE(native_per_bc_ratio, 
                                m_infoBlock.get_bc_size() == 0 ? 
                                0 : total_code_size/m_infoBlock.get_bc_size(),
                                m_fname);

#ifdef _DEBUG
    // At this point, the codeStream content is completely copied into the
    // 'codeBlock', thus no usage of m_codeStream beyond this point.
    memset(m_codeStream.data(), 0xCC, m_codeStream.size());
#endif

    //
    // runtime data. must be initialized before code patching
    //
    m_infoBlock.set_num_refs(m_lazyRefs.size());
    unsigned data_size = m_infoBlock.get_total_size();
    char * pdata = (char*)method_allocate_info_block(m_method, jit_handle, 
                                                     data_size);
    m_infoBlock.save(pdata);
    //
    // Finalize addresses
    //
    Timers::compCodePatch.start();
        cg_patch_code();
    Timers::compCodePatch.stop();
    
    //
    // register exception handlers
    //
    Timers::compEhandlers.start();
        bbs_ehandlers_set();
    Timers::compEhandlers.stop();
    
    // ****************
    // * UN LOCK HERE *
    // ****************
    g_compileLock.unlock();
    
    m_infoBlock.release();
    
    Timers::compTotal.stop();
    
    STATS_MEASURE_MIN_MAX_VALUE(patchItemsToBcSizeRatioX1000, 
                                m_patchItems.size()*1000/bc_size, m_fname);
    
    
    if (m_infoBlock.get_flags() & DBG_DUMP_CODE) {
        dbg_dump_code();
    }
    if (m_infoBlock.get_flags() & DBG_TRACE_SUMM) {
        dbg_trace_comp_end(true, "ok");
    }
    return JIT_SUCCESS;
}


void Compiler::bbs_mark_all(void)
{
    unsigned pc = 0;
    // the very first instruction always start a new BB - a prolog
    bbs_mark_one(0, false, true, false, false);
    const unsigned bc_size = m_infoBlock.get_bc_size();
    do{
        JInst& jinst = m_insts[pc];
        pc = fetch(pc, jinst);
        // 'pc' points to the instructions next to jinst ...
        if (jinst.flags & OPF_ENDS_BB) {
            // ... and is just considered as BB leader
            const bool is_dead_end = jinst.flags&OPF_DEAD_END;
            bbs_mark_one(pc, false, !is_dead_end, false, false);
            bool is_jsr = jinst.opcode == OPCODE_JSR;
            for (unsigned i=0, n = jinst.get_num_targets(); i<n; i++) {
                // mark jmp target(s)
                bbs_mark_one(jinst.get_target(i), true, true, false, is_jsr);
            }
            if (jinst.is_switch()) {
                bbs_mark_one(jinst.get_def_target(), true, true, false, false);
            }
        }
        
    } while(pc<bc_size);

    //
    // mark exception handlers as leads of basic blocks
    //
    for (unsigned i=0; i<m_handlers.size(); i++) {
        const HandlerInfo& hi = m_handlers[i];
        bbs_mark_one(hi.handler, false, true, true, false);
    }

    //
    // make list of heads and sort it.
    //
    // TODO: may eliminate vector here, and use simple scan over byte code
    // Another variant is: 
    // currently, 2 passes over the bytecode performed. Might want to replace
    // it with a single DFS pass, and also may eliminate BBInfo structure.
    
    const unsigned num_bbs = m_bbs.size();
    ::std::vector<unsigned> bb_heads;
    bb_heads.reserve(num_bbs);
    for (BBMAP::iterator i=m_bbs.begin(); i != m_bbs.end(); i++) {
        bb_heads.push_back(i->second.start);
    };
    ::std::sort(bb_heads.begin(), bb_heads.end());

    // initialize with ATHROW to avoid ref_count increment for pc=0
    JavaByteCodes prev = OPCODE_ATHROW; 
    // for each basic block, find it's last instruction
    for (unsigned idx = 0; idx != num_bbs; idx++) {
        unsigned pc = bb_heads[idx];
        BBInfo& bb = m_bbs[pc];
        unsigned next_bb_start = idx == num_bbs-1 ? 
                                                bc_size : bb_heads[idx + 1];
        while (next_bb_start != m_insts[pc].next) {
            pc = m_insts[pc].next;
        }
        bb.last_pc = m_insts[pc].pc;
        bb.next_bb = next_bb_start;
        unsigned prev_flags = instrs[prev].flags;
        if (!(prev_flags & OPF_ENDS_BB)) {
            // here we have an instruction (jinst.opcode) which
            // follows an instruction which normally does not
            // end a basic block. This normally means, that the
            // current instruction is a branch target, and thus become
            // a BB leader. As the previous opcode does not end basic
            // block, then this means that there is a control flow path
            // which leads from the previous instruction to the current one:
            //
            //      GOTO n      ; somewhere in the bytecode
            //      ...
            //      ALOAD_0     ; sample prev instruction
            //  n:  ASTORE_1    ; sample current instruction. ref_ocunt found
            //                  is '1' because of GOTO
            //
            // Thus incrementing ref_count and this assertion
            //assert(bb.jmp_target || bb.jsr_target || 
            //       bb.start == 0 || bb.ehandler);
            bb.ref_count++;
        }
        prev = m_insts[pc].opcode;
        
        // Statistics:: size of the basic blocks, in bytes
        if (bb.ref_count) {
            STATS_MEASURE_MIN_MAX_VALUE( bb_size, bb.next_bb - bb.start, m_fname );
        }
    }
}

void Compiler::bbs_mark_one(unsigned pc, bool jmp_target, bool add_ref,
                            bool ehandler, bool jsr_target)
{
    assert(pc < USHRT_MAX);
    if (pc >= m_infoBlock.get_bc_size()) {
        return;
    }

    BBMAP::iterator i = m_bbs.find(pc);
    if (i != m_bbs.end()) {
        i->second.jmp_target = i->second.jmp_target || jmp_target;
        i->second.ehandler = i->second.ehandler || ehandler;
        i->second.jsr_target = i->second.jsr_target || jsr_target;
        if (add_ref )   ++i->second.ref_count;
    }
    else {
        BBInfo bbinfo;
        bbinfo.start = pc;
        bbinfo.last_pc = 0;
        bbinfo.next_bb = 0;
        bbinfo.ipoff = 0;

        //      bbinfo.next_bb = NOTHING;
        //      bbinfo.code_start = NULL;
        //      bbinfo.code_size = NOTHING;
        bbinfo.code_size = 0;
        bbinfo.jmp_target = jmp_target;
        bbinfo.ehandler = ehandler;
        bbinfo.ref_count = add_ref ? 1 : 0;
        bbinfo.jsr_target = jsr_target;
        bbinfo.processed = false;
        m_bbs[pc] = bbinfo;
    }
}

void Compiler::bbs_gen_code(unsigned pc, BBState * prev, unsigned jsr_lead)
{
    assert(m_bbs.find(pc) != m_bbs.end());
    BBInfo& bbinfo = m_bbs[pc];
    if (bbinfo.processed) {
        if (bbinfo.jsr_target) {
            // we're processing JSR subroutine as result of JSR call || we 
            // are processing JSR subroutine as result of fall-through pass
            assert(jsr_lead == pc || jsr_lead==NOTHING);
            // JSR block was processed already - there must be a stack state
            assert(m_stack_states.find(pc) != m_stack_states.end());
            // Simply load the state back to the parent's
            const JFrame& stackState = m_stack_states[ pc ];
            // make sure we'll not lost anything
            assert(prev->jframe.need_update() == 0);
            prev->jframe.init(&stackState);
        }
        return;
    }

    bbinfo.processed = true;
    BBState bbstate;
    
    if (prev == NULL) {
        bbstate.jframe.init(m_infoBlock.get_stack_max(),
                              m_infoBlock.get_num_locals(), 
                              I_STACK_REGS, I_LOCAL_REGS,
                              F_STACK_REGS, F_LOCAL_REGS);
    }
    else {
        bbstate.jframe.init(&prev->jframe);
    }

    m_jframe = &bbstate.jframe;
    m_curr_bb_state = &bbstate;
    m_curr_bb = &bbinfo;

    unsigned bb_ip_start = bbinfo.ipoff = m_codeStream.ipoff();
    
    if (pc == 0) {
        assert(!bbinfo.ehandler); // cant be at pc=0
        m_pc = 0;
        if (!(m_java_meth_flags & ACC_STATIC)) {
            // for instance methods, their local_0 is 'this' at entrance,
            // so we can say it's not null for sure.
            bbstate.jframe.var_def(jobj, 0, SA_NZ);
        }
        gen_prolog(m_args);
    }

    // If there are several execution paths merged on this basic block, then 
    // we can not predict many things, including, but not limited to type of
    // of local variables, state of stack items, what stack depth was saved 
    // last, etc. So, clearing this out.
    // The current convention is that before the execution goes to such 
    // 'multiref' basic block, then all local vars are in the memory and 
    // all appropriate stack items are on their registers
    //
    // The same applies to exception handlers.
    if (bbinfo.ehandler || bbinfo.ref_count > 1 || prev==NULL) {
        m_jframe->clear_vars();
        m_jframe->clear_attrs();
    //todo: might be useful to extract this several assignments into, say,
    // operator=(BBState*) - ?
        bbstate.seen_gcpt = false;
        bbstate.stack_depth = NOTHING;
        bbstate.stack_mask = 0;
        bbstate.stack_mask_valid = false;
    }
    else {
        bbstate.seen_gcpt = prev->seen_gcpt;
        bbstate.stack_depth = prev->stack_depth;
        bbstate.stack_mask = prev->stack_mask;
        bbstate.stack_mask_valid = prev->stack_mask_valid;
        bbstate.resState = prev->resState;
    }

    if (m_infoBlock.get_flags() & DBG_CHECK_STACK && (jsr_lead==NOTHING)){
        // do not check stack for JSR subroutines (at least for IA32/EM64T)
        // More comments in cg_ia32.cpp::gen_dbg_check_bb_stack
        gen_dbg_check_bb_stack();
    }

    if (bbinfo.ehandler) {
        //
        // Here, we invoke gen_save_ret() because this is how the idea of
        // exception handlers works in DRL VM: 
        // Loosely speaking, calling something like 'throw_<whatever>' is 
        // like a regular function call. The only difference is that the 
        // return point is at another address, not at the next instruction - 
        // we're 'returning' to the proper exception handler.
        //
        // That's why the exception object acts like a return value - for 
        // example on IA32 it's in EAX.
        //
        
        // a stack get empty on entrance into exception handler ...
        m_jframe->clear_stack();
        
        // ... with an reference to Exception object on the top of it
        gen_save_ret(jobj);
        
        // We're entering exception handler - the exception object 
        // on the stack is guaranteed to be non-null, marking it as such
        m_jframe->stack_attrs(0, SA_NZ);
    }

    if (m_infoBlock.get_flags() & DBG_TRACE_CG) {
        dbg(";; ========================================================\n");
        dbg(";; bb.ref.count=%d%s%s savedStackDepth=%d stackMask=0x%X %s\n",
            bbinfo.ref_count, bbinfo.ehandler ? " ehandler " : "",
            bbinfo.jsr_target ? " #JSR# " : "",
            bbstate.stack_depth, bbstate.stack_mask,
            bbstate.stack_mask_valid ? "" : "*mask.invalid*");
        if (bb_ip_start != m_codeStream.ipoff()) {
            dbg_dump_code(m_codeStream.data() + bb_ip_start,
                          m_codeStream.ipoff()-bb_ip_start, "bb.head");
        }
    }
    
    unsigned next_pc = bbinfo.start;
    
    do {
        // read out instruction to process
        const JInst& jinst = m_insts[next_pc];
        m_pc = next_pc;
        m_curr_inst = &jinst;
        next_pc = jinst.next;
        unsigned inst_code_start = m_codeStream.ipoff();
        
        if (m_infoBlock.get_flags() & DBG_TRACE_CG) {
            dbg_dump_jframe("before", &bbstate.jframe);
            // print an opcode
            dbg(";; %-30s\n", toStr(jinst, true).c_str());
        }
        
#if defined(_DEBUG) && defined(JET_PROTO)
        if (dbg_break_pc == jinst.pc) {
            gen_dbg_brk();
        }
#endif
        if (m_infoBlock.get_flags() & DBG_TRACE_BC) {
            gen_dbg_rt_out("//%s @ PC = %u", m_fname, m_pc);
        }

        STATS_INC(Stats::opcodesSeen[jinst.opcode], 1);

        handle_inst(jinst);
        
        unsigned inst_code_end = m_codeStream.ipoff();
        unsigned inst_code_size = inst_code_end-inst_code_start;

        unsigned bb_off = inst_code_start - bb_ip_start;
        // store a native offset inside the basic block for now,
        // this will be adjusted in bbs_layout_code(), by adding the BB's
        // start address
        for (unsigned i=jinst.pc; i<next_pc; i++) {
            m_infoBlock.set_code_info(i, (const char *)bb_off);
        }

        if (m_infoBlock.get_flags() & DBG_TRACE_CG) {
            // disassemble the code
            dbg_dump_code(m_codeStream.data() + inst_code_start, 
                          inst_code_size, NULL);
        }
        // no one should change m_pc, it's used right after the loop to get
        // the same JInst back again
        assert(jinst.pc == m_pc);
    } while(next_pc != bbinfo.next_bb);
    
    const JInst& jinst = m_insts[m_pc];

    unsigned bb_code_end = m_codeStream.ipoff();
    bbinfo.code_size = bb_code_end - bb_ip_start;
    
    // process JSR target first, so it will update the state of jframe
    bool is_jsr = jinst.opcode == OPCODE_JSR || jinst.opcode == OPCODE_JSR_W;
    if (is_jsr) {
        unsigned targetBB = jinst.get_target(0);
        bbs_gen_code(targetBB, &bbstate, targetBB);
        //
        // bbs_gen_code() invalidated m_* fields, so below only use
        // local variables, not the instance ones !
        //
        
        // We just executed/generated the JSR subroutine. 
        // We must have a state for it:
        assert(m_stack_states.find(targetBB) != m_stack_states.end());
        // Reload the stack state into the current one
        bbstate.jframe.init(&m_stack_states[targetBB]);
    }
    else if (jinst.opcode == OPCODE_RET) {
        assert(jsr_lead != NOTHING);
        m_stack_states[jsr_lead] = bbstate.jframe;
    }
    else {
        // process jmp target blocks
        for (unsigned i=0, n=jinst.get_num_targets(); i<n; i++) {
            bbs_gen_code(jinst.get_target(i), &bbstate, jsr_lead);
        }
        if (jinst.is_switch()) {
            bbs_gen_code(jinst.get_def_target(), &bbstate, jsr_lead);
        }
    }
    if (!(jinst.flags & OPF_DEAD_END)) {
        // process fall through block
        assert(next_pc != NOTHING);
        bbs_gen_code(next_pc, &bbstate, jsr_lead);
    }
    else if ((jinst.flags & OPF_EXIT) && jsr_lead != NOTHING) {
        // A special case - when a JSR subroutine ends the method 
        // (xRETURN, ATHROW) save an empty state, but if there are 
        // several paths in the subroutine and at least one of them does 
        // not end the method, then keep that state:
        // JSR _jsr
        // ...
        // _jsr: ASTORE_0
        //       IF_xCMPx _ret
        //       xRETURN        // <= here, save an empty state
        // _ret: ... RET 0      // <= here, save the stack state
        //
        // So, only save the empty state if there is no state exists
        // otherwise left it as is, this is either a state saved by 
        // RET or an empty state saved by another xRETURN. 
        // There is no such check in RET processing, so if we have both
        // xRETURN and RET then RET's state always overwrites xRETURN's.
        if (m_stack_states.find(jsr_lead) == m_stack_states.end()) {
            bbstate.jframe.clear_stack();
            bbstate.jframe.clear_vars();
            m_stack_states[jsr_lead] = bbstate.jframe;
        }
    }
}

void Compiler::bbs_layout_code(void)
{
    char * p = m_vmCode;
    // copy all BBs
    for (unsigned pc = 0; pc<m_infoBlock.get_bc_size(); ) {
        // it's basic block lead.
        assert(m_bbs.find(pc) != m_bbs.end());
        BBInfo& bbinfo = m_bbs[pc];
        const char * pBBStart = p;
        if (bbinfo.processed) {
            // copy the code. 
            memcpy(p, m_codeStream.data()+bbinfo.ipoff, bbinfo.code_size);
            bbinfo.ipoff = p - m_vmCode;
            if (m_infoBlock.get_flags() & DBG_TRACE_LAYOUT) {
                dbg("code.range: [%u - %u] => [%p - %p)\n", 
                    bbinfo.start, bbinfo.last_pc, 
                    pBBStart, pBBStart+bbinfo.code_size);
            }
        }
        else {
            if (m_infoBlock.get_flags() & DBG_TRACE_LAYOUT) {
                dbg("warning - dead code @ %d\n", pc);
            }
            assert(bbinfo.code_size == 0);
        }        
        // update runtime info with the mapping pc/ip data
        for (unsigned j=pc; j<bbinfo.next_bb; j++) {
            const char * inst_ip = pBBStart + (int)m_infoBlock.get_code_info(j);
            m_infoBlock.set_code_info(j, inst_ip);
        }
        p = (char*)(pBBStart+bbinfo.code_size);
        pc = bbinfo.next_bb;
    }
}

bool Compiler::bbs_ehandlers_resolve(void)
{
    unsigned num_handlers = method_get_num_handlers(m_method);
    m_handlers.resize(num_handlers);

    bool eh_ok = true;
    for (unsigned i=0; i<num_handlers; i++) {
        unsigned regStart, regEnd, handlerStart, klassType;
        method_get_handler_info(m_method, i,
                                &regStart, &regEnd, &handlerStart,
                                &klassType);
        HandlerInfo& hi = m_handlers[i];
        hi.start = regStart;
        hi.end = regEnd;
        hi.handler = handlerStart;
        hi.type = klassType;
        if (hi.type != 0) {
            Timers::vmResolve.start();
                hi.klass = resolve_class(m_compileHandle, m_klass, hi.type);
            Timers::vmResolve.stop();
            eh_ok = eh_ok && (hi.klass != NULL);
        }
        if (m_infoBlock.get_flags() & DBG_DUMP_BBS) {
            dbg("handler: [%-02d,%-02d] => %-02d\n", 
                hi.start, hi.end, hi.handler);
        }
    }
    return eh_ok;
}

void Compiler::bbs_ehandlers_set(void)
{
    unsigned real_num_handlers = 0;
    for (unsigned i=0, n=m_handlers.size(); i<n; i++) {
        HandlerInfo& hi = m_handlers[i];
        if (bbs_hi_to_native(m_vmCode, hi)) {
            ++real_num_handlers;
        }
        else {
            if (m_infoBlock.get_flags() & DBG_TRACE_LAYOUT) {
                dbg("warning: unreachable handler [%d;%d)=>%d\n", 
                    hi.start, hi.end, hi.handler);
            }
        }
    }

    method_set_num_target_handlers(m_method, jit_handle, real_num_handlers);
    
    for (unsigned i=0, handlerID=0, n=m_handlers.size(); i<n; i++) {
        const HandlerInfo& hi = m_handlers[i];
        if (hi.handler_ip == NULL) {
            continue;
        }
        // The last param is 'is_exception_object_dead'
        // In many cases, when an exception handler does not use an exception
        // object, the fist bytecode instruction is 'pop' to throw the 
        // exception object away. Thus, this can used as a quick, cheap but 
        // good check whether the 'exception_object_is_dead'. 
        method_set_target_handler_info(m_method, jit_handle, handlerID,
                                       hi.start_ip, hi.end_ip, hi.handler_ip, 
                                       hi.klass, 
                                       m_bc[hi.handler] == OPCODE_POP );
        
        ++handlerID;
        
        if (m_infoBlock.get_flags() & DBG_TRACE_LAYOUT) {
            dbg("native.handler: [%08X,%08X] => %08X"
                " ([%-02d,%-02d] => %-02d)\n",
                hi.start_ip, hi.end_ip, hi.handler_ip,  hi.start, hi.end, 
                hi.handler);
        }
    }
}

void Compiler::get_args_info(bool is_static, unsigned cp_idx, 
                             ::std::vector<jtype>& args, jtype * retType)
{
    const char * cpentry = class_get_cp_entry_signature(m_klass, 
                                                        (short)cp_idx);
    // expecting an empty vector
    assert(args.size() == 0);
    
    if (!is_static) {
        // all but static methods has 'this' as first argument
        args.push_back(jobj);
    }
    
    if (!cpentry) {
        assert(false);
        *retType = jvoid;
        return;
    }

    // skip '('
    const char *p = cpentry + 1;
    
    //
    // The presumption (cast of '*p' to VM_Data_Type) below is based on the 
    // VM_Data_Type values - they are equal to the appropriate characters: 
    // i.e. VM_Data_Type's long is 'J' - exactly as it's used in methods'
    // signatures
    //
    
    for (; *p != ')'; p++) {
        jtype jt = to_jtype((VM_Data_Type)*p);
        if (jt == jvoid) {
            break;
        }
        if (jt==jobj) {
            // skip multi-dimension array
            while(*p == '[') { p++; }
            // skip the full class name
            if (*p == 'L') {
                while(*p != ';') { p++; }
            }
        }
        args.push_back(jt);
    }
    p++;
    *retType = to_jtype((VM_Data_Type)*p);
}

void Compiler::get_args_info(Method_Handle meth, ::std::vector<jtype>& args,
                             jtype * retType)
{
    Method_Signature_Handle hsig = method_get_signature(meth);
    unsigned num_params = method_args_get_number(hsig);
    if (num_params) {
        args.resize(num_params);
    }
    for (unsigned i=0; i<num_params; i++) {
        Type_Info_Handle th = method_args_get_type_info(hsig, i);
        assert(!type_info_is_void(th));
        args[i] = to_jtype(th);
    }
    *retType = to_jtype(method_ret_type_get_type_info(hsig));
}

jtype Compiler::to_jtype(Type_Info_Handle th)
{
    if (type_info_is_unboxed(th)) {
        Class_Handle ch = type_info_get_class(th);
        assert(class_is_primitive(ch));
        VM_Data_Type vmdt = class_get_primitive_type_of_class(ch);
        return to_jtype(vmdt);
    }
    if (type_info_is_void(th)) {
        return jvoid;
    }
    assert(type_info_is_reference(th) || type_info_is_vector(th));
    return jobj;
}

bool Compiler::bbs_hi_to_native(char * codeBlock, HandlerInfo& hi)
{
    // Find beginning of the area protected by the handler, 
    // lookup for the first reachable instruction
    unsigned pc = hi.start;
    for (; pc < hi.end; pc++) {
        if ((hi.start_ip = (char*)m_infoBlock.get_ip(pc)) != NULL)   break;
    }

    hi.end_ip = (char*)m_infoBlock.get_ip(hi.end);
    if (hi.end_ip == NULL) {
        assert(false);
    }

    // either both are NULLs, or both are not NULLs
    assert(!((hi.start_ip == NULL) ^ (hi.end_ip == NULL)));

    if (hi.start_ip != NULL) { // which also implies '&& end_ip != NULL' 
                               // - see assert() above

        // we must have the handler
        assert(m_infoBlock.get_ip(hi.handler) != NULL);

        const BBInfo& handlerBB = m_bbs[hi.handler];
        assert(handlerBB.ehandler);
        hi.handler_ip = codeBlock + handlerBB.ipoff;
    }
    else {
        hi.handler_ip = NULL;
    }
    return hi.start_ip != NULL;
}

void Compiler::initStatics(void)
{

    // must only be called once
    assert(NULL == rt_helper_throw);
    
    //
    // Collect addresses of runtime helpers
    //
    rt_helper_throw = 
                (char*)vm_get_rt_support_addr(VM_RT_THROW);
    rt_helper_throw_out_of_bounds = 
                (char*)vm_get_rt_support_addr(VM_RT_IDX_OUT_OF_BOUNDS);
    rt_helper_throw_npe = 
                (char*)vm_get_rt_support_addr(VM_RT_NULL_PTR_EXCEPTION);
    rt_helper_throw_linking_exc = 
                (char*)vm_get_rt_support_addr(VM_RT_THROW_LINKING_EXCEPTION);
    rt_helper_throw_div_by_zero_exc = 
                (char*)vm_get_rt_support_addr(VM_RT_DIVIDE_BY_ZERO_EXCEPTION);

    rt_helper_monitor_enter = 
                (char*)vm_get_rt_support_addr(VM_RT_MONITOR_ENTER);
    rt_helper_monitor_exit  = 
                (char*)vm_get_rt_support_addr(VM_RT_MONITOR_EXIT);
    rt_helper_monitor_enter_static = 
                (char*)vm_get_rt_support_addr(VM_RT_MONITOR_ENTER_STATIC);
    rt_helper_monitor_exit_static = 
                (char*)vm_get_rt_support_addr(VM_RT_MONITOR_EXIT_STATIC);

    rt_helper_ldc_string =
                (char*)vm_get_rt_support_addr(VM_RT_LDC_STRING);
    rt_helper_new = 
     (char*)vm_get_rt_support_addr(VM_RT_NEW_RESOLVED_USING_VTABLE_AND_SIZE);
    rt_helper_new_array = 
                (char*)vm_get_rt_support_addr(VM_RT_NEW_VECTOR_USING_VTABLE);
    rt_helper_init_class = 
                (char*)vm_get_rt_support_addr(VM_RT_INITIALIZE_CLASS);
    rt_helper_aastore = (char*)vm_get_rt_support_addr(VM_RT_AASTORE);
    rt_helper_multinewarray = 
                (char*)vm_get_rt_support_addr(VM_RT_MULTIANEWARRAY_RESOLVED);
    rt_helper_get_vtable = 
              (char*)vm_get_rt_support_addr(VM_RT_GET_INTERFACE_VTABLE_VER0);
    rt_helper_checkcast = 
                (char*)vm_get_rt_support_addr(VM_RT_CHECKCAST);
    rt_helper_instanceof = 
                (char*)vm_get_rt_support_addr(VM_RT_INSTANCEOF);
    rt_helper_gc_safepoint = 
                (char*)vm_get_rt_support_addr(VM_RT_GC_SAFE_POINT);
    rt_helper_get_thread_suspend_ptr = 
                (char*)vm_get_rt_support_addr(VM_RT_GC_GET_THREAD_SUSPEND_FLAG_PTR);
    //
    rt_helper_ti_method_enter = 
            (char*)vm_get_rt_support_addr(VM_RT_JVMTI_METHOD_ENTER_CALLBACK);
    rt_helper_ti_method_exit = 
            (char*)vm_get_rt_support_addr(VM_RT_JVMTI_METHOD_EXIT_CALLBACK);
    //
    // Collect runtime constants
    //
    rt_array_length_offset = vector_length_offset();
    rt_suspend_req_flag_offset = thread_get_suspend_request_offset();
    
    Class_Handle clss;
    clss = class_get_class_of_primitive_type(VM_DATA_TYPE_INT8);
    jtypes[i8].rt_offset = vector_first_element_offset_unboxed(clss);
    
    clss = class_get_class_of_primitive_type(VM_DATA_TYPE_INT16);
    jtypes[i16].rt_offset= vector_first_element_offset_unboxed(clss);

    clss = class_get_class_of_primitive_type(VM_DATA_TYPE_CHAR);
    jtypes[u16].rt_offset= vector_first_element_offset_unboxed(clss);

    clss = class_get_class_of_primitive_type(VM_DATA_TYPE_INT32);
    jtypes[i32].rt_offset= vector_first_element_offset_unboxed(clss);

    clss = class_get_class_of_primitive_type(VM_DATA_TYPE_INT64);
    jtypes[i64].rt_offset= vector_first_element_offset_unboxed(clss);

    clss = class_get_class_of_primitive_type(VM_DATA_TYPE_F4);
    jtypes[flt32].rt_offset= vector_first_element_offset_unboxed(clss);

    clss = class_get_class_of_primitive_type(VM_DATA_TYPE_F8);
    jtypes[dbl64].rt_offset= vector_first_element_offset_unboxed(clss);

    jtypes[jobj].rt_offset= vector_first_element_offset(VM_DATA_TYPE_CLASS);

    jtypes[jvoid].rt_offset = 0xFFFFFFFF;
}

void Compiler::initProfilingData(unsigned * pflags) {
    m_p_methentry_counter = NULL;
    m_p_backedge_counter = NULL;
#if !defined(PROJECT_JET)
    JITModeData* modeData = Jitrino::getJITModeData(jit_handle);
    ProfilingInterface* pi = modeData->getProfilingInterface();
    if (pi->isProfilingEnabled(ProfileType_EntryBackedge,
                               JITProfilingRole_GEN)) {
        MemoryManager mm(128, "jet_profiling_mm");
        DrlVMMethodDesc md(m_method, jit_handle);

        g_compileLock.lock();

        EntryBackedgeMethodProfile* mp = 
            (EntryBackedgeMethodProfile*)pi->getMethodProfile(mm, 
                        ProfileType_EntryBackedge, md, JITProfilingRole_GEN);
        if (mp == NULL) {
            mp = pi->createEBMethodProfile(mm, md);
        }
        *pflags |= JMF_PROF_ENTRY_BE;
        m_p_methentry_counter = mp->getEntryCounter();
        m_p_backedge_counter = mp->getBackedgeCounter();
        if (pi->isEBProfilerInSyncMode()) {
            *pflags |= JMF_PROF_SYNC_CHECK;
            m_methentry_threshold = pi->getEBProfilerMethodEntryThreshold();
            m_backedge_threshold = pi->getEBProfilerBackedgeThreshold();
            m_profile_handle = mp->getHandle();
            m_recomp_handler_ptr = (void*)pi->getEBProfilerSyncModeCallback();
        }

        g_compileLock.unlock();
    }
#endif
}

}}; // ~namespace Jitrino::Jet

