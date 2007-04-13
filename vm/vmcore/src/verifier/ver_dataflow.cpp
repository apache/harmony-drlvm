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
#include "ver_graph.h"

/**
 * Debug flag macros
 */
// Macro prints data flow node vectors
#define DUMP_NODE_VECTOR       0
// Macro prints data flow node instruction vectors
#define DUMP_NODE_INSTR_VECTOR 0

/************************************************************
 **************** Graph Data Flow Analysis ******************
 ************************************************************/

#if _VF_DEBUG

/**
 * Prints stack map entry into output stream.
 */
static void
vf_dump_vector_entry( vf_MapEntry *entry,       // stack map entry
                      ostream *stream ) // output stream
{
    switch ( entry->m_type ) {
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
    case SM_ANY:
        *stream << " [ANY ]";
        break;
    default:
        *stream << " [? " << entry->m_type << "]";
    }
}                               // vf_dump_vector_entry

/**
 * Prints data flow vector into output stream.
 */
void
vf_dump_vector( vf_MapVectorHandle vector,      // data flow vector
                vf_InstrHandle instr,   // code instruction
                ostream *stream )       // output stream (can be NULL)
{
    unsigned index, count;

    // set steam if it's needed
    if( stream == NULL ) {
        stream = &cerr;
    }
    // dump code instruction if it's needed
    if( instr != NULL ) {
        *stream << ( ( instr->m_stack < 0 ) ? "[" : "[ " )
            << instr->m_stack << "| " << instr->m_minstack << "] "
            << vf_opcode_names[*( instr->m_addr )] << endl;
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
                *stream << "UREF #" << index << ": [" << vector->
                    m_local[index].m_new << "] ";
            } else {
                *stream << "UREF #" << index << ": ";
            }
        } else {
            continue;
        }
        if( vector->m_local[index].m_vtype ) {
            vf_ValidType *type = vector->m_local[index].m_vtype;
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
    for( index = 0; index < vector->m_depth; index++ ) {
        vf_dump_vector_entry( &vector->m_stack[index], stream );
        if( vector->m_stack[index].m_is_local ) {
            *stream << "!";
        } else {
            *stream << " ";
        }
    }
    *stream << endl;
    // dump stack references
    for( index = 0; index < vector->m_depth; index++ ) {
        if( vector->m_stack[index].m_type == SM_REF ) {
            *stream << " REF #" << index << ": ";
        } else if( vector->m_stack[index].m_type == SM_UNINITIALIZED ) {
            if( vector->m_stack[index].m_new ) {
                *stream << "UREF #" << index << ": [" << vector->
                    m_stack[index].m_new << "] ";
            } else {
                *stream << "UREF #" << index << ": ";
            }
        } else {
            continue;
        }
        if( vector->m_stack[index].m_vtype ) {
            vf_ValidType *type = vector->m_stack[index].m_vtype;
            *stream << type;
            for( count = 0; count < type->number; count++ ) {
                *stream << " " << type->string[count];
            }
        } else {
            *stream << "NULL";
        }
        *stream << endl;
    }
}                               // vf_dump_vector

#endif // _VF_DEBUG

/**
 * Function compares two valid types. 
 */
bool
vf_is_types_equal( vf_ValidType *type1, // first checked type
                   vf_ValidType *type2 )        // second checked type
{
    if( type1 == type2 ) {
        return true;
    } else if( type1 == NULL || type2 == NULL
               || type1->number != type2->number ) {
        return false;
    }
    for( unsigned index = 0; index < type1->number; index++ ) {
        if( type1->string[index] != type2->string[index] ) {
            // types aren't equal
            return false;
        }
    }
    return true;
}                               // vf_is_types_equal

/**
 * Merges two vectors and saves result vector to first vector.
 * Sets <code>is_changed</code> to <code>true</code> if the first vector was
 * changed.
 */
