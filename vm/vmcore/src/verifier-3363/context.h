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
 * @author Mikhail Loenko, Vladimir Molotkov
 */  

#ifndef __CONTEXT_H__
#define __CONTEXT_H__

#include <assert.h>
#include <string.h>
#include "verifier.h"
#include "stackmap.h"
#include "memory.h"
#include "tpool.h"
#include "instr_props.h"

namespace CPVerifier {

    //
    // Context - main class of Type Checker
    //

    class vf_Context_t {
    public:
        vf_Context_t(class_handler _klass) :

#pragma warning( push )
#pragma warning( disable : 4355 )
          k_class(_klass), tpool(this, 256),
#pragma warning( pop )

              constraintPool(), class_constraints(0)
          {
              k_major = class_get_version( _klass );
          }

          vf_Result verify_method(method_handler method);
          void set_class_constraints();
          Address processed_instruction;
          int  pass;

          const char *error_message;
    protected:
        friend class vf_TypePool;

        //class handler for the current class
		class_handler  k_class;

        //major version of current class
		unsigned short  k_major;

        //method handler for the method being verified
        method_handler m_method;

        //method's bytecode
        unsigned char  *m_bytecode;

        //legth of the code in the method being verified
        unsigned       m_code_length;

        //max number of locals for the method being verified (as recorded in the classfile)
        unsigned       m_max_locals;

        //is the current method construcor (and current class in not a j.l.Object)?
        bool           m_is_constructor;

        //m_max_locals or m_max_locals+1 if it's a constructor
        unsigned       m_stack_start;

        //max stack size for the method being verified (as recorded in the classfile)
        unsigned       m_max_stack;

        //number of exception handlers for the being verified (as recorded in the classfile)
        unsigned short m_handlecount;

        //stores constraints in the classloader for reuse in vf_verify_class_constraints()
        vf_TypeConstraint_s *class_constraints;

        //storage for these constraints. class-wide
        //TODO: makes sense to unite it with tpool containing other class-wide data?
        Memory constraintPool;


        /*****************/

        // method's return type
        SmConstant return_type;

        // various flags for all the method's bytecode instructions
        InstrProps props;

        // table used to get various info (type, length, etc) about possible bytecode instructions
        static ParseInfo parseTable[255];

        // current set of derived types
        WorkmapHead *workmap;

        // stack to push instrauctions like branch targets, etc to go thru the method. the stack is method-wide.
        MarkableStack stack;

        FastStack dead_code_stack;
        bool      dead_code_parsing;

        static const short MARK_SUBROUTINE_DONE = -1;
    public:
        // basic storage for most of the method-wide data
        Memory mem;
    protected:

        //basic storage for class-wide data, like mapping from Java classes to SmConstant types
        vf_TypePool tpool;

        /******* exception handling **********/

        //flag array. if a local var i was changed by the previous instruction ==> changed_locals[i]=1, otherwise it's 0
        byte *changed_locals;

        //if there is at least one local changed
        int locals_changed;

        //if we don't know whether previous instruction changed locals (like if we are at the branch target)
        int no_locals_info;

        //number of the first handler valid for the given instruction
        int loop_start;

        //<number of the last handler valid for the given instruction> + 1
        int loop_finish;

        //start of the nearest next try block. 0 means "don't know"
        Address next_start_pc;

        /*****************/

        //report verify error and store a message if any
        vf_Result error(vf_Result result, const char* message) {
            error_message = message ? message : "";
            //assert(0);
            return result;
        }

