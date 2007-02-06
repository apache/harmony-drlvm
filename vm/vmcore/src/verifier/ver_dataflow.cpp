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
 * @author Pavel Rebriy
 * @version $Revision: 1.1.2.2.4.4 $
 */  

#include "ver_real.h"

/**
 * Debug flag macros
 */
// Macro prints data flow node vectors
#define DUMP_NODE_VECTOR       0
// Macro prints data flow node instruction vectors
#define DUMP_NODE_INSTR_VECTOR 0

/**
 * Set namespace Verifier
 */
namespace Verifier {

/************************************************************
 **************** Graph Data Flow Analysis ******************
 ************************************************************/

#if _VERIFY_DEBUG

/**
 * Function prints stack map entry into output stream.
 */
static void
vf_dump_vector_entry( vf_MapEntry_t *entry,     // stack map entry
                      ostream *stream)          // output stream
{
    switch( entry->m_type )
    {
    case SM_TOP:
        *stream << " [TOP ]";
        break;
    case SM_INT:
        *stream << " [INT ]";
        break;
    case SM_FLOAT:
        *stream << " [FLT ]";
        break;
    case SM_LONG_LO:
        *stream << " [LOLO]";
        break;
    case SM_LONG_HI:
        *stream << " [LOHI]";
        break;
    case SM_DOUBLE_LO:
        *stream << " [DOLO]";
        break;
    case SM_DOUBLE_HI:
        *stream << " [DOHI]";
        break;
    case SM_NULL:
        *stream << " [NULL]";
        break;
    case SM_REF:
        *stream << " [REF ]";
        break;
    case SM_UNINITIALIZED:
        if( entry->m_new ) {
            *stream << " [UN " << entry->m_new % 10 << "]";
        } else {
            *stream << " [THIS]";
        }
        break;
    case SM_UNINITIALIZED_THIS:
        *stream << " [THIS]";
        break;
    default:
        *stream << " [? " << entry->m_type << "]";
    }
    return;
} // vf_dump_vector_entry

/**
 * Function prints data flow vector into output stream.
 */
void
vf_dump_vector( vf_MapVector_t *vector,     // data flow vector
                vf_Code_t *code,            // code instruction
                ostream *stream)            // output stream (can be NULL)
{
    unsigned index,
             count;

    // set steam if it's needed
    if( stream == NULL ) {
        stream = &cerr;
    }

    // dump code instruction if it's needed
    if( code != NULL ) {
        *stream << ((code->m_stack < 0) ? "[" : "[ ")
            << code->m_stack << "| " << code->m_minstack << "] "
            << vf_opcode_names[*(code->m_addr)] << endl;
    }

    // dump locals vector
    *stream << "L:";
    for( index = 0; index < vector->m_number; index++ ) {
        vf_dump_vector_entry( &vector->m_local[index], stream );
        if( vector->m_local[index].m_is_local ) {
            *stream << " ";
        } else {
            *stream << "!";
        }
    }
    *stream << endl;
    // dump local references
    for( index = 0; index < vector->m_number; index++ ) {
        if( vector->m_local[index].m_type == SM_REF ) {
            *stream << " REF #" << index << ": ";
        } else if( vector->m_local[index].m_type == SM_UNINITIALIZED ) {
            if( vector->m_local[index].m_new ) {
                *stream << "UREF #" << index << ": [" << vector->m_local[index].m_new << "] ";
            } else {
                *stream << "UREF #" << index << ": ";
            }
        } else {
            continue;
        }
        if( vector->m_local[index].m_vtype ) {
            vf_ValidType_t *type = vector->m_local[index].m_vtype;
            *stream << type;
            for( count = 0; count < type->number; count++ ) {
                *stream << " " << type->string[count];
            }
        } else {
            *stream << "NULL";
        }
        *stream << endl;
    }
    // dump stack vector
    *stream << "S:";
    for( index = 0; index < vector->m_deep; index++ ) {
        vf_dump_vector_entry( &vector->m_stack[index], stream );
        if( vector->m_stack[index].m_is_local ) {
            *stream << "!";
        } else {
            *stream << " ";
        }
    }
    *stream << endl;
    // dump stack references
    for( index = 0; index < vector->m_deep; index++ ) {
        if( vector->m_stack[index].m_type == SM_REF ) {
            *stream << " REF #" << index << ": ";
        } else if( vector->m_stack[index].m_type == SM_UNINITIALIZED ) {
            if( vector->m_stack[index].m_new ) {
                *stream << "UREF #" << index << ": [" << vector->m_stack[index].m_new << "] ";
            } else {
                *stream << "UREF #" << index << ": ";
            }
        } else {
            continue;
        }
        if( vector->m_stack[index].m_vtype ) {
            vf_ValidType_t *type = vector->m_stack[index].m_vtype;
            *stream << type;
            for( count = 0; count < type->number; count++ ) {
                *stream << " " << type->string[count];
            }
        } else {
            *stream << "NULL";
        }
        *stream << endl;
    }
    return;
} // vf_dump_vector

#endif // _VERIFY_DEBUG

/**
 * Function compares two valid types. 
 */
bool
vf_is_types_equal( vf_ValidType_t *type1,   // first checked type
                   vf_ValidType_t *type2)   // second checked type
{
    if( type1 == type2 ) {
        return true;
    } else if( type1 == NULL || type2 == NULL || type1->number != type2->number ) {
        return false;
    }
    for( unsigned index = 0; index < type1->number; index++ ) {
        if( type1->string[index] != type2->string[index] ) {
            // types aren't equal
            return false;
        }
    }
    return true;
} // vf_is_types_equal

/**
 * Function merges two vectors and saves result vector to first vector.
 * If first vector was changed, returns true, else - false.
 */
static inline bool
vf_merge_vectors( vf_MapVector_t *first,    // first vector
                  vf_MapVector_t *second,   // second vector
                  bool handler_flag,        // if merged node is handler
                  vf_Context_t *ctex)       // verifier context
{
    bool is_changed = false;
    vf_MapEntry_t zero = {0};

    // check vectors parameters
    assert( first->m_maxlocal == second->m_maxlocal );
    assert( first->m_maxstack == second->m_maxstack );

    // merge local variable vector
    unsigned index;
    for( index = 0; index < first->m_number; index++ ) {
        // merge entries type
        if( first->m_local[index].m_type == SM_TOP ) {
            // no need to merge
            continue;
        } else if( first->m_local[index].m_type != second->m_local[index].m_type ) {
            // types are differ, reset result entry
            first->m_local[index].m_type = SM_TOP;
            first->m_local[index].m_vtype = NULL;
            first->m_local[index].m_is_local = 1;
            first->m_local[index].m_local = (unsigned short)index;
            is_changed = true;
        } else if( first->m_local[index].m_type == SM_REF ) {
            // reference types, merge them
            vf_ValidType_t *type = ctex->m_type->MergeTypes( first->m_local[index].m_vtype,
                                        second->m_local[index].m_vtype );
            if( type ) {
                // set merged type
                first->m_local[index].m_vtype = type;
                is_changed = true;
            }
        } else if( first->m_local[index].m_type == SM_UNINITIALIZED ) {
            // reference types, merge them
            vf_ValidType_t *type = ctex->m_type->MergeTypes( first->m_local[index].m_vtype,
                                        second->m_local[index].m_vtype );
            if( type
                || first->m_local[index].m_new != second->m_local[index].m_new )
            {
                // types are differ, reset result entry
                first->m_local[index].m_type = SM_TOP;
                first->m_local[index].m_new = 0;
                first->m_local[index].m_vtype = NULL;
                first->m_local[index].m_is_local = 1;
                first->m_local[index].m_local = (unsigned short)index;
                is_changed = true;
            }
        }
        // check local variable entries
        assert( first->m_local[index].m_is_local == 1 );
        assert( first->m_local[index].m_local == index );
    }

    // set maximal local variable number
    if( first->m_number < second->m_number ) {
        for( index = first->m_number; index < second->m_number; index++ ) {
            first->m_local[index].m_is_local = 1;
            first->m_local[index].m_local = (unsigned short)index;
        }
        first->m_number = second->m_number;
    }

    // check handler flag
    if( handler_flag ) {
        // no need merge stack for handler node
        return is_changed;
    }

    // merge stack map vector
    assert( first->m_deep == second->m_deep );
    for( index = 0; index < second->m_deep; index++ ) {
        // merge entries type
        if( first->m_stack[index].m_type == SM_TOP ) {
            // no need to merge
            continue;
        } else if( first->m_stack[index].m_type != second->m_stack[index].m_type ) {
            // types are differ, reset result entry
            first->m_stack[index] = zero;
            is_changed = true;
        } else if( first->m_stack[index].m_type == SM_REF ) {
            // reference types, merge them
            vf_ValidType_t *type = ctex->m_type->MergeTypes( first->m_stack[index].m_vtype,
                                        second->m_stack[index].m_vtype );
            if( type ) {
                // set merged type
                first->m_stack[index].m_vtype = type;
                is_changed = true;
            }
        } else if( first->m_stack[index].m_type == SM_UNINITIALIZED ) {
            // reference types, merge them
            vf_ValidType_t *type = ctex->m_type->MergeTypes( first->m_stack[index].m_vtype,
                                        second->m_stack[index].m_vtype );
            if( type
                || first->m_stack[index].m_new != second->m_stack[index].m_new )
            {
                // types are differ, reset result entry
                first->m_stack[index] = zero;
                is_changed = true;
            }
        }
    }
    return is_changed;
} // vf_merge_vectors

/**
 * Function copies source vector to data vector.
 */
static inline void
vf_copy_vector( vf_MapVector_t *source,     // copied vector
                vf_MapVector_t *data)       // data vector
{
    unsigned index;
    vf_MapEntry_t zero = {0};

    assert( source->m_maxlocal <= data->m_maxlocal );
    assert( source->m_maxstack <= data->m_maxstack );
    // copy locals
    data->m_number = source->m_number;
    for( index = 0; index < source->m_number; index++ ) {
        data->m_local[index] = source->m_local[index];
    }
    for( ; index < data->m_maxlocal; index++ ) {
        data->m_local[index] = zero;
    }
    // copy stack
    data->m_deep = source->m_deep;
    for( index = 0; index < source->m_deep; index++ ) {
        data->m_stack[index] = source->m_stack[index];
    }
    for( ; index < data->m_maxstack; index++ ) {
        data->m_stack[index] = zero;
    }
    return;
} // vf_copy_vector

/**
 * Function compares two vectors.
 */
static inline bool
vf_compare_vectors( vf_MapVector_t *first,      // first vector
                    vf_MapVector_t *second)     // second vector
{
    // compare vector parameters
    if( first->m_maxlocal != second->m_maxlocal
        || first->m_maxstack != second->m_maxstack
        || first->m_number != second->m_number
        || first->m_deep != second->m_deep )
    {
        return false;
    }

    // compare locals
    unsigned index;
    for( index = 0; index < first->m_number; index++ ) {
        assert( first->m_local[index].m_is_local == 1 );
        assert( second->m_local[index].m_is_local == 1 );
        if( first->m_local[index].m_local != second->m_local[index].m_local
            || first->m_local[index].m_type != second->m_local[index].m_type
            || first->m_local[index].m_vtype != second->m_local[index].m_vtype )
        {
            return false;
        }
    }
    // compare stack
    for( index = 0; index < first->m_deep; index++ ) {
        if( first->m_stack[index].m_type != second->m_stack[index].m_type
            || first->m_stack[index].m_vtype != second->m_stack[index].m_vtype ) 
        {
            return false;    
        }
    }
    return true;
} // vf_compare_vectors

/**
 * Function check access constraint for two stack map references.
 */
static inline Verifier_Result
vf_check_access(vf_MapEntry_t *source,    // stack map entry
                vf_MapEntry_t *target,    // required map entry
                vf_Context_t *ctex)       // verifier context
{
    // compare types
    assert( target->m_vtype->number == 1 );
    vf_CheckConstraint_t check = (target->m_ctype == VF_CHECK_ACCESS_FIELD )
        ? VF_CHECK_ASSIGN : VF_CHECK_PARAM;
    for( unsigned index = 0; index < source->m_vtype->number; index++ ) {
        // set constraints for differing types
        if( target->m_vtype->string[0] != source->m_vtype->string[index] ) {
            ctex->m_type->SetRestriction( target->m_vtype->string[0],
                source->m_vtype->string[index], 0, check );
        }
        // set access constraints for differing types
        if( ctex->m_vtype.m_class->string[0] != source->m_vtype->string[index] ) {
            // not the same class
            Verifier_Result result = vf_check_access_constraint( target->m_vtype->string[0],
                source->m_vtype->string[index], target->m_index,
                (vf_CheckConstraint_t)target->m_ctype, ctex );
            if( result == VER_ClassNotLoaded ) {
                // cannot complete check, set restriction
                ctex->m_type->SetRestriction( ctex->m_vtype.m_class->string[0],
                    source->m_vtype->string[index], 0,
                    (vf_CheckConstraint_t)target->m_ctype );
            } else if( result != VER_OK ) {
                // return error
                return result;
            }
        }
    }
    return VER_OK;
} // vf_check_access

/**
 * Function checks two stack map references.
 */
static inline Verifier_Result
vf_check_entry_refs( vf_MapEntry_t *source,    // stack map entry
                     vf_MapEntry_t *target,    // required map entry
                     bool local_init,          // initialization flag of locals
                     vf_Context_t *ctex)       // verifier context
{
    // check entries type
    if( !(source->m_type == SM_REF || source->m_type == SM_UNINITIALIZED)
        || !(target->m_type == SM_REF || target->m_type == SM_UNINITIALIZED) )
    {
        return VER_ErrorDataFlow;
    }
    // check local variable type
    if( source->m_is_local 
        && (!target->m_is_local || source->m_local != target->m_local) )
    {
        return VER_ErrorDataFlow;
    }
    // check available entry
    if( source->m_vtype == NULL ) {
        // nothing checks
        return VER_OK;
    }
    // check initialization
    if( source->m_type == SM_UNINITIALIZED && target->m_type != SM_UNINITIALIZED) {
        if( (source->m_new == 0 && target->m_ctype == VF_CHECK_ACCESS_FIELD)
            || (local_init == false && target->m_ctype == VF_CHECK_UNINITIALIZED_THIS)
            || (!ctex->m_dump.m_verify && source->m_new == 0
                    && target->m_ctype == VF_CHECK_UNINITIALIZED_THIS) )
        {
            // 1. In initialization method instance fields of this
            //    that are declared in the current class may be assigned
            //    before calling any instance initialization method.
            // 2. Uninitialized class instance can be stored in
            //    a local variable if no backward branch is taken or
            //    the code isn't protected by exception handler.
            // 3. In default mode (without any verifier control options)
            //    uninitiliazed class reference in initialization method
            //    can be stored in a local variable if backward branch is
            //    taken or the code is protected by exception handler.
        } else {
            VERIFY_REPORT( ctex, "(class: " << class_get_name( ctex->m_class ) 
                << ", method: " << method_get_name( ctex->m_method )
                << method_get_descriptor( ctex->m_method )
                << ") Uninitialized reference usage" );
            return VER_ErrorDataFlow;
        }
    }

    // check references
    bool is_error = false;
    switch( target->m_ctype )
    {
    case VF_CHECK_NONE:
    case VF_CHECK_UNINITIALIZED_THIS:
    case VF_CHECK_PARAM:                // check method invocation conversion
        if( target->m_vtype != NULL ) {
            is_error = ctex->m_type->CheckTypes( target->m_vtype,
                            source->m_vtype, 0, VF_CHECK_PARAM );
        }
        break;
    case VF_CHECK_ARRAY:    // check if source reference is array 
        is_error = ctex->m_type->CheckTypes( NULL, source->m_vtype, 0, VF_CHECK_ARRAY );
        break;
    case VF_CHECK_REF_ARRAY:
        is_error = ctex->m_type->CheckTypes( NULL, source->m_vtype, 0, VF_CHECK_REF_ARRAY );
        break;
    case VF_CHECK_EQUAL:    // check if references are equal
        is_error = ctex->m_type->CheckTypes( target->m_vtype,
                        source->m_vtype, 0, VF_CHECK_EQUAL );
        break;
    case VF_CHECK_ASSIGN:       // check assignment conversion
    case VF_CHECK_ASSIGN_WEAK:  // check weak assignment conversion
        assert( target->m_vtype != NULL );
        is_error = ctex->m_type->CheckTypes( target->m_vtype,
            source->m_vtype, 0, (vf_CheckConstraint_t)target->m_ctype );
        break;
    case VF_CHECK_ACCESS_FIELD:     // check field access
    case VF_CHECK_ACCESS_METHOD:    // check method access
        {
            assert( target->m_vtype != NULL );      // not null reference
            assert( target->m_local != 0 );         // constant pool index is set
            Verifier_Result result = vf_check_access( source, target, ctex );
            return result;
        }
        break;
    case VF_CHECK_DIRECT_SUPER:     // check is target class is direct super class of source
        is_error = ctex->m_type->CheckTypes( target->m_vtype, source->m_vtype,
            0, VF_CHECK_DIRECT_SUPER );
        break;
    case VF_CHECK_INVOKESPECIAL:    // check invokespecial object reference
        is_error = ctex->m_type->CheckTypes( target->m_vtype, source->m_vtype,
                    0, VF_CHECK_INVOKESPECIAL );
        break;
    default:
        DIE( "Verifier: vf_check_entry_refs: unknown check in switch" );
    }
    // check error
    if( is_error ) {
        return VER_ErrorDataFlow;
    }
 
    return VER_OK;
} // vf_check_entry_refs

/**
 * Function checks two stack map entries.
 */
static inline Verifier_Result
vf_check_entry_types( vf_MapEntry_t *entry1,    // stack map entry
                      vf_MapEntry_t *entry2,    // required map entry
                      bool local_init,          // initialization flag of locals
                      bool *need_copy,          // pointer to copy flag
                      vf_Context_t *ctex)       // verifier context
{
    switch( entry2->m_type )
    {
    case SM_REF:
        // check reference entries
        {
            Verifier_Result result = 
                vf_check_entry_refs( entry1, entry2, local_init, ctex );
            *need_copy = true;
            return result;
        }
        break;
    case SM_TOP:
    case SM_INT:
    case SM_FLOAT:
    case SM_LONG_LO:
    case SM_DOUBLE_LO:
    case SM_LONG_HI:
    case SM_DOUBLE_HI:
        // check entries type
        if( entry1->m_type != entry2->m_type ) {
            return VER_ErrorDataFlow;
        }
        break;
    case SM_WORD:
        // check not strict correspondence types
        if( entry1->m_type >= SM_LONG_LO ) {
            return VER_ErrorDataFlow;
        }
        *need_copy = true;
        break;
    case SM_WORD2_LO:
        // check not strict correspondence types
        if( entry1->m_type >= SM_LONG_HI ) {
            return VER_ErrorDataFlow;
        }
        *need_copy = true;
        break;
    case SM_WORD2_HI:
        // check not strict correspondence types
        if( entry1->m_type >= SM_LONG_LO && 
            entry1->m_type != SM_LONG_HI && entry1->m_type != SM_DOUBLE_HI )
        {
            return VER_ErrorDataFlow;
        }
        *need_copy = true;
        break;
    default:
        return VER_ErrorDataFlow;
    }

    // check local variable type
    if( entry1->m_is_local 
        && (!entry2->m_is_local || entry1->m_local != entry2->m_local) )
    {
        return VER_ErrorDataFlow;
    }
    return VER_OK;
} // vf_check_entry_types

/**
 * Function creates array element valid type.
 */
static inline void
vf_set_array_element_type( vf_MapEntry_t *buf,      // result data flow vector entry
                           vf_MapEntry_t *vector,   // data flow vector entry
                           vf_MapEntry_t *array,    // array data flow vector entry
                           vf_Context_t *ctex)      // verifier context
{
    assert( array->m_type == SM_REF );
    buf->m_type = array->m_type;
    buf->m_vtype = array->m_vtype;
    buf->m_local = vector->m_local;
    buf->m_is_local = vector->m_is_local;
    if( buf->m_vtype ) {
        // create cell array type
        buf->m_vtype = ctex->m_type->NewArrayElemType( buf->m_vtype );
    }
    return;
} // vf_set_array_element_type

/**
 * Function receives IN data flow vector entry for code instruction.
 */
static inline vf_MapEntry_t *
vf_set_new_in_vector( vf_Code_t *code,              // code instruction
                      vf_MapEntry_t *vector,        // data flow vector entry
                      vf_MapEntry_t *stack_buf,     // stack map vector
                      vf_Context_t *ctex)           // verifier context

{
    short number;
    vf_MapEntry_t *newvector;

    switch( vector->m_type )
    {
    case SM_COPY_0:
    case SM_COPY_1:
    case SM_COPY_2:
    case SM_COPY_3:
        // it's a copy type, copy value from IN vector
        number = (short)(vector->m_type - SM_COPY_0);
        assert( number < code->m_inlen );
        newvector = &code->m_invector[number];
        if( newvector->m_type == SM_TOP
            || newvector->m_type >= SM_WORD
            || (newvector->m_type == SM_REF && newvector->m_vtype == NULL) )
        {
            // copy value from saved stack entry
            newvector = &stack_buf[number];
            newvector->m_local = vector->m_local;
            newvector->m_is_local = vector->m_is_local;
        }
        break;
    case SM_UP_ARRAY:
        // set reference array element
        newvector = &stack_buf[0];
        vf_set_array_element_type( newvector, vector, &stack_buf[0], ctex );
        break;
    default:
        newvector = vector;
        break;
    }
    return newvector;
} // vf_set_new_in_vector

/**
 * Function sets OUT data flow vector for code instruction.
 */
static inline void
vf_set_instruction_out_vector( vf_Code_t *code,         // code instruction
                               vf_MapEntry_t *stack,    // stack map vector
                               vf_MapEntry_t *locals,   // local variable vector
                               unsigned short *number,  // pointer to local variables number
                               vf_MapEntry_t *buf,      // memory buf array
                               vf_Context_t *ctex)      // verifier context
{
    unsigned index;
    vf_MapEntry_t *entry,
                  *vector;

    // set instruction out vector
    for( index = 0, vector = code->m_outvector;
         index < code->m_outlen;
         index++, vector = &code->m_outvector[index] )
    {
        vf_MapEntry_t *newvector = vf_set_new_in_vector( code, vector, buf, ctex );
        if( newvector->m_is_local ) {
            // get local variable
            entry = locals + newvector->m_local;
            // set local variable type
            entry->m_local = newvector->m_local;
            entry->m_is_local = 1;
            // set locals vector number
            if( newvector->m_local + 1 > (*number) ) {
                *number = (unsigned short)(newvector->m_local + 1);
            }
        } else {
            // get stack entry
            entry = stack + index;
        }
        // set entry
        entry->m_type = newvector->m_type;
        entry->m_new = newvector->m_new;
        entry->m_vtype = newvector->m_vtype;
    }
    return;
} // vf_set_instruction_out_vector

/**
 * Function clears stack map vector.
 */
static inline void
vf_clear_stack( vf_MapVector_t *vector )    // map vector
{
    vf_MapEntry_t zero_entry = {0};

    // zero stack vector
    for( unsigned index = 0; index < vector->m_deep; index++ ) {
        vector->m_stack[index] = zero_entry;
    }
    vector->m_deep = 0;

    return;
} // vf_clear_stack

/**
 * Function sets stack map vector for return instructions.
 */
static inline void
vf_set_return_out_vector( vf_MapEntry_t *stack,     // stack map vector
                          vf_MapEntry_t *buf,       // stored values vector
                          vf_MapEntry_t *invector,  // IN data flow vector
                          unsigned len,             // IN vector length
                          vf_Context_t * UNREF ctex)       // verifier context
{
    unsigned index;
    vf_MapEntry_t *entry;

    // set return instruction out vector
    for( index = 0, entry = invector;
         index < len;
         index++, entry = &invector[index] )
    {
        if( entry->m_type == SM_REF ) {
            stack[index].m_type = buf[index].m_type;
            stack[index].m_vtype = buf[index].m_vtype;
        } else {
            stack[index].m_type = entry->m_type;
        }
    }
    return;
} // vf_set_return_out_vector

/**
 * Function checks data flow for code instruction.
 */
static inline Verifier_Result
vf_check_instruction_in_vector( vf_MapEntry_t *stack,       // stack map vector
                                vf_MapEntry_t *locals,      // local variable vector
                                vf_MapEntry_t *buf,         // buf storage vector
                                vf_MapEntry_t *invector,    // code instruction IN vector
                                unsigned len,               // IN vector length
                                bool local_init,            // initialization flag of locals
                                bool *need_init,            // init uninitialized entry
                                vf_Context_t *ctex)         // verifier context
{
    unsigned index;
    vf_MapEntry_t *entry,
                  *vector,
                  *newvector;
    Verifier_Result result;

    // check IN vector entries
    for( index = 0, vector = invector;
         index < len;
         index++, vector = &invector[index] )
    {
        // set copy flag
        bool copy = false;
        // get check entry
        entry = vector->m_is_local ? locals + vector->m_local : stack + index;
        switch( vector->m_type )
        {
        case SM_TOP:
            // copy entry
            copy = true;
            break;
        case SM_UP_ARRAY:
            // check reference array element
            assert( index > 0 );
            newvector = &buf[index];
            // check assignment conversion
            vf_set_array_element_type( newvector, vector, &buf[0], ctex );
            if( newvector->m_vtype ) {
                newvector->m_ctype = VF_CHECK_ASSIGN_WEAK;
            } else {
                newvector->m_ctype = VF_CHECK_NONE;
            }
            // check entry types
            result = vf_check_entry_refs( entry, newvector, local_init, ctex );
            if( result != VER_OK ) {
                VERIFY_REPORT( ctex, "(class: " << class_get_name( ctex->m_class )
                    << ", method: " << method_get_name( ctex->m_method )
                    << method_get_descriptor( ctex->m_method )
                    << ") Incompatible types for array assignment" );
                return result;
            }
            break;
        case SM_REF:
            // check entry references
            result = vf_check_entry_refs( entry, vector, local_init, ctex );
            if( result != VER_OK ) {
                if( !ctex->m_error ) {
                    VERIFY_REPORT( ctex, "(class: " << class_get_name( ctex->m_class ) 
                        << ", method: " << method_get_name( ctex->m_method )
                        << method_get_descriptor( ctex->m_method )
                        << ") Data flow analysis error" );
                }
                return result;
            }
            copy = true;
            break;
        case SM_UNINITIALIZED:
            // check entry references
            if( entry->m_type == SM_REF ) {
                // double initialization
                VERIFY_REPORT( ctex, "(class: " << class_get_name( ctex->m_class ) 
                    << ", method: " << method_get_name( ctex->m_method )
                    << method_get_descriptor( ctex->m_method )
                    << ") Double initialization of object reference" );
                return VER_ErrorDataFlow;
            }
            result = vf_check_entry_refs( entry, vector, local_init, ctex );
            if( result != VER_OK ) {
                VERIFY_REPORT( ctex, "(class: " << class_get_name( ctex->m_class ) 
                    << ", method: " << method_get_name( ctex->m_method )
                    << method_get_descriptor( ctex->m_method )
                    << ") Data flow analysis error (uninitialized)" );
                return result;
            }
            // check initialization class in constructor
            if( entry->m_type == SM_UNINITIALIZED && entry->m_new == 0 ) {
                // initialization of this reference in class construction
                assert( entry->m_vtype->number == 1 );
                if( vector->m_vtype->string[0] != entry->m_vtype->string[0] ) {
                    ctex->m_type->SetRestriction( vector->m_vtype->string[0],
                        entry->m_vtype->string[0], 0, VF_CHECK_DIRECT_SUPER );
                }
            }
            *need_init = true;
            copy = true;
            break;
        default:
            // check entry types
            result = vf_check_entry_types( entry, vector, local_init, &copy, ctex );
            if( result != VER_OK ) {
                VERIFY_REPORT( ctex, "(class: " << class_get_name( ctex->m_class ) 
                    << ", method: " << method_get_name( ctex->m_method )
                    << method_get_descriptor( ctex->m_method )
                    << ") Data flow analysis error" );
                return result;
            }
        }
        // copy entry
        if( copy ) {
            buf[index].m_type = entry->m_type;
            buf[index].m_new = entry->m_new;
            buf[index].m_vtype = entry->m_vtype;
        }
    }
    return VER_OK;
} // vf_check_instruction_in_vector

/**
 * Function receives code instruction OUT data flow vector.
 */
static Verifier_Result
vf_get_instruction_out_vector( unsigned node_num,           // graph node
                               vf_Code_t *instr,            // code instruction
                               vf_MapVector_t *invector,    // incoming data flow vector
                               vf_MapEntry_t *buf,          // buf storage vector
                               vf_Context_t *ctex)          // verifier context
{
    unsigned index;
    bool need_init = false;
    vf_MapEntry_t zero_entry = {0};

    // set stack vector
    assert( invector->m_deep - instr->m_minstack >= 0 );
    vf_MapEntry_t *stack = invector->m_stack + invector->m_deep - instr->m_minstack;
    // set locals vector
    vf_MapEntry_t *locals = invector->m_local;
    // check instruction in vector
    Verifier_Result result = vf_check_instruction_in_vector( stack, locals, buf,
        instr->m_invector, instr->m_inlen, ctex->m_graph->GetNodeInitFlag(node_num),
        &need_init, ctex );
    if( result != VER_OK ) {
        return result;
    }

    // create out vector for return instructions
    if( vf_is_instruction_has_flags( instr, VF_FLAG_RETURN ) ) {
        // clear stack
        unsigned deep = invector->m_deep;
        vf_clear_stack( invector );
        // set result vector stack deep
        invector->m_deep = (unsigned short)(deep + instr->m_stack);
        // set out vector
        vf_set_return_out_vector( invector->m_stack, buf, instr->m_invector,
                                  instr->m_inlen, ctex );
        return VER_OK;
    } else if( vf_is_instruction_has_flags( instr, VF_FLAG_THROW ) ) {
        // set result vector stack deep
        invector->m_stack->m_type = SM_TERMINATE;
        return VER_OK;
    }

    // zero stack entries
    for( index = 0; index < instr->m_minstack; index++ ) {
        stack[index] = zero_entry;
    }

    // init uninitialized entry
    if( need_init ) {
        assert( buf[0].m_type == SM_UNINITIALIZED );
        // init local variables reference
        for( index = 0; index < invector->m_number; index++ ) {
            if( invector->m_local[index].m_type == SM_UNINITIALIZED
                && invector->m_local[index].m_new == buf[0].m_new )
            {
                assert( invector->m_local[index].m_vtype == buf[0].m_vtype );
                invector->m_local[index].m_type = SM_REF;
            }
        }
        // init stack reference
        for( index = 0; index < (unsigned)invector->m_deep - instr->m_minstack; index++ ) {
            if( invector->m_stack[index].m_type == SM_UNINITIALIZED
                && invector->m_stack[index].m_new == buf[0].m_new )
            {
                assert( invector->m_stack[index].m_vtype == buf[0].m_vtype );
                invector->m_stack[index].m_type = SM_REF;
            }
        }
    }

    // set instruction OUT vector
    invector->m_deep = (unsigned short )(invector->m_deep + instr->m_stack);
    assert( invector->m_deep <= invector->m_maxstack );
    index = invector->m_number;
    vf_set_instruction_out_vector( instr, stack, locals, &invector->m_number, buf, ctex );
    assert( invector->m_number <= invector->m_maxlocal );
    // set local variable numbers
    for( ; index < invector->m_number; index++ ) {
        if( !locals[index].m_is_local ) {
            locals[index].m_is_local = 1;
            locals[index].m_local = index;
        }
    }

    return VER_OK;
} // vf_get_instruction_out_vector

/**
 * Function receives handler OUT data flow vector.
 */
static inline Verifier_Result
vf_get_handler_out_vector( vf_Code_t *instr,            // handler code instruction
                           vf_MapVector_t *invector)    // incoming data flow vector
{
    // set handler out vector
    assert( invector->m_deep == 0 );
    assert( instr->m_outlen == 1 );
    invector->m_stack->m_type = instr->m_outvector->m_type;
    invector->m_stack->m_vtype = instr->m_outvector->m_vtype;
    // set modify vector value
    invector->m_deep++;
    return VER_OK;
} // vf_get_handler_out_vector

/**
 * Function sets graph node OUT data flow vector.
 */
static Verifier_Result
vf_set_node_out_vector( unsigned node_num,          // graph node number
                        vf_MapVector_t *invector,   // incoming data flow vector
                        vf_MapEntry_t *buf,         // buf stack map vector
                        vf_Context_t *ctex)         // verifier context
{
    unsigned index,
             instruction;
    vf_Code_t *instr;
    Verifier_Result result;

    // get node instruction number
    vf_Graph_t *graph = ctex->m_graph;
    instruction = graph->GetNodeLastInstr( node_num ) 
        - graph->GetNodeFirstInstr( node_num ) + 1;
    // get first instruction
    instr = &ctex->m_code[graph->GetNodeFirstInstr( node_num )];

    /** 
     * For start-entry node doesn't need to check data flow
     */
    if( vf_is_instruction_has_flags( instr, VF_FLAG_START_ENTRY ) ) {
        assert( instruction == 1 );
        return VER_OK;
    } else if( vf_is_instruction_has_flags( instr, VF_FLAG_HANDLER ) ) {
        assert( instruction == 1 );
        // set out vector for handler node
        result = vf_get_handler_out_vector( instr, invector );
        if( result != VER_OK ) {
            return result;
        }
    } else {
        // set out vector for each instruction
        for( index = 0; index < instruction; index++ ) {
            if( instr[index].m_inlen + instr[index].m_outlen == 0 
                && !vf_is_instruction_has_any_flags( instr ) )
            {
                continue;
            } else {
                result = vf_get_instruction_out_vector( node_num, &instr[index], invector, 
                    buf, ctex );
            }
            if( result != VER_OK ) {
                return result;
            }
#if _VERIFY_DEBUG
            if( ctex->m_dump.m_code_vector ) {
                // dump instruction OUT vector
                cerr << "-------------- instruction #" << index << " out: " << endl;
                vf_dump_vector( invector, &instr[index], &cerr );
            }
#endif // _VERIFY_DEBUG
        }
    }

    return VER_OK;
} // vf_set_node_out_vector

/**
 * Function creates and sets graph node OUT vector.
 */
static Verifier_Result
vf_create_node_vectors( unsigned node_num,          // graph node number
                        vf_MapVector_t *incoming,   // vector for instruction data flow change
                        vf_MapEntry_t *buf,         // buf stack map vector
                        bool *is_out_changed,       // pointer to OUT vector change flag
                        vf_Context_t *ctex)         // verifier context
{
    // copy IN vector to buf
    vf_Graph_t *graph = ctex->m_graph;
    vf_copy_vector( graph->GetNodeInVector( node_num ), incoming );

#if _VERIFY_DEBUG
    if( ctex->m_dump.m_code_vector || ctex->m_dump.m_node_vector ) {
        // dump node number
        cerr << endl << "-------------- Node #" << node_num << endl;
        if( ctex->m_dump.m_node_vector ) {
            // dump in vector
            cerr << "IN vector :" << endl;
            vf_dump_vector( incoming, NULL, &cerr );
        }
    }
#endif // _VERIFY_DEBUG

    // calculate OUT node vector
    Verifier_Result result = vf_set_node_out_vector( node_num, incoming, buf, ctex );
    if( result != VER_OK ) {
        return result;
    }

    // set node OUT vector
    vf_MapVector_t *outcoming = graph->GetNodeOutVector( node_num );
    if( !outcoming->m_maxlocal || !outcoming->m_maxstack )
    {
        // create node OUT vector
        graph->SetNodeOutVector( node_num, incoming, true );
        outcoming = graph->GetNodeOutVector( node_num );
        *is_out_changed = true;
    } else if( !vf_compare_vectors( outcoming, incoming ) ) {
        // vectors are differ
        vf_copy_vector( incoming, outcoming );
        *is_out_changed = true;
    }

#if _VERIFY_DEBUG
    if( ctex->m_dump.m_node_vector ) {
        // dump out vector
        cerr << "-------------- Node #" << node_num << endl <<"OUT vector :" << endl;
        vf_dump_vector( outcoming, NULL, &cerr );
    }
#endif // _VERIFY_DEBUG

    // check stack modifier
    assert( (int)((graph->GetNodeOutVector( node_num )->m_deep
        - graph->GetNodeInVector( node_num )->m_deep))
        == graph->GetNodeStackModifier( node_num ) );

    return VER_OK;
} // vf_create_node_vectors

/**
 * Function checks data flow for end graph node.
 */
static Verifier_Result
vf_check_end_node_data_flow( unsigned node_num,         // graph node number
                             vf_MapVector_t *invector,  // end node incoming data flow vector
                             vf_Context_t *ctex)        // verifier context
{
    bool copy;
    unsigned index;
    vf_MapEntry_t *vector;

    // don't need check
    if( invector->m_stack && invector->m_stack->m_type == SM_TERMINATE ) {
        return VER_OK;
    }

    // check <init> method
    if( !memcmp( method_get_name( ctex->m_method ), "<init>", 7 )
        && ctex->m_vtype.m_class != ctex->m_vtype.m_object )
    {
        if( invector->m_local->m_type != SM_UNINITIALIZED
            && invector->m_local->m_vtype == ctex->m_vtype.m_class)
        {
            // constructor returns initialized reference of a given class
        } else {
            VERIFY_REPORT( ctex, "(class: " << class_get_name( ctex->m_class ) 
                << ", method: " << method_get_name( ctex->m_method )
                << method_get_descriptor( ctex->m_method )
                << ") Constructor must be invoked" );
            return VER_ErrorDataFlow;
        }
    }

    // get first instruction
    vf_Code_t *instr = &ctex->m_code[ctex->m_graph->GetNodeFirstInstr( node_num )];

    // check void return
    if( !instr->m_inlen ) {
        if( invector->m_stack ) {
            if( invector->m_stack->m_type == SM_TOP ) {
                // no return value, empty stack - all is ok
                return VER_OK;
            }
        } else {
            // no stack, no return value - all is ok
            return VER_OK;
        }
        VERIFY_REPORT( ctex, "(class: " << class_get_name( ctex->m_class ) 
            << ", method: " << method_get_name( ctex->m_method )
            << method_get_descriptor( ctex->m_method )
            << ") Wrong return type in function" );
        return VER_ErrorDataFlow;
    }
    assert( invector->m_deep - instr->m_minstack >= 0 );

    /**
     * Check end-entry IN vector
     */
    for( index = 0, vector = &instr->m_invector[0];
         index < instr->m_inlen;
         index++, vector = &instr->m_invector[index] )
    {
        // get check entry
        assert( vector->m_is_local == 0 );
        // check stack type
        vf_MapEntry_t *entry = invector->m_stack + index;
        // check entry types
        Verifier_Result result = vf_check_entry_types( entry, vector, true, &copy, ctex );
        if( result != VER_OK ) {
            VERIFY_REPORT( ctex, "(class: " << class_get_name( ctex->m_class ) 
                << ", method: " << method_get_name( ctex->m_method )
                << method_get_descriptor( ctex->m_method )
                << ") Wrong return type in function" );
            return result;
        }
    }
    return VER_OK;
} // vf_check_end_node_data_flow

/**
 * Function checks data flow for graph node.
 */
static Verifier_Result
vf_check_node_data_flow( unsigned node_num,             // graph node number
                         vf_MapVector_t *incoming,      // incoming data flow vector
                         vf_MapEntry_t *buf,            // buf stack map vector
                         unsigned *node_count,          // last graph node in recursion
                         bool *need_recheck,            // set to true if need to recheck previous nodes
                         vf_Context_t *ctex)            // verifier context
{
    // get graph
    vf_Graph_t *graph = ctex->m_graph;

    // skip end-entry node
    if( vf_is_instruction_has_flags( &ctex->m_code[graph->GetNodeFirstInstr( node_num )],
            VF_FLAG_END_ENTRY) )
    {
        return VER_OK;
    }

    // set node vectors
    bool is_changed = false;
    int node_mark = graph->GetNodeMark( node_num );
    if( !node_mark ) {
        // node isn't end-entry node, end-entry node no need to check
        Verifier_Result result = vf_create_node_vectors( node_num,
            incoming, buf, &is_changed, ctex );
        if( result != VER_OK ) {
            return result;
        }
        graph->SetNodeMark( node_num, VERIFY_START_MARK );
    }
    if( !is_changed ) {
        /**
         * Node OUT vector isn't changed,
         * no need change data flow vectors for following out nodes.
         */
        return VER_OK;
    }

    // set incoming vector for following nodes
    vf_MapVector_t *in_node_vector = graph->GetNodeOutVector( node_num );
    for( unsigned out_edge = graph->GetNodeFirstOutEdge( node_num );
         out_edge;
         out_edge = graph->GetEdgeNextOutEdge( out_edge ) )
    {
        // get in node, mark and enumeration count
        unsigned out_node = graph->GetEdgeEndNode( out_edge );

        // check vectors for end-entry
        if( vf_is_instruction_has_flags( 
            &ctex->m_code[graph->GetNodeFirstInstr( out_node )],
            VF_FLAG_END_ENTRY) )
        {
            Verifier_Result result = vf_check_end_node_data_flow( out_node,
                                        in_node_vector, ctex );
            if( result != VER_OK ) {
                return result;
            }
            continue;
        }

        // get out node IN vector
        vf_MapVector_t *out_node_vector = graph->GetNodeInVector( out_node );
        if( !out_node_vector->m_maxlocal || !out_node_vector->m_maxstack )
        {
            // node's IN vector is invalid, set it
            if( vf_is_instruction_has_flags( 
                    &ctex->m_code[graph->GetNodeFirstInstr( out_node )],
                    VF_FLAG_HANDLER) )
            {
                // it's exception node, create IN vector for it
                vf_copy_vector( in_node_vector, incoming );
                vf_clear_stack( incoming );
                graph->SetNodeInVector( out_node, incoming, true );
            } else {
                // other nodes
                graph->SetNodeInVector( out_node, in_node_vector, true );
            }
        } else {
#if _VERIFY_DEBUG
            if( ctex->m_dump.m_merge_vector ) {
                // copy out node IN vector for dump
                vf_copy_vector( out_node_vector, incoming );
            }
#endif // _VERIFY_DEBUG
            // node's IN vector is valid, merge them
            bool is_handler = vf_is_instruction_has_flags( 
                    &ctex->m_code[graph->GetNodeFirstInstr( out_node )],
                    VF_FLAG_HANDLER);
            is_changed = vf_merge_vectors( out_node_vector, in_node_vector, is_handler, ctex );
            if( is_changed ) {
                // node IN vector is changed, reset node OUT vector results
                graph->SetNodeMark( out_node, 0 );
                // set node for re-verification if it's needed.
                unsigned count = graph->GetNodeCountElement( out_node );
                if( count <= (*node_count) ) {
                    *node_count = count;
                    *need_recheck = true;
                }
            }
#if _VERIFY_DEBUG
            if( ctex->m_dump.m_merge_vector && is_changed ) {
                // dump out vectors
                cerr << "============== merge IN vector for Node #" << out_node << endl;
                cerr << "IN vectors:" << endl;
                cerr << "1: --------------" << endl;
                vf_dump_vector( incoming, NULL, &cerr );
                cerr << "2: --------------" << endl;
                vf_dump_vector( in_node_vector, NULL, &cerr );
                // dump out vector
                cerr << "result: --------------" << endl;
                vf_dump_vector( out_node_vector, NULL, &cerr );
                cerr << "### Recount from " << *node_count
                     << " (now " << graph->GetNodeCountElement( node_num ) << ")" << endl;
            }
#endif // _VERIFY_DEBUG
        }
    }

    return VER_OK;
} // vf_check_node_data_flow

/**
 * Function creates initial data flow vector for method.
 */
static vf_MapVector_t*
vf_create_method_begin_vector( vf_Context_t *ctex )     // verifier context
{
    int index,
        count,
        inlen,
        begin,
        outlen;
    unsigned locals,
             maxstack;
    vf_MapEntry_t *invector,
                  *outvector;
    vf_MapVector_t *vector;

    // get method values
    locals = method_get_max_local( ctex->m_method );
    maxstack = method_get_max_stack( ctex->m_method );

    // alloc memory for vector structure
    vector = (vf_MapVector_t*)ctex->m_graph->AllocMemory( sizeof(vf_MapVector_t) );
    // alloc memory for stack vector
    if( maxstack ) {
        vector->m_maxstack = (unsigned short)maxstack;
        vector->m_stack = (vf_MapEntry_t*)ctex->m_graph->
            AllocMemory( maxstack * sizeof(vf_MapEntry_t) );
    }
    // alloc memory for locals vector
    if( locals ) {
        vector->m_maxlocal = (unsigned short)locals;
        vector->m_local = (vf_MapEntry_t*)ctex->m_graph->
            AllocMemory( locals * sizeof(vf_MapEntry_t) );
    }

    // get method signature
    const char *descr = method_get_descriptor( ctex->m_method );

    // get method vectors
    vf_parse_description( descr, &inlen, &outlen );
    vf_set_description_vector( descr, inlen, 0, outlen, &invector, &outvector, ctex );

    // set "this" reference in local variable
    if( method_is_static( ctex->m_method ) ) {
        begin = 0;
    } else {
        begin = 1;
        // fill "this" entry
        const char *name = class_get_name( ctex->m_class );
        vf_ValidType_t *type = vf_create_class_valid_type( name, ctex );
        if( !memcmp( method_get_name( ctex->m_method ), "<init>", 7 ) ) {
            vector->m_local->m_type = SM_UNINITIALIZED;
        } else {
            vector->m_local->m_type = SM_REF;
        }
        vector->m_local->m_vtype = type;
        vector->m_local->m_is_local = 1;
        vector->m_local->m_local = 0;
    }

    // set start vector
    for( index = begin, count = 0; count < inlen; index++, count++ ) {
        vector->m_local[index].m_type = invector[count].m_type;
        vector->m_local[index].m_vtype = invector[count].m_vtype;
        vector->m_local[index].m_is_local = 1;
        vector->m_local[index].m_local = (unsigned short)index;
    }
    vector->m_number = (unsigned short)index;

    // FIXME - need set end entry vector
    return vector;
} // vf_create_method_begin_vector

/**
 * Function enumerates graph nodes by wave numeration.
 */
void
vf_enumerate_graph_node( vf_Context_t *ctex )
{
    // clear graph node marks
    vf_Graph_t *graph = ctex->m_graph;
    graph->CleanNodesMark();

    // set first enumeration node
    graph->SetStartCountNode(0);
    graph->SetNodeMark( 0, VERIFY_START_MARK );

    // enumerate graph nodes
    for( unsigned index = 0; index < graph->GetNodeNumber(); index++ ) {
        // get node by count element
        unsigned node_num = graph->GetCountElementNode( index );
        if( node_num == ~0U ) {
            // remove dead nodes from enumeration
            continue;
        }

        // get node mark
        int mark = graph->GetNodeMark( node_num );

        // override all out edges of node
        for( unsigned out_edge = graph->GetNodeFirstOutEdge( node_num );
             out_edge;
             out_edge = graph->GetEdgeNextOutEdge( out_edge ) )
        {
            // get out node and its mark
            unsigned out_node = graph->GetEdgeEndNode( out_edge );
            int out_node_mark = graph->GetNodeMark( out_node );
            if( !out_node_mark ) {
                // it's unnumerated node, enumerate it
                graph->SetNextCountNode( out_node );
                graph->SetNodeMark( out_node, mark + 1 );
            }
        }
    }
    return;
} // vf_enumerate_graph_node

/**
 * Function provides data flow checks of verifier graph structure.
 */
Verifier_Result
vf_check_graph_data_flow( vf_Context_t *ctex )  // verifier context
{
    if( method_get_max_local( ctex->m_method ) + method_get_max_stack( ctex->m_method ) == 0 ) {
        // nothing verify
        return VER_OK;
    }

    // enumerate graph
    vf_enumerate_graph_node( ctex );

    // get begin vector
    vf_MapVector_t *begin = vf_create_method_begin_vector( ctex );

    // create buf stack map vector (max 4 entry)
    vf_MapEntry_t *buf = (vf_MapEntry_t*)vf_alloc_pool_memory( ctex->m_pool,
            sizeof(vf_MapEntry_t) * method_get_max_stack( ctex->m_method ) );

    // clean graph mark
    vf_Graph_t *graph = ctex->m_graph;
    graph->CleanNodesMark();

    // set start node IN vector
    graph->SetNodeInVector( 0, begin, true );

    // check graph data flow
    bool need_recheck = false;
    unsigned count = 0;
    do {
        unsigned node = graph->GetCountElementNode( count );
        Verifier_Result result = vf_check_node_data_flow( node, begin, buf, &count, &need_recheck, ctex );
        if( result != VER_OK ) {
            return result;
        }
        if( !need_recheck ) {
            // check next node
            count++;
        } else {
            need_recheck = false;
        }
    } while( count < graph->GetEnumCount() );

    return VER_OK;
} // vf_check_graph_data_flow

} // namespace Verifier