static inline vf_Result
vf_merge_vectors( vf_MapVector *first,  // first vector
                  vf_MapVectorHandle second,    // second vector
                  bool handler_flag,    // if merged node is handler
                  bool &is_changed,     // true if the first vector was changed
                  vf_Context *ctx )     // verification context
{
    is_changed = false;
    vf_MapEntry zero = { 0 };

    // merge local variable vector
    unsigned index;
    for( index = 0; index < first->m_number; index++ ) {
        // merge entries type
        if( first->m_local[index].m_type == SM_TOP ) {
            // no need to merge
            continue;
        } else if( first->m_local[index].m_type !=
                   second->m_local[index].m_type ) {
            // types are differ, reset result entry
            first->m_local[index].m_type = SM_TOP;
            first->m_local[index].m_vtype = NULL;
            first->m_local[index].m_is_local = 1;
            first->m_local[index].m_local = ( unsigned short )index;
            is_changed = true;
        } else if( first->m_local[index].m_type == SM_REF ) {
            // reference types, merge them
            vf_ValidType *type =
                ctx->m_type->MergeTypes( first->m_local[index].m_vtype,
                                         second->m_local[index].m_vtype );
            if( type ) {
                // set merged type
                first->m_local[index].m_vtype = type;
                is_changed = true;
            }
        } else if( first->m_local[index].m_type == SM_UNINITIALIZED ) {
            // reference types, merge them
            vf_ValidType *type =
                ctx->m_type->MergeTypes( first->m_local[index].m_vtype,
                                         second->m_local[index].m_vtype );
            if( type
                || first->m_local[index].m_new !=
                second->m_local[index].m_new ) {
                // types are differ, reset result entry
                first->m_local[index].m_type = SM_TOP;
                first->m_local[index].m_new = 0;
                first->m_local[index].m_vtype = NULL;
                first->m_local[index].m_is_local = 1;
                first->m_local[index].m_local = ( unsigned short )index;
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
            first->m_local[index].m_local = ( unsigned short )index;
        }
        first->m_number = second->m_number;
    }
    // check handler flag
    if( handler_flag ) {
        // no need merge stack for handler node
        return VER_OK;
    }
    // merge stack map vector
    assert( first->m_depth == second->m_depth );
    for( index = 0; index < second->m_depth; index++ ) {
        // merge entries type
        if( first->m_stack[index].m_type == SM_TOP ) {
            // no need to merge
            continue;
        } else if( first->m_stack[index].m_type !=
                   second->m_stack[index].m_type ) {
            // types differ, verification should fail
            VF_REPORT( ctx, "Type mismatch while merging a stack map" );
            return VER_ErrorDataFlow;
        } else if( first->m_stack[index].m_type == SM_REF ) {
            // reference types, merge them
            vf_ValidType *type =
                ctx->m_type->MergeTypes( first->m_stack[index].m_vtype,
                                         second->m_stack[index].m_vtype );
            if( type ) {
                // set merged type
                first->m_stack[index].m_vtype = type;
                is_changed = true;
            }
        } else if( first->m_stack[index].m_type == SM_UNINITIALIZED ) {
            // reference types, merge them
            vf_ValidType *type =
                ctx->m_type->MergeTypes( first->m_stack[index].m_vtype,
                                         second->m_stack[index].m_vtype );
            if( type
                || first->m_stack[index].m_new !=
                second->m_stack[index].m_new ) {
                // types are differ, reset result entry
                first->m_stack[index] = zero;
                is_changed = true;
            }
        }
    }
    return VER_OK;
}                               // vf_merge_vectors

/**
 * Function compares two vectors.
 */
static inline bool
vf_compare_vectors( vf_MapVectorHandle first,   // first vector
                    vf_MapVectorHandle second ) // second vector
{
    // compare vector parameters
    if( first->m_number != second->m_number
        || first->m_depth != second->m_depth ) {
        return false;
    }
    // compare locals
    unsigned index;
    for( index = 0; index < first->m_number; index++ ) {
        assert( first->m_local[index].m_is_local == 1 );
        assert( second->m_local[index].m_is_local == 1 );
        if( first->m_local[index].m_local != second->m_local[index].m_local
            || first->m_local[index].m_type != second->m_local[index].m_type
            || first->m_local[index].m_vtype !=
            second->m_local[index].m_vtype ) {
            return false;
        }
    }
    // compare stack
    for( index = 0; index < first->m_depth; index++ ) {
        if( first->m_stack[index].m_type != second->m_stack[index].m_type
            || first->m_stack[index].m_vtype !=
            second->m_stack[index].m_vtype ) {
            return false;
        }
    }
    return true;
}                               // vf_compare_vectors

/**
 * Function check access constraint for two stack map references.
 */
static inline vf_Result
vf_check_access( vf_MapEntry *source,   // stack map entry
                 vf_MapEntry *target,   // required map entry
                 vf_Context *ctx )      // verification context
{
    // compare types
    assert( target->m_vtype->number == 1 );
    vf_CheckConstraint check = ( target->m_ctype == VF_CHECK_ACCESS_FIELD )
        ? VF_CHECK_ASSIGN : VF_CHECK_PARAM;
    for( unsigned index = 0; index < source->m_vtype->number; index++ ) {
        // set constraints for differing types
        if( target->m_vtype->string[0] != source->m_vtype->string[index] ) {
            ctx->m_type->SetRestriction( target->m_vtype->string[0],
                                         source->m_vtype->string[index], 0,
                                         check );
        }
        // set access constraints for differing types
        if( ctx->m_vtype.m_class->string[0] !=
            source->m_vtype->string[index] ) {
            // not the same class
            vf_Result result =
                vf_check_access_constraint( target->m_vtype->string[0],
                                            source->m_vtype->string[index],
                                            target->m_index,
                                            ( vf_CheckConstraint )
                                            target->m_ctype,
                                            ctx );
            if( result == VER_ClassNotLoaded ) {
                // cannot complete check, set restriction
                ctx->m_type->SetRestriction( ctx->m_vtype.m_class->
                                             string[0],
                                             source->m_vtype->string[index],
                                             0, ( vf_CheckConstraint )
                                             target->m_ctype );
            } else if( VER_OK != result ) {
                // return error
                return result;
            }
        }
    }
    return VER_OK;
}                               // vf_check_access

/**
 * Checks two stack map references.
 */
static inline vf_Result
vf_check_entry_refs( vf_MapEntry *source,       // stack map entry
                     vf_MapEntry *target,       // required map entry
                     bool local_init,   // initialization flag of locals
                     vf_Context *ctx )  // verification context
{
    // check local variable type
    if( source->m_is_local
        && ( !target->m_is_local || source->m_local != target->m_local ) ) {
        return VER_ErrorDataFlow;
    }
    // check entries type
    if( !( SM_REF == source->m_type || SM_UNINITIALIZED == source->m_type )
        || !( SM_REF == target->m_type
              || SM_UNINITIALIZED == target->m_type ) ) {
        if( SM_ANY == source->m_type ) {
            return VER_OK;
        }
        // only aload and astore get SM_REF/VF_CHECK_UNINITIALIZED_THIS
        if( SM_RETURN_ADDR == source->m_type
            && VF_CHECK_UNINITIALIZED_THIS == target->m_ctype ) {
            if( source->m_is_local ) {
                // aload a return address
                return VER_ErrorJsrLoadRetAddr;
            } else if( !target->m_is_local ) {
                // astore a return address
                return VER_OK;
            }
        }
        return VER_ErrorDataFlow;
    }
    // check available entry
    if( source->m_vtype == NULL ) {
        // nothing to check
        return VER_OK;
    }
    // check initialization
    if( source->m_type == SM_UNINITIALIZED
        && target->m_type != SM_UNINITIALIZED ) {
        if( ( source->m_new == 0 && target->m_ctype == VF_CHECK_ACCESS_FIELD )
            || ( local_init == false
                 && target->m_ctype == VF_CHECK_UNINITIALIZED_THIS )
            || ( !ctx->m_verify_all && source->m_new == 0
                 && target->m_ctype == VF_CHECK_UNINITIALIZED_THIS ) ) {
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
            VF_REPORT( ctx, "Uninitialized reference usage" );
            return VER_ErrorDataFlow;
        }
    }
    // check references
    bool is_error = false;
    switch ( target->m_ctype ) {
    case VF_CHECK_NONE:
    case VF_CHECK_UNINITIALIZED_THIS:
    case VF_CHECK_PARAM:       // check method invocation conversion
        if( target->m_vtype != NULL ) {
            is_error = ctx->m_type->CheckTypes( target->m_vtype,
                                                source->m_vtype, 0,
                                                VF_CHECK_PARAM );
        }
        break;
    case VF_CHECK_ARRAY:       // check if source reference is array 
        is_error =
            ctx->m_type->CheckTypes( NULL, source->m_vtype, 0,
                                     VF_CHECK_ARRAY );
        break;
    case VF_CHECK_REF_ARRAY:
        is_error =
            ctx->m_type->CheckTypes( NULL, source->m_vtype, 0,
                                     VF_CHECK_REF_ARRAY );
        break;
    case VF_CHECK_EQUAL:       // check if references are equal
        is_error = ctx->m_type->CheckTypes( target->m_vtype,
                                            source->m_vtype, 0,
                                            VF_CHECK_EQUAL );
        break;
    case VF_CHECK_ASSIGN:      // check assignment conversion
    case VF_CHECK_ASSIGN_WEAK: // check weak assignment conversion
        assert( target->m_vtype != NULL );
        is_error = ctx->m_type->CheckTypes( target->m_vtype,
                                            source->m_vtype, 0,
                                            ( vf_CheckConstraint ) target->
                                            m_ctype );
        break;
    case VF_CHECK_ACCESS_FIELD:        // check field access
    case VF_CHECK_ACCESS_METHOD:       // check method access
        {
            assert( target->m_vtype != NULL );  // not null reference
            assert( target->m_local != 0 );     // constant pool index is set
            vf_Result result = vf_check_access( source, target, ctx );
            return result;
        }
        break;
    case VF_CHECK_DIRECT_SUPER:        // check is target class is direct super class of source
        is_error =
            ctx->m_type->CheckTypes( target->m_vtype, source->m_vtype, 0,
                                     VF_CHECK_DIRECT_SUPER );
        break;
    case VF_CHECK_INVOKESPECIAL:       // check invokespecial object reference
        is_error =
            ctx->m_type->CheckTypes( target->m_vtype, source->m_vtype, 0,
                                     VF_CHECK_INVOKESPECIAL );
        break;
    default:
        VF_DIE( "vf_check_entry_refs: unknown check in switch" );
    }
    // check error
    if( is_error ) {
        return VER_ErrorDataFlow;
    }

    return VER_OK;
}                               // vf_check_entry_refs

/**
 * Checks two stack map entries.
 */
static inline vf_Result
vf_check_entry_types( vf_MapEntry *entry1,      // stack map entry
                      vf_MapEntry *entry2,      // required map entry
                      bool local_init,  // initialization flag of locals
                      bool *need_copy,  // pointer to copy flag
                      vf_Context *ctx ) // verification context
{
    if( SM_ANY == entry1->m_type ) {
        return VER_OK;
    }
    switch ( entry2->m_type ) {
    case SM_REF:
        // check reference entries
        {
            vf_Result result =
                vf_check_entry_refs( entry1, entry2, local_init, ctx );
            *need_copy = true;
            return result;
        }
    case SM_RETURN_ADDR:
        if( entry1->m_type != entry2->m_type ) {
            return VER_ErrorDataFlow;
        }
        *need_copy = true;
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
        // pop gets any single word
        switch ( entry1->m_type ) {
        case SM_INT:
        case SM_FLOAT:
        case SM_NULL:
            break;
        case SM_REF:
        case SM_UNINITIALIZED:
        case SM_RETURN_ADDR:
            *need_copy = true;
            break;
        default:
            return VER_ErrorDataFlow;
        }
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
            entry1->m_type != SM_LONG_HI && entry1->m_type != SM_DOUBLE_HI ) {
            return VER_ErrorDataFlow;
        }
        *need_copy = true;
        break;
    default:
        return VER_ErrorDataFlow;
    }

    // check local variable type
    if( entry1->m_is_local
        && ( !entry2->m_is_local || entry1->m_local != entry2->m_local ) ) {
        return VER_ErrorDataFlow;
    }
    return VER_OK;
}                               // vf_check_entry_types