        //init method-wide data
        void init(method_handler _m_method) {
            //store method's parameters
            //TODO: it might be mot slower not to store them
            m_method = _m_method;
            m_max_locals = method_get_max_local( m_method );
            m_max_stack = method_get_max_stack( m_method );
            m_code_length = method_get_code_length( m_method );
            m_handlecount = method_get_exc_handler_number( m_method );
            m_bytecode = method_get_bytecode( m_method );

            m_is_constructor = !strcmp(method_get_name(m_method), "<init>") 
                && class_get_super_class(k_class);

            m_stack_start = m_max_locals + (m_is_constructor ? 1 : 0);

            // initialize own parameters
            mem.init();
            props.init(mem, m_code_length);

            stack.init();
            dead_code_stack.init();

            changed_locals = (byte*)mem.malloc((m_stack_start & ~3) + 4);

            //to correct it later
            return_type = SM_TOP;
        }

        // load derived types previously stored for the given instruction
        void fill_workmap(Address instr) {
            PropsHead *head = props.getInstrProps(instr);
            if( head->is_workmap() ) {
                tc_memcpy(workmap, head->getWorkmap(), sizeof(WorkmapHead) + sizeof(WorkmapElement) * (m_stack_start + head->workmap.depth));
            } else {
                StackmapHead *stackmap = head->getStackmap();

                workmap->depth = stackmap->depth;

                for( unsigned i = 0; i < m_stack_start + stackmap->depth; i++) {
                    workmap->elements[i] = _WorkmapElement(&stackmap->elements[i]);
                    assert( workmap->elements[i].getAnyPossibleValue() != SM_TOP );
                }
            }
            no_locals_info = 1;
        }

        //store a copy of the current workmap for another instruction (such as a branch target)
        void storeWorkmapCopy(Address target) {
            int sz = m_stack_start + workmap->depth;
            PropsHead* copy = newWorkmap(sz);
            tc_memcpy(copy->getWorkmap(), workmap, sizeof(WorkmapHead) + sizeof(WorkmapElement) * sz);

            props.setInstrProps(target, copy);
        }

        //create a stackmap vector of the given size sz (max_locals <= sz <= max_locals+max_stack)
        PropsHead* newStackmap(int sz) {
            return (PropsHead*)mem.calloc(sizeof(PropsHead) + sizeof(StackmapElement) * sz);
        }

        //create a workmap vector for the given size sz (max_locals <= sz <= max_locals+max_stack)
        PropsHead *newWorkmap(int sz) {
            PropsHead * ret = (PropsHead*)mem.malloc(sizeof(PropsHead) + sizeof(WorkmapElement) * sz);
            ret->set_as_workmap();
            return ret;
        }

        //create a vector that will be used for JSR procesing. 
        //It contains ether stackmap or workmap vector, SubrouitineData, and flags vector indicating 
        //changed locals
        PropsHead *newRetData() {
            assert( sizeof(StackmapElement) >= sizeof(WorkmapElement) );

            int sz = sizeof(PropsHead) + sizeof(StackmapElement) * (m_max_stack + m_stack_start) + //stackmap
                ((sizeof(SubroutineData)+ m_stack_start) & (~3)) + 4; // fixed data and changed locals vector

            PropsHead * ret = (PropsHead *) mem.calloc(sz);
            ret->set_as_workmap();
            return ret;
        }

        //creates a temporary variable for converting 
        StackmapElement *new_variable() {
            return (StackmapElement *)mem.calloc(sizeof(StackmapElement));
        }

        /////////////////////////////////////////////////////////////////////////////////////////////////////

        //First verification pass thru the method. checks that no jump outside the method or to the middle of instruction
        //checks that opcodes are valid
        vf_Result parse(Address instr);

        //Second pass: dataflow of a piece of the method starting from the beginning or a branch target and finishing
        //on return, athrow or hitting previously passed instruction. 
        //This function initializes workmap and calls DataflowLoop
        vf_Result StartLinearDataflow(Address start);

        //Second pass: Finilize subroutie processing -- once we are here, then all the RETs from achievable for
        //the given subroutine are passed, so we can resume passing for JSRs to the given address
        //This function initializes workmap properly and calls DataflowLoop
        vf_Result SubroutineDone(Address start);

        //Second pass: dataflow of a piece of the method starting from the beginning or a branch target and finishing
        //on return, athrow or hitting previously passed instruction
        vf_Result DataflowLoop(Address start, int workmap_is_a_copy_of_stackmap);

