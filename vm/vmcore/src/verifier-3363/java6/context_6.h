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

#ifndef __CONTEXT6_H__
#define __CONTEXT6_H__

#include <assert.h>
#include <string.h>
#include "../base/context_x.h"
#include "stackmap_6.h"

using namespace CPVerifier;


namespace CPVerifier_6 {

    //
    // Context - main class of Type Checker
    //

    class vf_Context_6 : public vf_Context_x<vf_Context_6, WorkmapElement, _WorkmapElement, StackmapElement> {
    public:
        vf_Context_6(SharedClasswideData &classwide) :
          vf_Context_x<vf_Context_6, WorkmapElement, _WorkmapElement, StackmapElement>(classwide) {}

          vf_Result verify_method(method_handler method);
    protected:
        // stackmaps for instructions
        InstrPropsBase props;

        //init method-wide data
        void init(method_handler _m_method) {
            vf_Context_x<vf_Context_6, WorkmapElement, _WorkmapElement, StackmapElement>::init(_m_method);
            props.init(mem, m_code_length);
        }

        // load derived types previously stored for the given instruction
        void fill_workmap(Address instr) {
            PropsHead *head = (PropsHead*)props.getInstrProps(instr);
            assert(sizeof(StackmapElement) == sizeof(WorkmapElement));
            tc_memcpy(workmap, head->getStackmap(), sizeof(WorkmapHead) + sizeof(WorkmapElement) * (m_stack_start + head->stackmap.depth));
        }

        //create a stackmap vector of the given size sz (max_locals <= sz <= max_locals+max_stack)
        PropsHead* newStackmap(int sz) {
            return (PropsHead*)mem.calloc(sizeof(PropsHead) + sizeof(StackmapElement) * sz);
        }

        //parse StackMapTable attribute and store Stackmap vectors for the instructions 
        vf_Result load_stackmaptable();

        //Read cnt types from a row bytearray representing StackMapTable and record to workmap starting at 
        //the specified element. If Long or Double happens in StackMapTable, record SM_HIGH_WORD after SM_LONG or SM_DOUBLE
        //to the workmap and increase cnt. Check space_available when record to the workmap
        vf_Result read_types(uint8 **attr, uint8 *end, WorkmapElement* element, unsigned *cnt, unsigned space_available);


        ///////////////////////////////////  "VIRTUAL" METHODS /////////////////////////////////////////////
    public:
        //create constraint vector in case of a branch 
        //simple conatraints are created for pairs of both locals and stack (current must be assignable to target)
        vf_Result new_generic_vector_constraint(Address target_instr) {
            StackmapHead *target = getStackmap(target_instr);
            return target ? new_generic_vector_constraint_impl(target) : error(VF_ErrorBranch, "no stackmap at branch target");
        }

        //when we hit RET instruction we update the data for the given subroutine with current derived types
        vf_Result new_ret_vector_constraint(Address target_instr) {
            assert(0);
            return error(VF_ErrorInternal, "unexpected JSR/RET instruction");
        }

        // Java5 anachronism: push catch-block to the stack of branches to pass, empty in Java6
        void push_handler(Address handler_pc) {
        }

        //check stackmap for exception handler start
        vf_Result checkHandlerStackmap(Address handler_pc, SmConstant type) {
            StackmapHead *map = getStackmap(handler_pc);
            if( !map ) {
                return error(VF_ErrorHandler, "no stackmap at catch");
            }
            if( map->depth != 1 ) {
                return error(VF_ErrorHandler, "incorrect stack at catch");
            }
            return add_incoming_value(type, &map->elements[m_stack_start]);
        }

        //returns stackmap for the 'instr' instruction or 0 if it does not exist
        StackmapHead *getStackmap(Address instr) {
            PropsHead *pro = (PropsHead*) props.getInstrProps(instr);
            return pro ? pro->getStackmap() : 0;
        }

        /////////////// expect some type //////////////

        //expect exactly this type
        int workmap_expect_strict( WorkmapElement &el, SmConstant type ) {
            assert(type != SM_BOGUS);
            return type == el.const_val;
        }

        int workmap_expect( WorkmapElement &el, SmConstant type ) {
            return tpool.mustbe_assignable(el.const_val, type);
        }

        //create simple single constraint: "'from' is assingable to 'to'"
        vf_Result new_scalar_constraint(WorkmapElement *from, StackmapElement *to) {
            return add_incoming_value(from->const_val, to);
        }

        //add one more possible value (type) that can come to the given point (local or stack)
        vf_Result add_incoming_value(SmConstant new_value, StackmapElement *destination) {
            return tpool.mustbe_assignable(new_value, destination->const_val) ? VF_OK : 
                error(VF_ErrorIncompatibleArgument, "incompatible argument");
        }

        //create special type of conatraint: "'from' is an array and it's element is assignable to 'to'"
        vf_Result new_scalar_array2ref_constraint(WorkmapElement *from, WorkmapElement *to) {
            //although new_scalar_conatraint() whould process from constants correctly 
            // we just do not need new variable if it is really a constant
            *to = _WorkmapElement( tpool.get_ref_from_array(from->const_val) );
            return VF_OK;
        }

        void new_bogus_propagation_constraint(WorkmapElement &wm_el, SmConstant init_val) {
            wm_el = _WorkmapElement (init_val);
        }
    };

} // namespace CPVerifier

#endif