/**
 * Function creates array element valid type.
 */
static inline void
vf_set_array_element_type( vf_MapEntry *buf,    // result data flow vector entry
                           vf_MapEntry *vector, // data flow vector entry
                           vf_MapEntry *array,  // array data flow vector entry
                           vf_ContextHandle ctx )       // verification context
{
    assert( SM_REF == array->m_type || SM_ANY == array->m_type );
    buf->m_type = array->m_type;
    buf->m_vtype = array->m_vtype;
    buf->m_local = vector->m_local;
    buf->m_is_local = vector->m_is_local;
    if( buf->m_vtype ) {
        // create cell array type
        buf->m_vtype = ctx->m_type->NewArrayElemType( buf->m_vtype );
    }
}                               // vf_set_array_element_type

/**
 * Receives IN data flow vector entry for code instruction.
 */
static inline vf_MapEntry *
vf_set_new_in_vector( vf_InstrHandle instr,     // code instruction
                      vf_MapEntry *vector,      // data flow vector entry
                      vf_ContextHandle ctx )    // verification context
{
    short number;
    vf_MapEntry *stack_buf = ctx->m_buf;
    vf_MapEntry *newvector;

    switch ( vector->m_type ) {
    case SM_COPY_0:
    case SM_COPY_1:
    case SM_COPY_2:
    case SM_COPY_3:
        // it's a copy type, copy value from IN vector
        number = ( short )( vector->m_type - SM_COPY_0 );
        assert( number < instr->m_inlen );
        newvector = &instr->m_invector[number];
        if( newvector->m_type == SM_TOP
            || newvector->m_type >= SM_WORD
            || ( newvector->m_type == SM_REF
                 && newvector->m_vtype == NULL ) ) {
            // copy value from saved stack entry
            newvector = &stack_buf[number];
            newvector->m_local = vector->m_local;
            newvector->m_is_local = vector->m_is_local;
        }
        break;
    case SM_UP_ARRAY:
        // set reference array element
        newvector = &stack_buf[0];
        vf_set_array_element_type( newvector, vector, &stack_buf[0], ctx );
        break;
    default:
        newvector = vector;
        break;
    }
    return newvector;
}                               // vf_set_new_in_vector