        //Second pass: check type-safety of a single instruction
        vf_Result dataflow_instruction(Address instr);

        //Second pass: check type-safety for exception handlers of a single instruction
        vf_Result dataflow_handlers(Address instr);

        //specail care for <init> calls is in try blocks
        vf_Result propagate_bogus_to_handlers(Address instr, SmConstant uninit_value);

        //create constraint vector in case of a branch 
        //simple conatraints are created for pairs of both locals and stack (current must be assignable to target)
        vf_Result new_generic_vector_constraint(Address target);

        //create constraint vector for exception handler
        //simple conatraints are created for pairs of local variable (current must be assignable to start of exception handler)
        vf_Result new_handler_vector_constraint(Address handler);

        //create simple single constraint: "'from' is assingable to 'to'"
        vf_Result new_scalar_constraint(WorkmapElement *from, StackmapElement *to);

        //create special type of conatraint: "'from' is an array and it's element is assignable to 'to'"
        vf_Result new_scalar_array2ref_constraint(WorkmapElement *from, WorkmapElement *to);

        //constraint propagation
        vf_Result propagate(StackmapElement *changed, SmConstant new_value);

        //update current derived types according to what was changed in subroutine
        void restore_workmap_after_jsr(Address jsr_target);

        //when we hit RET instruction we update the data for the given subroutine with current derived types
        vf_Result new_ret_vector_constraint(Address target_instr);

        /////////////////////////////////////////////////////////////////////////////////////////////////////

        //check conditions for accessing protected non-static fields in different package
        vf_Result popFieldRef(SmConstant expected_ref, unsigned short cp_idx);

        //check conditions for accessing protected virtual methods in different package
        vf_Result popVirtualRef(SmConstant expected_ref, unsigned short cp_idx);

        /////////////////////////////////////////////////////////////////////////////////////////////////////

        //add one more possible value (type) that can come to the given point (local or stack)
        vf_Result add_incoming_value(SmConstant new_value, StackmapElement *destination);

        //returns stackmap for the 'instr' instruction
        //if it does not exists yet -- create it. When created use 'depth' as stack depth
        StackmapHead *getStackmap(Address instr, int depth) {
            PropsHead *pro = props.getInstrProps(instr);
            if( !pro ) {
                pro = newStackmap(m_stack_start + depth);
                props.setInstrProps(instr, pro);
                pro->getStackmap()->depth = depth;
            }
            return pro->getStackmap();
        }

        //create stackmap for exception handler start
        void createHandlerStackmap(Address handler_pc, SmConstant type) {
            StackmapHead *map = getStackmap(handler_pc, 1);
            //handler stackmaps are created before any dataflow analysis is done
            assert(map->depth == 0 || map->depth == 1);
            map->depth = 1;

            vf_Result tcr = add_incoming_value(type, &map->elements[m_stack_start]);

            // it is initialization stage
            assert(tcr == VF_OK);
        }


        /////////////// set, get locals; push, pop stack; check... //////////////

        //when exercizing instructions: POP operand from the stack
        WorkmapElement workmap_pop() {
            assert( workmap_can_pop(1) );
            return workmap->elements[ (--workmap->depth) + m_stack_start ];
        }

        //looking the operand stack
        WorkmapElement workmap_stackview(int depth) {
            assert( depth >= 0 && workmap_can_pop(depth+1) );
            return workmap->elements[ workmap->depth + m_stack_start - depth - 1];
        }

        //when exercizing instructions: PUSH operand to the stack
        void workmap_push(WorkmapElement el) {
            assert( workmap_can_push(1) );
            workmap->elements[ (workmap->depth++) + m_stack_start ] = el;
        }

        //when exercizing instructions: PUSH a const (known) type to the stack (except long and double)
        void workmap_push_const(SmConstant value) {
            assert( workmap_can_push(1) );
            workmap->elements[ workmap->depth + m_stack_start ] = _WorkmapElement(value);
            workmap->depth++;
        }

        //when exercizing instructions: PUSH a const (known) long or double type to the stack
        void workmap_2w_push_const(SmConstant value) {
            workmap_push_const(SM_HIGH_WORD);
            workmap_push_const(value);
        }

        //when exercizing instructions: check if the local idx is valid for long and double
        bool workmap_valid_2w_local(unsigned idx) {
            return workmap_valid_local(idx + 1);
        }

        //when exercizing instructions: check if the local idx is valid (except long and double)
        bool workmap_valid_local(unsigned idx) {
            return idx < m_max_locals;
        }

        //get local type by idx
        WorkmapElement workmap_get_local(unsigned idx) {
            assert( workmap_valid_local(idx) );
            return workmap->elements[ idx ];
        }

        //set local type
        void workmap_set_local(unsigned idx, WorkmapElement &el) {
            assert( workmap_valid_local(idx) );

            changed_locals[ idx ] = 1;
            locals_changed = true;

            el.setJsrModified();
            workmap->elements[ idx ] = el;		
        }

        //set local to a const (known) type except long and double
        void workmap_set_local_const(unsigned idx, SmConstant value) {
            assert( workmap_valid_local(idx) );

            changed_locals[ idx ] = 1;
            locals_changed = true;

            workmap->elements[idx] = _WorkmapElement(value);

            //don't need to set "jsr modified" flag for constants
            //because they are already odd
            assert(workmap->elements[idx].isJsrModified());
        }                                                              

        //set local to a const (known) long or double type
        void workmap_set_2w_local_const(unsigned idx, SmConstant value) {
            assert( workmap_valid_2w_local(idx) );
            workmap_set_local_const(idx + 1, value);
            workmap_set_local_const(idx, SM_HIGH_WORD);
        }

        //check whether we can pop 'number' elements from the operand stack
        int workmap_can_pop(unsigned number) {
            return workmap->depth >= number;
        }

        //check whether we can push 'number' elements to the operand stack
        int workmap_can_push(unsigned number) {
            return workmap->depth + number <= m_max_stack;
        }

        /////////////// expect some type //////////////

        //expect exactly this type (or SM_TOP)
        int workmap_expect_strict( WorkmapElement &el, SmConstant type ) {
            assert(type != SM_BOGUS);

            if( !el.isVariable() ) {
                return type == el.getConst();
            }

            IncomingType *in = el.getVariable()->firstIncoming();
            while( in ) {
                if( type != in->value ) {
                    return false;
                }
                in = in->next();
            }

            ExpectedType *exp = el.getVariable()->firstExpected();
            while( exp ) {
                if( type == exp->value ) {
                    return true;
                }
                exp = exp->next();
            }

            el.getVariable()->newExpectedType(&mem, type);

            return true;
        }

        int workmap_expect( WorkmapElement &el, SmConstant type ) {
            if( !el.isVariable() ) {
                return tpool.mustbe_assignable(el.getConst(), type);
            } else {
                ExpectedType* exp = el.getVariable()->firstExpected();
                while( exp ) {
                    if( type == exp->value ) {
                        return true;
                    }
                    exp = exp->next();
                }

                IncomingType *in = el.getVariable()->firstIncoming();
                //check that all existing incoming type are assignable to the new expected type
                while( in ) {
                    if( !tpool.mustbe_assignable(in->value, type) ) {
                        return false;
                    }
                    in = in->next();
                }
                //add the new expected type
                el.getVariable()->newExpectedType(&mem, type);
            }
            return true;
        }

        vf_Result create_method_initial_workmap();


        //////////////// get constant SM_ELEMENTs ///////////////////////