/**
 * Sets OUT data flow vector for code instruction.
 */
static inline void
vf_set_instruction_out_vector( vf_InstrHandle instr,    // code instruction
                               vf_MapEntry *stack,      // stack map vector
                               vf_MapEntry *locals,     // local variable vector
                               unsigned short *number,  // pointer to local variables number
                               vf_ContextHandle ctx )   // verification context
{
    unsigned index;
    vf_MapEntry *entry, *vector;

    // set instruction out vector
    for( index = 0, vector = instr->m_outvector;
         index < instr->m_outlen;
         index++, vector = &instr->m_outvector[index] ) {
        vf_MapEntry *newvector = vf_set_new_in_vector( instr, vector, ctx );
        if( newvector->m_is_local ) {
            // get local variable
            entry = locals + newvector->m_local;
            // set local variable type
            entry->m_local = newvector->m_local;
            entry->m_is_local = 1;
            // set locals vector number
            if( newvector->m_local + 1 > ( *number ) ) {
                *number = ( unsigned short )( newvector->m_local + 1 );
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
}                               // vf_set_instruction_out_vector

/**
 * Function clears stack map vector.
 */
static inline void
vf_clear_stack( vf_MapVector *vector )  // map vector
{
    vf_MapEntry zero_entry = { 0 };

    // zero stack vector
    for( unsigned index = 0; index < vector->m_depth; index++ ) {
        vector->m_stack[index] = zero_entry;
    }
    vector->m_depth = 0;

}                               // vf_clear_stack

/**
 * Checks that <code>this</code> instance was initialized.
 */
static inline vf_Result
vf_check_initialized_this( vf_MapEntry *locals, // stack map vector
                           vf_Context *ctx )    // verification context
{
    // check <init> method
    if( ctx->m_is_constructor
        && ctx->m_vtype.m_class != ctx->m_vtype.m_object ) {
        if( SM_REF == locals->m_type
            && locals->m_vtype == ctx->m_vtype.m_class ) {
            // constructor returns initialized reference of a given class
        } else {
            VF_REPORT( ctx, "Constructor must be invoked" );
            return VER_ErrorDataFlow;
        }
    }
    return VER_OK;
}                               // vf_check_initialized_this

/**
 * Checks that return instruction matches function return type. The check
 * is delayed to match Sun's implementation.
 */
static inline vf_Result
vf_check_return_instruction( vf_InstrHandle instr,      // a return instruction
                             vf_Context *ctx )  // verification context
{
    if( instr->m_inlen != ctx->m_method_outlen ) {
        VF_REPORT( ctx,
                   "Return instruction stack modifier doesn't match method return type" );
        return VER_ErrorInstruction;
    }

    for( unsigned short index = 0; index < instr->m_inlen; index++ ) {
        assert( instr->m_invector[index].m_is_local == 0 );

        // check entry types
        if( instr->m_invector[index].m_type !=
            ctx->m_method_outvector[index].m_type ) {
            VF_REPORT( ctx,
                       "Return instruction stack doesn't match function return type" );
            return VER_ErrorInstruction;
        }
    }
    return VER_OK;
}                               // vf_check_return_instruction

/**
 * Checks data flow for code instruction.
 */
static inline vf_Result
vf_check_instruction_in_vector( vf_MapEntry *stack,     // stack map vector
                                vf_MapEntry *locals,    // local variable vector
                                vf_MapEntry *buf,       // buf storage vector
                                vf_MapEntry *invector,  // code instruction IN vector
                                unsigned len,   // IN vector length
                                bool local_init,        // initialization flag of locals
                                bool *need_init,        // init uninitialized entry
                                vf_Context *ctx )       // verification context
{
    unsigned index;
    vf_MapEntry *entry, *vector, *newvector;
    vf_Result result;

    // check IN vector entries
    for( index = 0, vector = invector;
         index < len; index++, vector = &invector[index] ) {
        // set copy flag
        bool copy = false;
        // get check entry
        entry = vector->m_is_local ? locals + vector->m_local : stack + index;
        switch ( vector->m_type ) {
        case SM_TOP:
            // copy entry
            copy = true;
            break;
        case SM_UP_ARRAY:
            // check reference array element
            assert( index > 0 );
            newvector = &buf[index];
            // check assignment conversion
            vf_set_array_element_type( newvector, vector, &buf[0], ctx );
            if( newvector->m_vtype ) {
                newvector->m_ctype = VF_CHECK_ASSIGN_WEAK;
            } else if( SM_ANY == newvector->m_type ) {
                break;          // anything can be assigned to such array
            } else {
                newvector->m_ctype = VF_CHECK_NONE;
            }
            // check entry types
            result = vf_check_entry_refs( entry, newvector, local_init, ctx );
            if( VER_OK != result ) {
                VF_REPORT( ctx, "Incompatible types for array assignment" );
                return result;
            }
            break;
        case SM_REF:
            // check entry references
            result = vf_check_entry_refs( entry, vector, local_init, ctx );
            if( VER_OK != result ) {
                if( VER_ErrorJsrLoadRetAddr == result ) {
                    VF_REPORT( ctx,
                               "Cannot load a return address from a local variable "
                               << entry->m_local );
                }
                if( !ctx->m_error ) {
                    VF_REPORT( ctx, "Data flow analysis error" );
                }
                return result;
            }
            copy = true;
            break;
        case SM_UNINITIALIZED:
            // check entry references
            if( entry->m_type == SM_REF ) {
                // double initialization
                VF_REPORT( ctx, "Double initialization of object reference" );
                return VER_ErrorDataFlow;
            }
            result = vf_check_entry_refs( entry, vector, local_init, ctx );
            if( VER_OK != result ) {
                VF_REPORT( ctx, "Data flow analysis error (uninitialized)" );
                return result;
            }
            // check initialization class in constructor
            if( entry->m_type == SM_UNINITIALIZED && entry->m_new == 0 ) {
                // initialization of this reference in class construction
                assert( entry->m_vtype->number == 1 );
                if( vector->m_vtype->string[0] != entry->m_vtype->string[0] ) {
                    ctx->m_type->SetRestriction( vector->m_vtype->
                                                 string[0],
                                                 entry->m_vtype->string[0], 0,
                                                 VF_CHECK_DIRECT_SUPER );
                }
            }
            *need_init = true;
            copy = true;
            break;
        default:
            // check entry types
            result =
                vf_check_entry_types( entry, vector, local_init, &copy, ctx );
            if( VER_OK != result ) {
                VF_REPORT( ctx, "Data flow analysis error" );
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
}                               // vf_check_instruction_in_vector

/**
 * Receives code instruction OUT data flow vector.
 */
static vf_Result
vf_get_instruction_out_vector( vf_NodeHandle node,      // graph node
                               vf_InstrHandle instr,    // code instruction
                               vf_MapVector *invector,  // incoming data flow vector
                               vf_Context *ctx )        // verification context
{
    unsigned index;
    bool need_init = false;
    vf_MapEntry *buf = ctx->m_buf;
    vf_MapEntry zero_entry = { 0 };

    // set stack vector
    assert( invector->m_depth - instr->m_minstack >= 0 );
    vf_MapEntry *stack =
        invector->m_stack + invector->m_depth - instr->m_minstack;
    // set locals vector
    vf_MapEntry *locals = invector->m_local;
    // check instruction in vector
    vf_Result result = vf_check_instruction_in_vector( stack, locals, buf,
                                                       instr->m_invector,
                                                       instr->m_inlen,
                                                       node->m_initialized,
                                                       &need_init, ctx );
    if( VER_OK != result ) {
        return result;
    }
    // don't create out vector for return instructions and athrow
    if( VF_INSTR_RETURN == instr->m_type ) {
        result = vf_check_initialized_this( locals, ctx );
        if( VER_OK != result ) {
            return result;
        }
        return vf_check_return_instruction( instr, ctx );
    } else if( VF_INSTR_THROW == instr->m_type ) {
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
                && invector->m_local[index].m_new == buf[0].m_new ) {
                assert( invector->m_local[index].m_vtype == buf[0].m_vtype );
                invector->m_local[index].m_type = SM_REF;
            }
        }
        // init stack reference
        for( index = 0;
             index < ( unsigned )invector->m_depth - instr->m_minstack;
             index++ ) {
            if( invector->m_stack[index].m_type == SM_UNINITIALIZED
                && invector->m_stack[index].m_new == buf[0].m_new ) {
                assert( invector->m_stack[index].m_vtype == buf[0].m_vtype );
                invector->m_stack[index].m_type = SM_REF;
            }
        }
    }
    // set instruction OUT vector
    vf_GraphHandle graph = ctx->m_graph;
    invector->m_depth =
        ( unsigned short )( invector->m_depth + instr->m_stack );
    assert( invector->m_depth <= ctx->m_maxstack );
    index = invector->m_number;
    vf_set_instruction_out_vector( instr, stack, locals, &invector->m_number,
                                   ctx );
    assert( invector->m_number <= ctx->m_maxlocal );
    // set local variable numbers
    for( ; index < invector->m_number; index++ ) {
        if( !locals[index].m_is_local ) {
            locals[index].m_is_local = 1;
            locals[index].m_local = index;
        }
    }

    return VER_OK;
}                               // vf_get_instruction_out_vector

/**
 * Copies a stored handler vector to the out vector.
 */
static inline vf_Result
vf_get_handler_out_vector( vf_MapVector *invector,      // IN handler vector
                           vf_MapVectorHandle handler_vector )  // stored handler vector
{
//    assert( 0 == invector->m_depth ); FIXME
    assert( 1 == handler_vector->m_depth );
    assert( SM_REF == handler_vector->m_stack->m_type );

    // clear stack for exception handler
    vf_clear_stack( invector );

    // set handler out vector
    invector->m_stack->m_type = handler_vector->m_stack->m_type;
    invector->m_stack->m_vtype = handler_vector->m_stack->m_vtype;

    // set modify vector value
    invector->m_depth = 1;
    return VER_OK;
}                               // vf_get_handler_out_vector

/**
 * Sets graph node OUT data flow vector.
 */
vf_Result
vf_set_node_out_vector( vf_NodeHandle node,     // a graph node
                        vf_MapVector *invector, // incoming data flow vector
                        vf_Context *ctx )       // verification context
{
    vf_Result result;

    // get node instruction number
    vf_Graph *graph = ctx->m_graph;

    /** 
     * For start-entry node doesn't need to check data flow
     */
    if( VF_NODE_START_ENTRY == node->m_type ) {
        return VER_OK;
    }

    if( VF_NODE_HANDLER == node->m_type ) {
        // set OUT vector for a handler node
        return vf_get_handler_out_vector( invector, &node->m_outmap );
    }
    // get first instruction
    vf_InstrHandle instr = node->m_start;
    unsigned instruction = node->m_end - node->m_start + 1;

    // set out vector for each instruction
    for( unsigned index = 0; index < instruction; index++ ) {
        if( ( 0 == instr[index].m_inlen + instr[index].m_outlen )
            && ( VF_INSTR_NONE == instr[index].m_type ) ) {
            continue;
        } else {
            result = vf_get_instruction_out_vector( node, &instr[index],
                                                    invector, ctx );
        }
        if( VER_OK != result ) {
            return result;
        }
        VF_DUMP( DUMP_INSTR_MAP, {
                 // dump instruction OUT vector
                 cerr << "-------------- instruction #" << index << ", node #"
                 << node->m_nodecount << " out: " << endl;
                 vf_dump_vector( invector, &instr[index], &cerr );}
         );
    }
    return VER_OK;
}                               // vf_set_node_out_vector

struct vf_MapMark
{
    bool in_set:1;
    bool out_set:1;
    bool out_updated:1;
};

static inline vf_MapMark *
vf_get_node_mapmark( vf_NodeHandle node )
{
    assert( sizeof( vf_MapMark ) <= sizeof( node->m_mark ) );
    return (vf_MapMark*) & node->m_mark;
}

/**
 * Creates and sets graph node OUT vector.
 */
static vf_Result
vf_create_node_vectors( vf_NodeHandle node,     // a graph node
                        vf_MapVector *incoming, // a vector for instruction data flow change
                        vf_Context *ctx )       // a verifier context
{
    vf_Graph *graph = ctx->m_graph;
    assert( vf_get_node_mapmark( node )->in_set );
    graph->CopyFullVector( &node->m_inmap, incoming );

    VF_DUMP( DUMP_NODE_MAP, {
             // dump node number
             cerr << endl << "-------------- Node #" << node->
             m_nodecount << endl;
             // dump in vector
             cerr << "IN vector :" << endl;
             vf_dump_vector( incoming, NULL, &cerr );} );

    // calculate OUT node vector
    vf_Result result = vf_set_node_out_vector( node, incoming, ctx );
    if( VER_OK != result ) {
        return result;
    }
    // set node OUT vector
    vf_MapVector *outcoming = (vf_MapVector*)&node->m_outmap;
    bool is_out_changed = false;
    if( !vf_get_node_mapmark( node )->out_set ) {
        // create node OUT vector
        vf_get_node_mapmark( node )->out_set = true;
        graph->AllocVector( outcoming );
        graph->CopyVector( incoming, outcoming );
        is_out_changed = true;
    } else if( !vf_compare_vectors( outcoming, incoming ) ) {
        // vectors differ
        graph->CopyFullVector( incoming, outcoming );
        is_out_changed = true;
    }
    VF_DUMP( DUMP_NODE_MAP, {
             // dump out vector
             cerr << "-------------- Node #" << node->m_nodecount
             << endl << "OUT vector:" << endl;
             vf_dump_vector( outcoming, NULL, &cerr );}
     );

    // check stack modifier
    assert( ( int )( node->m_outmap.m_depth
                     - node->m_inmap.m_depth ) == node->m_stack );

    return ( is_out_changed ) ? VER_Continue : VER_OK;
}                               // vf_create_node_vectors

/**
 * Checks data flow for a graph node.
 */
static vf_Result
vf_check_node_data_flow( vf_NodeHandle node,    // a graph node
                         unsigned *node_count,  // last graph node in recursion
                         bool *need_recheck,    // set to true if need to recheck previous nodes
                         vf_Context *ctx )      // verification context
{
    // get graph
    vf_Graph *graph = ctx->m_graph;
    vf_MapVector *incoming = ctx->m_map;        // incoming data flow vector

    // skip end-entry node
    if( VF_NODE_END_ENTRY == node->m_type ) {
        return VER_OK;
    }

    if( vf_get_node_mapmark( node )->out_updated ) {
        return VER_OK;
    }

    vf_Result result = vf_create_node_vectors( node, incoming, ctx );
    vf_get_node_mapmark( node )->out_updated = true;
    if( VER_Continue != result ) {
        return result;
    }
    // set incoming vector for following nodes
    vf_MapVectorHandle innode_vector = &node->m_outmap;
    for( vf_EdgeHandle outedge = node->m_outedge;
         outedge; outedge = outedge->m_outnext ) {

        // get an edge end node
        vf_NodeHandle outnode = outedge->m_end;

        // skip an end entry
        if( VF_NODE_END_ENTRY == outnode->m_type ) {
            continue;
        }
        // get out node IN vector
        vf_MapVector *outnode_vector = (vf_MapVector*)&outnode->m_inmap;

        if( !vf_get_node_mapmark( outnode )->in_set ) {
            vf_get_node_mapmark( outnode )->in_set = true;
            graph->AllocVector( outnode_vector );
            // node's IN vector is invalid, set it
            if( VF_NODE_HANDLER == outnode->m_type ) {
                // it's exception node, create IN vector for it
                graph->CopyFullVector( innode_vector, incoming );
                vf_clear_stack( incoming );
                graph->CopyVector( incoming, outnode_vector );
            } else {
                // other nodes
                graph->CopyVector( innode_vector, outnode_vector );
            }
        } else {
            // copy out node IN vector for dump
            VF_DUMP( DUMP_MERGE_MAP,
                     graph->CopyFullVector( outnode_vector, incoming ) );

            // node's IN vector is valid, merge them
            bool is_handler = VF_NODE_HANDLER == outnode->m_type;
            // FIXME for handler branches need to merge local variable maps for any
            // FIXME instruction since a jump can happen anywhere

            bool is_changed;
            result = vf_merge_vectors( outnode_vector, innode_vector,
                                       is_handler, is_changed, ctx );
            if( VER_OK != result ) {
                return result;
            }

            if( is_changed ) {
                // node IN vector is changed, reset node OUT vector results
                vf_get_node_mapmark( outnode )->out_updated = false;
                // set node for re-verification if it's needed.
                unsigned count = outnode->m_nodecount;
                if( count <= ( *node_count ) ) {
                    *node_count = count;
                    *need_recheck = true;
                }
            }

            VF_DUMP( DUMP_MERGE_MAP, if( is_changed ) {
                     // dump out vectors
                     cerr << "============== merge IN vector for Node #"
                     << outnode->m_nodecount << endl;
                     cerr << "IN vectors:" << endl;
                     cerr << "1: --------------" << endl;
                     vf_dump_vector( incoming, NULL, &cerr );
                     cerr << "2: --------------" << endl;
                     vf_dump_vector( innode_vector, NULL, &cerr );
                     // dump out vector
                     cerr << "result: --------------" << endl;
                     vf_dump_vector( outnode_vector, NULL, &cerr );
                     cerr << "### Recount from " << *node_count
                     << " (now " << node->m_nodecount << ")" << endl;}
             );
        }
    }

    return VER_OK;
}                               // vf_check_node_data_flow

/**
 * Creates a map for a start node.
 */
static void
vf_create_start_map( vf_Context *ctx )      // verification context
{
    vf_Graph *graph = ctx->m_graph;

    // alloc memory for vector structure
    vf_Node *start_node = (vf_Node*)graph->GetStartNode();
    vf_MapVector *vector = (vf_MapVector*)&start_node->m_inmap;
    graph->AllocVector( vector );
    vf_get_node_mapmark( start_node )->in_set = true;

    // set "this" reference in a local variable
    unsigned short start;
    if( method_is_static( ctx->m_method ) ) {
        start = 0;
    } else {
        start = 1;
        // fill "this" entry
        const char *name = class_get_name( ctx->m_class );
        vf_ValidType *type = vf_create_class_valid_type( name, ctx );
        if( ctx->m_is_constructor ) {
            vector->m_local->m_type = SM_UNINITIALIZED;
        } else {
            vector->m_local->m_type = SM_REF;
        }
        vector->m_local->m_vtype = type;
        vector->m_local->m_is_local = 1;
        vector->m_local->m_local = 0;
    }

    // set start vector
    unsigned short inlen = ctx->m_method_inlen;
    vf_MapEntry *invector = ctx->m_method_invector;

    unsigned short index, count;
    for( index = start, count = 0; count < inlen; index++, count++ ) {
        vector->m_local[index].m_type = invector[count].m_type;
        vector->m_local[index].m_vtype = invector[count].m_vtype;
        vector->m_local[index].m_is_local = 1;
        vector->m_local[index].m_local = ( unsigned short )index;
    }
    vector->m_number = index;
}                               // vf_create_start_map

/**
 * Function enumerates graph nodes by wave numeration.
 */
void
vf_enumerate_graph_node( vf_ContextHandle ctx )
{
    // clear graph node marks
    vf_Graph *graph = ctx->m_graph;
    graph->CleanNodeMarks();

    // set first enumeration node
    vf_Node *start_node = (vf_Node*)graph->GetStartNode();
    graph->SetStartCountNode( start_node );
    start_node->m_mark = VF_START_MARK;

    // enumerate graph nodes
    for( unsigned index = 0; index < graph->GetReachableNodeCount();
         index++ ) {
        // get node by count element
        vf_NodeHandle node = graph->GetReachableNode( index );
        assert( node );

        // get node mark
        int mark = node->m_mark;

        // override all out edges of node
        for( vf_EdgeHandle outedge = node->m_outedge;
             outedge; outedge = outedge->m_outnext ) {
            // get out node and its mark
            vf_Node *outnode = (vf_Node*)outedge->m_end;
            int out_node_mark = outnode->m_mark;
            if( !out_node_mark ) {
                // it's unnumerated node, enumerate it
                graph->AddReachableNode( outnode );
                outnode->m_mark = mark + 1;
            }
        }
    }
}                               // vf_enumerate_graph_node

/**
 * Provides data flow checks of verifier graph structure.
 */
vf_Result
vf_check_graph_data_flow( vf_Context *ctx )     // verification context
{
    // enumerate graph
    vf_enumerate_graph_node( ctx );

    // clean node marks, set a mark bit when IN or OUT node stack maps
    // get initialized 
    vf_Graph *graph = ctx->m_graph;
    graph->CleanNodeMarks();

    // set a map for a start node
    vf_create_start_map( ctx );

    // check graph data flow
    bool need_recheck = false;
    unsigned count = 0;
    do {
        vf_NodeHandle node = graph->GetReachableNode( count );
        vf_Result result =
            vf_check_node_data_flow( node, &count, &need_recheck, ctx );
        if( VER_OK != result ) {
            return result;
        }
        if( !need_recheck ) {
            // check next node
            count++;
        } else {
            need_recheck = false;
        }
    }
    while( count < graph->GetReachableNodeCount() );

    return VER_OK;
}                               // vf_check_graph_data_flow