        //for given uninit_value create SmConstant for initialized value
        //this function is used when <init>s and invoked
        SmConstant sm_convert_to_initialized(SmConstant uninit_value) {
            if( uninit_value == SM_THISUNINIT ) {
                return tpool.sm_get_const_this();
            }

            if( uninit_value.isNewObject() ) {
                Address addr = uninit_value.getNewInstr();

                unsigned cp_idx = read_int16(m_bytecode + addr + 1);
                SmConstant new_type;
                if( !tpool.cpool_get_class(cp_idx, &new_type) ) {
                    assert(0);
                    return SM_BOGUS;
                }
                return new_type;
            }

            assert(0);
            return SM_BOGUS;
        }

        //create vector constraints for each target of a switch
        vf_Result processSwitchTarget(Address target) {
            vf_Result tcr;
            if( props.isMultiway(target) ) {
                if( (tcr=new_generic_vector_constraint(target)) != VF_OK ) {
                    return tcr;
                }

                if( !props.isDataflowPassed(target) ) {
                    stack.xPush(target);
                }
            } else {
                assert( !props.isDataflowPassed(target) );
                storeWorkmapCopy(target);

                stack.xPush(target);
            }
            return VF_OK;
        }

        /////////////////////// convinient methods //////////////////////////////////////////

        //get length of variable size instruction (WIDE, *SWITCH)
        int instr_get_len_compound(Address instr, OpCode opcode);

        //read two-byte value
        static uint16 read_int16(byte* ptr) {
            return (ptr[0] << 8) | ptr[1];
        }

        //read four-byte value
        static uint32 read_int32(byte* ptr) {
            return (ptr[0] << 24) | (ptr[1] << 16) | (ptr[2] << 8) | ptr[3];
        }

        //get properties specific for the given opcode
        static ParseInfo &instr_get_parse_info(OpCode opcode) {
            return parseTable[opcode];
        }

        //get the length of the given instruction or minimal length if unknown
        static byte instr_get_minlen(ParseInfo &pi) {
            return pi.instr_min_len;
        }

        //whether this instruction GOTO, IF*, or JSR
        static int instr_is_jump(ParseInfo &pi) {
            return pi.flags & PI_JUMP;
        }

        //whether this instruction GOTO, RETURN, ATHROW, or RET
        static int instr_direct(ParseInfo &pi, OpCode opcode, byte* code, Address instr) {
            return (pi.flags & PI_DIRECT) || (opcode == OP_WIDE && code[instr + 1] == OP_RET);
        }

        //whether this instruction a *SWITCH
        static int instr_is_switch(ParseInfo &pi) {
            return pi.flags & PI_SWITCH;
        }

        //other types of instructions
        static int instr_is_regular(ParseInfo &pi) {
            return !(pi.flags & (PI_SWITCH|PI_JUMP|PI_DIRECT));
        }

        //whether instruction length is unknown
        static int instr_is_compound(OpCode opcode, ParseInfo &pi) {
            return (pi.flags & PI_SWITCH) || opcode == OP_WIDE;
        }

        //JSR ?
        static int instr_is_jsr(OpCode opcode) {
            return opcode == OP_JSR || opcode == OP_JSR_W;
        }

        //RET ?
        static int instr_is_ret(OpCode opcode, byte* code, Address instr) {
            return opcode == OP_RET || opcode == OP_WIDE && code[instr + 1] == OP_RET;
        }

        //return the jump target for the given instruction
        static Address instr_get_jump_target(ParseInfo &pi, byte* code, Address instr) {
            if( pi.flags & PI_WIDEJUMP ) {
                return instr + read_int32(code + instr + 1);
            } else {
                return instr + read_int16(code + instr + 1);
            }        
        }

        //is this opcode valid?
        static int instr_is_valid_bytecode(OpCode opcode) {
            return opcode <= OP_MAXCODE && opcode != OP_XXX_UNUSED_XXX;
        }
    };

    //check conatraints stored in the classloader data. force loading if necessary
    vf_Result
        vf_force_check_constraint(class_handler klass,
        vf_TypeConstraint_t *constraint);

} // namespace CPVerifier

#endif
