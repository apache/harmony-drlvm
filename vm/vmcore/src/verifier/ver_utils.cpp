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
 * @version $Revision: 1.1.2.3.4.4 $
 */  

#include "ver_real.h"
#include "port_malloc.h"

/**
 * Debug flag macros
 */
// Macro enable verifier memory trace
#define VERIFY_TRACE_MEMORY 0

/**
 * Set namespace Verifier
 */
namespace Verifier {

/************************************************************
 *********************** Hash class *************************
 ************************************************************/

/**
 * Hash table constructor.
 */
vf_Hash::vf_Hash() : HASH_SIZE(127), m_free(true)
{
    m_pool = vf_create_pool();
    m_hash = (vf_HashEntry_t**)vf_alloc_pool_memory( m_pool, 
        HASH_SIZE * sizeof(vf_HashEntry_t*) );
} // vf_Hash::vf_Hash

/**
 * Hash table constructor.
 */
vf_Hash::vf_Hash( vf_VerifyPool_t *pool ) : HASH_SIZE(127), m_free(false)
{
    m_pool = pool;
    m_hash = (vf_HashEntry_t**)vf_alloc_pool_memory( m_pool, 
        HASH_SIZE * sizeof(vf_HashEntry_t*) );
} // vf_Hash::vf_Hash

/**
 * Hash table destructor.
 */
vf_Hash::~vf_Hash()
{
    if( m_free ) {
        vf_delete_pool( m_pool );
    }
} // vf_Hash::vf_Hash

/**
 * Function checks key identity.
 */
inline bool
vf_Hash::CheckKey( vf_HashEntry_t *hash_entry,  // checked hash entry
                   const char *key)             // checked key
{
    if( !strcmp( hash_entry->key, key ) ) {
        return true;
    }
    return false;
} // vf_Hash::CheckKey( hash, key )

/**
 * Function checks key identity.
 */
inline bool
vf_Hash::CheckKey( vf_HashEntry_t *hash_entry,  // checked hash entry
                   const char *key,             // checked key
                   unsigned len)                // key length
{
    if( !strncmp( hash_entry->key, key, len ) && hash_entry->key[len] == '\0'  ) {
        return true;
    }
    return false;
} // vf_Hash::CheckKey( hash, key, len )

/**
 * Hash function.
 */
inline unsigned
vf_Hash::HashFunc( const char *key )    // key for hash function
{
    unsigned result = 0;
    unsigned char ch;

    while ((ch = (unsigned char)(*key++))) {
        result = (result * 16777619) ^ ch ;
    }
    return result;
} // vf_Hash::HashFunc( key )

/**
 * Hash function.
 */
inline unsigned
vf_Hash::HashFunc( const char *key,     // key for hash function
                   unsigned len)        // key length
{
    unsigned result = 0;

    for (unsigned index = 0; index < len; index++) {
        result = (result * 16777619) ^ (unsigned char)(*key++) ;
    }
    return result;
} // vf_Hash::HashFunc( key, len )

/**
 * Function looks up hash entry which is identical to current key.
 */
inline vf_HashEntry_t *
vf_Hash::Lookup( const char *key )      // hash key
{
    assert( key );
    unsigned hash_index = HashFunc( key );
    hash_index = hash_index % HASH_SIZE;
    vf_HashEntry_t *hash_entry = m_hash[hash_index];
    while( hash_entry != NULL ) {
        if( CheckKey( hash_entry, key ) ) {
            return hash_entry;
        }
        hash_entry = hash_entry->next;
    }
    return NULL;
} // vf_Hash::Lookup( key )

/**
 * Function looks up hash entry which is identical to current key.
 */
inline vf_HashEntry_t *
vf_Hash::Lookup( const char *key,       // hash key
                 unsigned len)          // key length
{
    assert( key );
    assert( len );
    unsigned hash_index = HashFunc( key, len );
    hash_index = hash_index % HASH_SIZE;
    vf_HashEntry_t *hash_entry = m_hash[hash_index];
    while( hash_entry != NULL ) {
        if( CheckKey( hash_entry, key, len ) ) {
            return hash_entry;
        }
        hash_entry = hash_entry->next;
    }
    return NULL;
} // vf_Hash::Lookup( key, len )

/**
 * Function creates hash entry which is identical to current key.
 */
inline vf_HashEntry_t *
vf_Hash::NewHashEntry( const char *key )    // hash key
{
    // lookup type in hash
    assert( key );
    unsigned hash_index = HashFunc( key );
    hash_index = hash_index % HASH_SIZE;
    vf_HashEntry_t *hash_entry = m_hash[hash_index];
    while( hash_entry != NULL ) {
        if( CheckKey( hash_entry, key ) ) {
            return hash_entry;
        }
        hash_entry = hash_entry->next;
    }

    // create key string
    unsigned len = strlen(key);
    char *hash_key = (char*)vf_alloc_pool_memory( m_pool, len + 1 );
    memcpy( hash_key, key, len );

    // create hash entry
    hash_entry = (vf_HashEntry_t*)vf_alloc_pool_memory( m_pool, sizeof(vf_HashEntry_t) );
    hash_entry->key = hash_key;
    hash_entry->next = m_hash[hash_index];
    m_hash[hash_index] = hash_entry;

    return hash_entry;
} // vf_Hash::NewHashEntry( key )

/**
 * Function creates hash entry which is identical to current key.
 */
inline vf_HashEntry_t *
vf_Hash::NewHashEntry( const char *key,     // hash key
                       unsigned len)        // key length
{
    // lookup type in hash
    assert( key );
    assert( len );
    unsigned hash_index = HashFunc( key, len );
    hash_index = hash_index % HASH_SIZE;
    vf_HashEntry_t *hash_entry = m_hash[hash_index];
    while( hash_entry != NULL ) {
        if( CheckKey( hash_entry, key, len ) ) {
            return hash_entry;
        }
        hash_entry = hash_entry->next;
    }

    // create key string
    char *hash_key = (char*)vf_alloc_pool_memory( m_pool, len + 1 );
    memcpy( hash_key, key, len );

    // create hash entry
    hash_entry = (vf_HashEntry_t*)vf_alloc_pool_memory( m_pool, sizeof(vf_HashEntry_t) );
    hash_entry->key = hash_key;
    hash_entry->next = m_hash[hash_index];
    m_hash[hash_index] = hash_entry;

    return hash_entry;
} // vf_Hash::NewHashEntry( key, len )

/************************************************************
 ******************** Valid Type class **********************
 ************************************************************/

/**
 * Type constraint collection constructor.
 */
vf_TypePool::vf_TypePool() : m_method(NULL), m_restriction(NULL)
{
    m_pool = vf_create_pool();
    m_Hash = new vf_Hash( m_pool );
    return;
} // vf_TypePool::vf_TypePool

/**
 * Type constraint collection destructor.
 */
vf_TypePool::~vf_TypePool()
{
    delete m_Hash;
    vf_delete_pool( m_pool );
} // vf_TypePool::vf_TypePool

/**
 * Function creates valid type which is identical to current class.
 */
vf_ValidType_t *
vf_TypePool::NewType( const char *type,     // class name
                      unsigned len)         // name length
{
    vf_ValidType_t *result;
    vf_HashEntry_t *hash;

    // find type in hash
    hash = m_Hash->NewHashEntry( type, len );
    if( hash->data ) {
        assert( ((vf_ValidType_t*)hash->data)->number == 1 );
        return (vf_ValidType_t*)hash->data;
    }

    // create and set type
    result = (vf_ValidType_t*)vf_alloc_pool_memory( m_pool, sizeof(vf_ValidType_t) );
    result->number = 1;
    result->string[0] = hash->key;
    // set type in hash
    hash->data = result;

    return result;
} // vf_TypePool::NewType


static
int vf_type_string_compare( const void *type_string1, const void *type_string2 )
{
    if( type_string1 < type_string2 ) {
        return -1;
    } else if( type_string1 > type_string2 ) {
        return 1;
    } else {
        return 0;
    }
} // vf_type_string_compare

/**
 * Function creates valid type which is identical to an element of a given array type.
 */
vf_ValidType_t *
vf_TypePool::NewArrayElemType( vf_ValidType_t *array )         // array type
{
    vf_ValidType_t *result;
    vf_HashEntry_t *hash;

    // lookup type in hash
    hash = m_Hash->NewHashEntry( &array->string[0][1] );
    if( array->number == 1 && hash->data ) {
        assert( ((vf_ValidType_t*)hash->data)->number == 1 );
        return (vf_ValidType_t*)hash->data;
    }

    // create and set type
    result = (vf_ValidType_t*)vf_alloc_pool_memory( m_pool, 
        sizeof(vf_ValidType_t) + (array->number - 1) * sizeof(const char*) );
    result->number = array->number;
    // set sting type
    result->string[0] = hash->key;
    if( result->number == 1 ) {
        // set type in hash
        hash->data = result;
    } else {
        // set other string types
        for( unsigned index = 1; index < array->number; index++ ) {
            hash = m_Hash->NewHashEntry( &array->string[index][1] );
            result->string[index] = hash->key;
        }
        // sort valid type
        qsort( &result->string[0], result->number, sizeof(const char*), vf_type_string_compare );
    }

    return result;
} // vf_TypePool::NewType

/**
 * Function dumps constraint collection in stream.
 */
void
vf_TypePool::DumpTypeConstraints( ostream *out )    // output stream
{
    if( out == NULL ) {
        out = &cerr;
    }
    for( vf_TypeConstraint_t *constraint = m_restriction;
        constraint;
        constraint = constraint->next )
    {
        *out << "CONSTRAINT: have \""
            << constraint->source << "\" need \"" << constraint->target << "\" for method "
            << class_get_name( method_get_class( m_method ) ) << "."
            << method_get_name( m_method ) << method_get_descriptor( m_method ) << endl;
    }
    return;
} // vf_TypePool::DumpTypeConstraints

/**
 * Function returns the methods constraints array.
 */
inline vf_TypeConstraint_t *
vf_TypePool::GetClassConstraint()
{
    return m_restriction;
} // vf_TypePool::GetClassConstraint

/**
 * Function sets current context method.
 */
void
vf_TypePool::SetMethod( method_handler method )
{
    // set method
    m_method = method;
    return;
} // vf_TypePool::SetMethod

/**
 * Function sets restriction from target class to source class.
 */
void
vf_TypePool::SetRestriction( const char *target,                // target class name
                             const char *source,                // source class name
                             unsigned short index,              // constant pool index
                             vf_CheckConstraint_t check_type)   // constraint check type
{
    vf_TypeConstraint_t *restriction;

    // lookup restriction
    for( restriction = m_restriction;
         restriction;
         restriction = restriction->next)
    {
        if( restriction->target == target
            && restriction->source == source
            && restriction->index == index
            && restriction->check_type == check_type )
        {
            // this restriction is already present
            return;
        }
    }

    // set restriction
    restriction = (vf_TypeConstraint_t*)vf_alloc_pool_memory( m_pool,
                        sizeof(vf_TypeConstraint_t) );
    restriction->target = target;
    restriction->source = source;
    restriction->index = index;
    restriction->check_type = check_type;
    restriction->method = m_method;
    restriction->next = m_restriction;
    m_restriction = restriction;

    // trace restriction
    if( index ) {
        VERIFY_TRACE( "constraint", "CONSTRAINT: for class \""
            << class_get_name( method_get_class( m_method ) ) << "\" CP index #" << index
            << " check access: have \"" << source << "\" need \"" << target << "\" for method "
            << class_get_name( method_get_class( m_method ) ) << "."
            << method_get_name( m_method ) << method_get_descriptor( m_method ) );
    } else {
        VERIFY_TRACE( "constraint", "CONSTRAINT: have \""
            << source << "\" need \"" << target << "\" for method "
            << class_get_name( method_get_class( m_method ) ) << "."
            << method_get_name( m_method ) << method_get_descriptor( m_method ) );
    }
    return;
} // vf_TypePool::SetRestriction

/**
 * Function checks types and create constraints if it's necessarily.
 */
bool
vf_TypePool::CheckTypes( vf_ValidType_t *required,        // required type
                         vf_ValidType_t *available,       // available type
                         unsigned short index,            // constant pool index
                         vf_CheckConstraint_t check_type) // constraint check type
{
    unsigned index1,
             index2;

    switch( check_type )
    {
    case VF_CHECK_ARRAY:    // provide array check
        // check if available types are array
        for( index1 = 0; index1 < available->number; index1++ ) {
            if( available->string[index1][0] != '[' ) {
                // type isn't array, return error
                return true;
            }
        }
        return false;

    case VF_CHECK_REF_ARRAY:    // provide reference array check
        for( index1 = 0; index1 < available->number; index1++ ) {
            if( available->string[index1][0] == '['
                && (available->string[index1][1] == '[' 
                    || available->string[index1][1] == 'L') )
            {
                // type is reference array, continue loop
                continue;
            } else {
                // type isn't array, return error
                return true;
            }
        }
        return false;

    case VF_CHECK_EQUAL:    // check equivalence
        // check if available types are equal
        return !vf_is_types_equal( required, available );

    case VF_CHECK_PARAM:        // check method invocation conversion
    case VF_CHECK_ASSIGN:       // check assignment conversion
    case VF_CHECK_ASSIGN_WEAK:  // check weak assignment conversion
        // compare types
        for( index1 = 0; index1 < required->number; index1++ ) {
            for( index2 = 0; index2 < available->number; index2++ ) {
                // set constraint for differing types
                if( required->string[index1] != available->string[index2] ) {
                    SetRestriction( required->string[index1], available->string[index2],
                                    0, check_type );
                }
            }
        }
        return false;

    case VF_CHECK_ACCESS_FIELD:    // check field access
        // compare types
        assert( required->number == 1 );
        for( index1 = 0; index1 < available->number; index1++ ) {
            // set access and type constraints for differing types
            if( required->string[0] != available->string[index1] ) {
                SetRestriction( required->string[0], available->string[index1],
                                0, VF_CHECK_ASSIGN );
            }
            SetRestriction( required->string[0], available->string[index1],
                            index, VF_CHECK_ACCESS_FIELD );
        }
        return false;

    case VF_CHECK_ACCESS_METHOD:   // check method access
        // compare types
        assert( required->number == 1 );
        for( index1 = 0; index1 < available->number; index1++ ) {
            // set access and type constraints for differing types
            if( required->string[0] != available->string[index1] ) {
                SetRestriction( required->string[0], available->string[index1],
                                0, VF_CHECK_PARAM );
            }
            SetRestriction( required->string[0], available->string[index1],
                            index, VF_CHECK_ACCESS_METHOD );
        }
        return false;
    case VF_CHECK_DIRECT_SUPER:
        if( required->number == 1 && available->number == 1 ) {
            if( required->string[0] != available->string[0] ) {
                SetRestriction( required->string[0], available->string[0],
                                    0, VF_CHECK_DIRECT_SUPER );
            }
            return false;
        }
        return true;

    case VF_CHECK_INVOKESPECIAL:
        assert( required->number == 1 );
        for( index1 = 0; index1 < available->number; index1++ ) {
            SetRestriction( required->string[0], available->string[index1],
                            index, VF_CHECK_INVOKESPECIAL );
        }
        return false;
    default:
        LDIE(39, "Verifier: CompareTypes: unknown check type in switch");
        return true;
    }
    // unreachable code
    assert(0);
} // vf_TypePool::CheckTypes

/**
 * Function merges two valid types.
 * Function returns NULL if vector wasn't merged.
 */
vf_ValidType_t *
vf_TypePool::MergeTypes( vf_ValidType_t *first,      // first merged type
                         vf_ValidType_t *second)     // second merged type
{
    // check null reference
    if( first == NULL ) {
        return second;
    } else if( second == NULL ) {
        return first;
    }

    // count differ types
    unsigned index1;
    unsigned index2;
    unsigned last_found;
    unsigned count = first->number + second->number;
    for( index1 = last_found = 0; index1 < first->number; index1++ ) {
        // find first type in second types
        for( index2 = last_found; index2 < second->number; index2++ ) {
            if( first->string[index1] == second->string[index2] ) {
                // found first type
                last_found = index2 + 1;
                count--;
                break;
            } else if( first->string[index1] < second->string[index2] ) {
                break;
            }
        }
    }
    if( first->number == second->number && count == first->number ) {
        // types are equal, no need to merge
        return NULL;
    }

    // create merged type
    vf_ValidType_t *type = (vf_ValidType_t*)vf_alloc_pool_memory( m_pool, 
        sizeof(vf_ValidType_t) + (count - 1) * sizeof(const char*) );
    type->number = count;

    // set type in ascending order of types string
    index1 = index2 = 0;
    for( unsigned index = 0; index < count; index++ ) {
        if( index1 >= first->number ) {
            type->string[index] = second->string[index2++];
        } else if( index2 >= second->number ) {
            type->string[index] = first->string[index1++];
        } else if( first->string[index1] < second->string[index2] ) {
            type->string[index] = first->string[index1++];
        } else if( first->string[index1] > second->string[index2] ) {
            type->string[index] = second->string[index2++];
        } else {
            type->string[index] = first->string[index1++];
            index2++;
        }
    }
    return type;
} // vf_TypePool::MergeTypes

/************************************************************
 ***************** Constraints functions ********************
 ************************************************************/

/**
 * Function sets constraint for method in class loader verify data.
 */
static void
vf_set_class_constraint( vf_TypeConstraint_t *collection,  // constraint for the class
                         vf_Context_t *ctex)               // verifier context
                         
{
    // get class loader of current class
    classloader_handler class_loader = class_get_class_loader( ctex->m_class );

    // lock data modification
    cl_acquire_lock( class_loader );
    vf_ClassLoaderData_t *cl_data = 
        (vf_ClassLoaderData_t*)cl_get_verify_data_ptr( class_loader );

    // create class loader data
    if( cl_data == NULL ) {
        vf_VerifyPool_t *new_pool = vf_create_pool();
        cl_data = (vf_ClassLoaderData_t*)vf_alloc_pool_memory( new_pool, 
                        sizeof(vf_ClassLoaderData_t) );
        cl_data->pool = new_pool;
        cl_data->string = new vf_Hash( new_pool );
        // set verify data for class loader
        cl_set_verify_data_ptr( class_loader, cl_data );
    }
    vf_VerifyPool_t *pool = cl_data->pool;
    vf_Hash_t *hash = cl_data->string;

    // create class constraints collection
    vf_TypeConstraint_t *save_collection = NULL;
    for( vf_TypeConstraint_t *restriction = collection;
         restriction;
         restriction = restriction->next )
    {
        // create constraint
        vf_TypeConstraint_t *constraint = 
            (vf_TypeConstraint_t*)vf_alloc_pool_memory( pool, sizeof(vf_TypeConstraint_t) );
        // create entry in string pool for target class
        vf_HashEntry_t *hash_entry = hash->NewHashEntry( restriction->target );
        constraint->target = hash_entry->key;
        // create entry in string pool for checked class
        hash_entry = hash->NewHashEntry( restriction->source );
        constraint->source = hash_entry->key;
        constraint->method = restriction->method;
        constraint->check_type = restriction->check_type;
        constraint->next = save_collection;
        save_collection = constraint;
    }
    assert( save_collection );

    // save method verify data
    assert( class_get_verify_data_ptr( ctex->m_class ) == NULL );
    class_set_verify_data_ptr( ctex->m_class, save_collection );

    // unlock data modification
    cl_release_lock( class_loader );
    return;
} // vf_set_class_constraint

/**
 * Function receives class by given class name, loads it if it's needed.
 */
static class_handler
vf_resolve_class( const char *name,         // resolved class name
                  bool need_load,           // load flag
                  vf_Context_t *ctex)       // verifier context
{
    // get class name
    if( name[0] == 'L' ) {
        // class is class, skip 'L'
        name++;
    } else {
        // class is array
        unsigned index = 0;
        do {
            index++;
        } while( name[index] == '[' );
        if( name[index] == 'L' ) {
            // array of objects, construct array name
            unsigned len = strlen( name );
            char *buf = (char *)STD_ALLOCA( len + 2 );
            memcpy( buf, name, len );
            buf[len] = ';';
            buf[len + 1] = '\0';
            name = buf;
        }
    }

    // get class loader
    classloader_handler class_loader = class_get_class_loader( ctex->m_class );

    // receive class
    class_handler result;
    if( need_load ) {
        // trace verifier loads
        VERIFY_TRACE( "load", "verify load: class " << name );
        result = cl_load_class( class_loader, name );
    } else {
        result = cl_get_class( class_loader, name );
    }

    return result;
} // vf_resolve_class

/**
 * Function checks if target class is super class of source class. 
 */
static inline bool
vf_is_super_class( class_handler source,        // checked class
                   class_handler target)        // super class
{
    // check super class
    for( ; source; source = class_get_super_class( source ) ) {
        if( class_is_same_class( source, target ) ) {
            return true;
        }
    }
    return false;
} // vf_is_super_class

/**
 * Function checks if target class is super interface of source class. 
 */
static inline bool
vf_is_super_interface( class_handler source,    // checked class
                       class_handler target)    // super interface
{
    assert( class_is_interface_( target ) );
    // classes are equal
    if( class_is_same_class( source, target ) ) {
        return true;
    }

    // check super interface
    for( ; source; source = class_get_super_class( source ) ) {
        for( unsigned index = 0;
             index < class_get_superinterface_number( source );
             index++ )
        {
            if( vf_is_super_interface( class_get_superinterface( source, (unsigned short)index ), target ) ) {
                return true;
            }
        }
    }
    return false;
} // vf_is_super_interface

/**
 * Function checks method invocation conversion between source ans target classes.
 */
static bool
vf_is_param_valid( class_handler source,    // checked class
                   class_handler target)    // required class
{
    // check if target class is array
    if( class_is_array( target ) && class_is_array( source ) ) {
        // get array element classes
        return vf_is_param_valid( class_get_array_element_class( source ),
                                  class_get_array_element_class( target ));
    }

    // check widening reference conversions
    if( class_is_interface_( target ) ) {
        // target class is interface
        return vf_is_super_interface( source, target );
    } else {
        // target class is class
        return vf_is_super_class( source, target );
    }
} // vf_is_param_valid

/**
 * Function checks narrow reference conversion between interface classes.
 * If class1 has method1 and class2 has method2, correspondingly,
 * and methods are the same name and signature, function returns 1.
 * If methods have different return types, function returns 0.
 */
static inline bool
vf_check_interface_methods( class_handler class1,   // first interface class
                            class_handler class2)   // second interface class
{
    assert( class_is_interface_( class1 ) );
    assert( class_is_interface_( class2 ) );

    // check interfaces methods    
    for( unsigned index = 0; index < class_get_method_number( class2 ); index++ ) {
        method_handler method1 = class_get_method( class2, index );
        for( unsigned count = 0; count < class_get_method_number( class1 ); count++ ) {
            method_handler method2 = class_get_method( class1, count );
            if( !strcmp( method_get_name( method1 ), method_get_name( method2 ) ) )
            {
                // interfaces have methods with the same name
                const char *sig1 = method_get_descriptor( method1 );
                const char *sig2 = method_get_descriptor( method2 );
                char *end_params = strrchr( sig1, ')' );
                unsigned len = end_params - sig1 + 1;
                if( !memcmp( sig1, sig2, len ) ) {
                    // methods arguments are the same
                    if( strcmp( &sig1[len], &sig2[len] ) ) {
                        // methods have different return types
                        return false;
                    }
                }
            }

        }
    }
    return true;
} // vf_check_interface_methods

/**
 * Function checks casting conversion between classes.
 */
static bool
vf_is_checkcast_valid( class_handler source,    // checked class
                       class_handler target)    // required class
{
    // check if target class and source class are array
    if( class_is_array( target ) && class_is_array( source ) ) {
        // get array element classes
        return vf_is_checkcast_valid( class_get_array_element_class( source ),
                                      class_get_array_element_class( target ));
    }

    // check widening reference conversions
    if( vf_is_param_valid( source, target ) ) {
        return true;
    }

    // check narrowing reference conversions
    if( class_is_interface_( source ) ) {
        // source class is interface
        if( class_is_interface_( target ) ) {
            // target class is interface
            return vf_check_interface_methods( source, target );
        } else {
            // target class is class
            if( class_is_final_( target ) ) {
                // target class is final
                return vf_is_super_interface( target, source );
            } else {
                // target class isn't final
                return true;
            }
        }
    } else {
        // source class is class
        if( !memcmp( class_get_name( source ), "java/lang/Object", 17 ) ) {
            // source class is Object
            return true;
        }
        if( class_is_interface_( target ) ) {
            // target class is interface
            if( !class_is_final_( source ) ) {
                // super class isn't final
                return true;
            }
        } else {
            // target class is class
            return vf_is_super_class( target, source );
        }
    }
    return false;
} // vf_is_checkcast_valid

/**
 * Function checks assignment conversion between classes.
 *
 * If strict flag is true, strict assignment compatible check is provided,
 * else if strict flag is false, weak assignment compatible check is provided.
 *
 * Strict assignment compatible check is provided for elements of array.
 * For weak assignment strict compatible check is provided for runtime by
 * execution engine.
 *
 * Here is an example of weak and strict assignment compatible check:
 *
 * ------------------------- weak check -----------------------------
 *
 *     Class1[] instance = new Class2[n];
 *     instance[0] = function_return_Class1();
 *     
 *     Class1 function_return_Class1() {
 *         return new Class2();
 *     }
 * 
 * Where Class2 extends or implements Class1.
 *
 * === bytecode ===
 *
 *   0: iconst_5
 *   1: anewarray #2=<Class Class2>
 *   4: astore_1
 *   5: aload_1
 *   6: iconst_0
 *   7: invokestatic #3=<Method test_weak1.function_return_Class1 ()Class1>
 *  10: aastore //-------------> weak assignment compatible check
 *  11: return
 *
 * Test is valid.
 *
 * ------------------------ strict check ----------------------------
 *
 *     Class1[][] instance = new Class2[n][];
 *     instance[0] = function_return_ArrayClass1();
 *     
 *     static Class1[] function_return_ArrayClass1() {
 *         return new Class2[5];
 *     }
 *
 * === bytecode ===
 *
 *   0: iconst_5
 *   1: anewarray #2=<Class [LClass2;>
 *   4: astore_1
 *   5: aload_1
 *   6: iconst_0
 *   7: invokestatic #3=<Method test_weak1.function_return_ArrayClass1 ()Class1[]>
 *  10: aastore //-------------> strict assignment compatible check
 *  11: return
 *
 * Test is invalid.
 */
static bool
vf_is_assign_valid( class_handler source,   // checked class
                    class_handler target,   // required class
                    bool is_strict)         // strict condition flag
{
    // check assignment reference conversions
    if( class_is_array( source ) ) {
        // source class is array
        if( class_is_interface_( target ) ) {
            // target class is interface
            if( !memcmp( class_get_name( target ), "java/lang/Cloneable", 20 )
                || !memcmp( class_get_name( target ), "java/io/Serializable", 21 ) )
            {
                // target class is Cloneable or Serializable
                return true;
            }
        } else {
            // target class is class
            if( !memcmp( class_get_name( target ), "java/lang/Object", 17 ) ) {
                // target class is object
                return true;
            } else if ( class_is_array( target ) ) {
                // get array element classes
                return vf_is_assign_valid( class_get_array_element_class( source ),
                                           class_get_array_element_class( target ),
                                           true);
            }
        }
    } else if( class_is_interface_( source ) ) {
        // source class is interface
        if( class_is_interface_( target ) ) {
            // target class is interface
            return vf_is_super_interface( source, target );
        } else {
            // target class is class
            if( !memcmp( class_get_name( target ), "java/lang/Object", 17 ) ) {
                // target class is object
                return true;
            }
            if( !is_strict ) {
                /**
                 * Strict correspondence is used only for element array compare.
                 * Compare interface and class is weaker because it's impossible to
                 * create pure interface.
                 */
                return vf_is_super_interface( target, source );
            }
        }
    } else {
        // source class is class
        bool valid = vf_is_param_valid( source, target );
        if( !valid && !is_strict ) {
            valid = vf_is_super_class( target, source );
        }
        return valid;
    }
    return false;
} // vf_is_assign_valid

/**
 * Function checks conversions between classes.
 */
static bool
vf_is_valid( class_handler source,              // checked class
             class_handler target,              // required class
             class_handler current,             // current class
             unsigned check)                    // checked class type
{
    switch( check )
    {
    case VF_CHECK_PARAM:            // method invocation conversion
        return vf_is_param_valid( source, target );
    case VF_CHECK_ASSIGN:           // assignment conversion
        return vf_is_assign_valid( source, target, true );
    case VF_CHECK_ASSIGN_WEAK:      // weak assignment conversion
        return vf_is_assign_valid( source, target, false );
    case VF_CHECK_CAST:             // casting conversion
        return vf_is_checkcast_valid( source, target );
    case VF_CHECK_SUPER:            // check if target is super class of source
        return vf_is_super_class( source, target );
    case VF_CHECK_DIRECT_SUPER:     // check if target is a direct super class of source
        return class_is_same_class( class_get_super_class( source ), target );
    case VF_CHECK_ACCESS_FIELD:     // protected field access
    case VF_CHECK_ACCESS_METHOD:    // protected method access
        return vf_is_super_class( source, current );
    case VF_CHECK_INVOKESPECIAL:    // check object for invokespecial instruction
        return vf_is_super_class( source, current )
                    && vf_is_super_class( current, target );
    default:
        LDIE(40, "Verifier: vf_is_valid: invalid check type" );
        return false;
    }
    // unreachable code
    assert(0);
} // vf_is_valid

/**
 * Sets verifier error.
 */
static inline void
vf_set_error( method_handler method,        // failed method
              unsigned check,               // failed check
              vf_Context_t *ctex)           // verifier context
{
    switch( check )
    {
    case VF_CHECK_PARAM:
        VERIFY_REPORT( ctex, "(class: " << class_get_name( ctex->m_class ) 
            << ", method: " << method_get_name( method )
            << method_get_descriptor( method )
            << ") Incompatible argument for function" );
        break;
    case VF_CHECK_ASSIGN:
        VERIFY_REPORT( ctex, "(class: " << class_get_name( ctex->m_class ) 
            << ", method: " << method_get_name( method )
            << method_get_descriptor( method )
            << ") Incompatible types for field assignment" );
        break;
    case VF_CHECK_ASSIGN_WEAK:
        VERIFY_REPORT( ctex, "(class: " << class_get_name( ctex->m_class ) 
            << ", method: " << method_get_name( method )
            << method_get_descriptor( method )
            << ") Incompatible types for array assignment" );
        break;
    case VF_CHECK_SUPER:
        VERIFY_REPORT( ctex, "(class: " << class_get_name( ctex->m_class ) 
            << ", method: " << method_get_name( method )
            << method_get_descriptor( method )
            << ") Exception class not a subclass of Throwable" );
        break;
    case VF_CHECK_ACCESS_FIELD:
        VERIFY_REPORT( ctex, "(class: " << class_get_name( ctex->m_class ) 
            << ", method: " << method_get_name( method )
            << method_get_descriptor( method )
            << ") Bad access to protected field" );
        break;
    case VF_CHECK_ACCESS_METHOD:
        VERIFY_REPORT( ctex, "(class: " << class_get_name( ctex->m_class ) 
            << ", method: " << method_get_name( method )
            << method_get_descriptor( method )
            << ") Bad access to protected method" );
        break;
    case VF_CHECK_DIRECT_SUPER:
        VERIFY_REPORT( ctex, "(class: " << class_get_name( ctex->m_class ) 
            << ", method: " << method_get_name( method )
            << method_get_descriptor( method )
            << ") Call to wrong initialization method" );
        break;
    case VF_CHECK_INVOKESPECIAL:
        VERIFY_REPORT( ctex, "(class: " << class_get_name( ctex->m_class ) 
            << ", method: " << method_get_name( method )
            << method_get_descriptor( method )
            << ") Incompatible object argument for invokespecial" );
        break;
    default:
        LDIE(41, "Verifier: vf_set_error: unknown check type" );
        break;
    }
    return;
} // vf_set_error

/**
 * Checks some constraints without loading of needed classes.
 */
static inline Verifier_Result
vf_check_without_loading( vf_TypeConstraint_t *restriction,  // checked constraint
                          vf_Context_t *ctex)                // verifier context
{
    switch( restriction->check_type )
    {
    case VF_CHECK_SUPER:
        /**
         * Extension for class java/lang/Object doesn't check
         * because it's expected all references extend it.
         */
        if( restriction->target == ctex->m_vtype.m_object->string[0] ) {
            // no need to check
            return VER_OK;
        }
        break;

    case VF_CHECK_PARAM:
        /**
         * Extension for class java/lang/Object doesn't check
         * because it's expected all references extend it.
         */
        if( restriction->target == ctex->m_vtype.m_object->string[0] ) {
            // no need to check
            return VER_OK;
        }

        /**
         * Extension for class [Ljava/lang/Object doesn't check
         * because it's expected all arrays extend it.
         * Just check is source array.
         */
        if( restriction->target == ctex->m_vtype.m_array->string[0]
            && restriction->source[0] == '[' 
            && (restriction->source[1] == '[' || restriction->source[1] == 'L') )
        {
            // no need to check
            return VER_OK;
        }

        /**
         * Extension for java/lang/Cloneable and java/io/Serializable
         * interfaces doesn't check because it's expected all arrays extend it.
         * Just check is source array.
         */
        if( (restriction->target == ctex->m_vtype.m_clone->string[0] 
             || restriction->target == ctex->m_vtype.m_serialize->string[0])
            && restriction->source[0] == '[' )
        {
            // no need to check
            return VER_OK;
        }

        /**
         * If method invocation conversion takes place between array and
         * non-array reference, return error.
         */
        if( (restriction->target[0] != '[' && restriction->source[0] == '[')
            || (restriction->target[0] != '[' && restriction->source[0] == '[') )
        {
            vf_set_error( ctex->m_method, VF_CHECK_PARAM, ctex );
            return VER_ErrorIncompatibleArgument;
        }
        break;

    case VF_CHECK_ASSIGN_WEAK:
        // check assignment weak reference conversions
        if( restriction->source[0] == 'L' ) {
            return VER_OK;
        }
        assert(restriction->source[0] == '[');
        // go to the next check...
    case VF_CHECK_ASSIGN:
        // check assignment reference conversions
        if( restriction->source[0] == '[' ) {
            // source class is array
            if( !memcmp( restriction->target, "Ljava/lang/Cloneable", 21 )
                || !memcmp( restriction->target, "Ljava/io/Serializable", 22 ) )
            {
                // target class is java/lang/Cloneable 
                // or java/lang/Serializable interface
                return VER_OK;
            } else if( restriction->target == ctex->m_vtype.m_object->string[0] ) {
                // target class is java/lang/Object
                return VER_OK;
            } else if ( restriction->target[0] != '[' ) {
                // target class isn't array class
                vf_set_error( ctex->m_method, restriction->check_type, ctex );
                return VER_ErrorIncompatibleArgument;
            }
        }
        break;

    default:
        break;
    }
    // need to load classes for check
    return VER_ClassNotLoaded;
} // vf_check_without_loading

/**
 * Function checks constraint between loaded classes.
 * If any class isn't loaded, function returns unloaded error
 * to store restriction to the class for future constraint check.
 */
static inline Verifier_Result
vf_check_constraint( vf_TypeConstraint_t *restriction,  // checked constraint
                     vf_Context_t *ctex)                // verifier context
{
    /**
     * Try to provide some checks without classes loading
     */
    if( !ctex->m_dump.m_verify )
    {
        Verifier_Result result = vf_check_without_loading( restriction, ctex );
        if( result != VER_ClassNotLoaded ) {
            // no need to check
            return result;
        }
    }

    // get target class handler
    class_handler target = vf_resolve_class( restriction->target, false, ctex);
    // get checked class
    class_handler source = vf_resolve_class( restriction->source, false, ctex);
    // check classes are loaded?
    if( !target || !source ) {
        return VER_ClassNotLoaded;
    }

    /**
     * Verifier which is built on Java VM Specification 2nd Edition (4.9.2)
     * recommendation of verification process doesn't check interfaces usage.
     * Unfortunately, a lot of Java applications depends on this neglect.
     * To be compatible with those applications we should do full constraint
     * checks only if -Xverify:all option is present in command line.
     */
    if( !ctex->m_dump.m_verify && class_is_interface_( target ) ) {
        // skip constraint check
        return VER_OK;
    }

    // check restriction
    if( !vf_is_valid( source, target, ctex->m_class, restriction->check_type ) ) {
        // return error
        vf_set_error( ctex->m_method, restriction->check_type, ctex );
        return VER_ErrorIncompatibleArgument;
    }
    return VER_OK;
} // vf_check_constraint

/**
 * Function checks access to protected field/method.
 * If function cannot check constraint because of any class isn't loaded,
 * function return unloaded error to store restriction to the class
 * for future constraint check.
 */
Verifier_Result
vf_check_access_constraint( const char *super_name,             // name of super class
                            const char *instance_name,          // name of instance class
                            unsigned short index,               // constant pool index
                            vf_CheckConstraint_t check_type,    // access check type
                            vf_Context_t *ctex)                 // verifier context
{
    // get class handler of super class
    class_handler super_class = vf_resolve_class( super_name, false, ctex );
    if( !super_class || !vf_is_super_class( ctex->m_class, super_class ) ) {
        // obtained class isn't super class of a given class, no need to check
        return VER_OK;
    }

    // check if a class and a parent class is in the same package
    if( class_is_same_package( ctex->m_class, super_class ) ) {
        // class and parent class is in the same package,
        // no need check access to protect members
        return VER_OK;
    }

    // check is a member protected
    bool need_check = false;
    if( check_type == VF_CHECK_ACCESS_FIELD ) {
        field_handler field = class_resolve_nonstatic_field( ctex->m_class, index );
        if( !field ) {
            // NoSuchFieldError or IllegalAccessError - nothing to check
            VERIFY_DEBUG( "verifying class " << class_get_name( ctex->m_class )
                << " (method " << method_get_name( ctex->m_method )
                << method_get_descriptor( ctex->m_method )
                << ") couldn't resolve field with constant pool index #" << index );
            return VER_OK;
        }
        if( field_is_protected( field ) ) {
            need_check = true;
        }
    } else {
        method_handler method = class_resolve_method( ctex->m_class, index );
        if(!method || method_is_static(method)) {
            // NoSuchMethodError or IllegalAccessError - nothing to check
            VERIFY_DEBUG( "verifying class " << class_get_name( ctex->m_class )
                << " (method " << method_get_name( ctex->m_method )
                << method_get_descriptor( ctex->m_method )
                << ") couldn't resolve method with constant pool index #" << index );
            return VER_OK;
        }
        if( method_is_protected( method ) ) {
            if( instance_name[0] == '[' && !memcmp( method_get_name( method ), "clone", 6 ) ) {
                // for arrays function clone is public
            } else {
                need_check = true;
            }
        }
    }
    if( !need_check ) {
        // no need to check
        return VER_OK;
    }

    // get instance class
    class_handler instance = vf_resolve_class( instance_name, false, ctex );
    if( !instance ) {
        // instance class isn't loaded
        return VER_ClassNotLoaded;
    }

    // check access constraint
    if( !vf_is_valid( instance, NULL, ctex->m_class, check_type ) ) {
        // return error
        vf_set_error( ctex->m_method, check_type, ctex );
        return VER_ErrorIncompatibleArgument;
    }
    return VER_OK;
} // vf_check_access_constraint

/**
 * Function provides initial constraint checks for current class.
 * Function checks only loaded classes, and stores restriction for unloaded ones.
 */
Verifier_Result
vf_check_class_constraints( vf_Context_t *ctex )    // verifier context
{
    // set class restriction collection
    vf_TypeConstraint_t *last = NULL;
    vf_TypeConstraint_t *collection = ctex->m_type->GetClassConstraint();

    // check constraints
    for( vf_TypeConstraint_t *constraint = collection;
         constraint;
         constraint = constraint->next )
    {
        // set context method
        ctex->m_method = constraint->method;

        // check constraint
        Verifier_Result result = vf_check_constraint( constraint, ctex );
        if( result == VER_OK ) {
            // constraint checked, remove constraint from the collection
            if( !last ) {
                collection = constraint->next;
            } else {
                last->next = constraint->next;
            }
        } else if( result != VER_ClassNotLoaded ) {
            // return error
            return result;
        } else {
            // set the last constraint
            last = constraint;
        }
    }
    if( collection ) {
        // set constraint for further checking
        vf_set_class_constraint( collection, ctex );
    }

    return VER_OK;
} // vf_check_class_constraints

/**
 * Function checks constraint for given class.
 * Function loads classes if it's needed.
 */
static inline Verifier_Result
vf_force_check_constraint( vf_TypeConstraint_t *constraint,     // class constraint
                           vf_Context_t *ctex)                  // verifier context
{
    // check if constraint is already verified
    if( constraint->check_type == VF_CHECK_NONE ) {
        // already verified
        VERIFY_TRACE( "constraint.checked", "verify constraint: have \""
            << constraint->source << "\" need \"" << constraint->target 
            << "\" already done (check #1) for class "
            << class_get_name( ctex->m_class ) );
        return VER_OK;
    }

    // get target class
    class_handler target = vf_resolve_class( constraint->target, true, ctex );
    if( !target ) {
        VERIFY_DEBUG( "verifying class " << class_get_name( ctex->m_class )
            << " (method " << method_get_name( constraint->method )
            << method_get_descriptor( constraint->method )
            << ") couldn't load class \""
            << ((constraint->target[0] == 'L')
                ? &(constraint->target[1]) : constraint->target )
            << "\"");
        unsigned index = 0;
        while( constraint->target[index++] != 'L' ) {
            assert( constraint->target[index] != '\0' );
        }
        VERIFY_REPORT( ctex, &(constraint->target[index]) );
        return VER_ErrorLoadClass;
    }

    // check if constraint is already verified
    if( constraint->check_type == VF_CHECK_NONE ) {
        // already verified
        VERIFY_TRACE( "constraint.checked", "verify constraint: have \""
            << constraint->source << "\" need \"" << constraint->target 
            << "\" already done (check #2) for class "
            << class_get_name( ctex->m_class ) );
        return VER_OK;
    }

    /**
     * Verifier which is built on Java VM Specification 2nd Edition (4.9.2)
     * recommendation of verification process doesn't check interfaces usage.
     * Unfortunately, a lot of Java applications depends on this neglect.
     * To be compatible with those applications we should do full constraint
     * checks only if -Xverify:all option is present in command line.
     */
    if( !ctex->m_dump.m_verify && class_is_interface_( target ) ) {
        // skip constraint check
        // reset constraint to successful
        constraint->check_type = VF_CHECK_NONE;
        return VER_OK;
    }

    // check if constraint is already verified
    if( constraint->check_type == VF_CHECK_NONE ) {
        // already verified
        VERIFY_TRACE( "constraint.checked", "verify constraint: have \""
            << constraint->source << "\" need \"" << constraint->target 
            << "\" already done (check #3) for class "
            << class_get_name( ctex->m_class ) );
        return VER_OK;
    }

    // get stack reference class
    class_handler source = vf_resolve_class( constraint->source, true, ctex );
    if( !source ) {
        VERIFY_DEBUG( "verifying class " << class_get_name( ctex->m_class )
            << " (method " << method_get_name( constraint->method )
            << method_get_descriptor( constraint->method )
            << ") couldn't load class \""
            << ((constraint->source[0] == 'L')
                ? &(constraint->source[1]) : constraint->source )
            << "\"");
        unsigned index = 0;
        while( constraint->source[index++] != 'L' ) {
            assert( constraint->source[index] != '\0' );
        }
        VERIFY_REPORT( ctex, &(constraint->source[index]) );
        return VER_ErrorLoadClass;
    }

    // store constraint check type (it could be changed during validation check)
    vf_CheckConstraint_t check = (vf_CheckConstraint_t)constraint->check_type;

    // check if constraint is already verified
    if( check == VF_CHECK_NONE ) {
        // already verified
        VERIFY_TRACE( "constraint.checked", "verify constraint: have \""
            << constraint->source << "\" need \"" << constraint->target 
            << "\" already done (check #4) for class "
            << class_get_name( ctex->m_class ) );
        return VER_OK;
    }

    // check restriction
    if( !vf_is_valid( source, target, ctex->m_class, check ) ) {
        // return error
        vf_set_error( constraint->method, check, ctex );
        return VER_ErrorIncompatibleArgument;
    }
    // reset constraint to successful
    constraint->check_type = VF_CHECK_NONE;
    return VER_OK;
} // vf_force_check_constraint

/**
 * Function verifies class constraints.
 */
Verifier_Result
vf_verify_class_constraints( vf_Context_t *ctex )        // verifier context
{
    // get method verify data
    vf_TypeConstraint_t *constraint = 
        (vf_TypeConstraint_t*)class_get_verify_data_ptr( ctex->m_class );
    if( constraint == NULL ) {
        return VER_OK;
    }

    // trace verified class
    VERIFY_TRACE( "class.constraint", "verify constraints: " << class_get_name( ctex->m_class ) );

    // check method constraints
    Verifier_Result result = VER_OK;
    for( ; constraint; constraint = constraint->next )
    {
        result = vf_force_check_constraint( constraint, ctex );
        if( result != VER_OK ) {
            break;
        }
    }
    return result;
} // vf_verify_class_constraints

/************************************************************
 ******************** Memory functions **********************
 ************************************************************/

/**
 * Function performs abend exit from VM.
 */
void
vf_error_func( VERIFY_SOURCE_PARAMS )
{
    LDIE(42, "{0} Verifier: abort!" << VERIFY_REPORT_SOURCE );
    exit(1);
} // vf_error_func

/**
 * Function allocates an array in memory with elements initialized to zero. 
 */
void*
vf_calloc_func( unsigned num,            // number of elements
                unsigned size,           // size of element
                VERIFY_SOURCE_PARAMS)    // debug info
{
    assert(num);
    assert(size);
    void *result = STD_CALLOC( num, size );
    if( result == NULL ) {
        // out of memory error
        LECHO(41, "Verifier: {0}: out of memory" << "vf_calloc_func");
        vf_error();
    }

#if VERIFY_TRACE_MEMORY
    // trace memory
    VERIFY_TRACE("memory", VERIFY_REPORT_SOURCE
        << "(calloc) allocate memory addr: " << result
        << ", size: " << size * num << " (" << num << " by " << size << ")");
#endif // VERIFY_TRACE_MEMORY

    return result;
} // vf_calloc_func

/**
 * Function allocates memory blocks.
 */
void *
vf_malloc_func( unsigned size,          // size of memory block
                VERIFY_SOURCE_PARAMS)   // debug info
{
    assert( size );
    void *result = STD_MALLOC( size );
    if( result == NULL ) {
        // out of memory error
        LECHO(41, "Verifier: {0}: out of memory" << "vf_malloc_func");
        vf_error();
    }

#if VERIFY_TRACE_MEMORY
    // trace memory
    VERIFY_TRACE("memory", VERIFY_REPORT_SOURCE
        << "(malloc) allocate memory addr: " << result 
        << ", size: " << size);
#endif // VERIFY_TRACE_MEMORY

    return result;
} // vf_malloc_func

/**
 * Function reallocates memory blocks.
 */
void *
vf_realloc_func( void *pointer,             // old pointer
                 unsigned size,             // size of memory block
                 VERIFY_SOURCE_PARAMS)      // debug info
{
    assert( size );
    void *result = STD_REALLOC( pointer, size );
    if( result == NULL ) {
        // out of memory error
        LECHO(41, "Verifier: {0}: out of memory" << "vf_realloc_func");
        vf_error();
    }

#if VERIFY_TRACE_MEMORY
    // trace memory
    VERIFY_TRACE("memory", VERIFY_REPORT_SOURCE
        << "(realloc) reallocate memory from addr: " << pointer
        << " to addr: " << result << ", size: " << size );
#endif // VERIFY_TRACE_MEMORY

    return result;
} // vf_realloc_func

/**
 * Function releases allocated memory blocks.
 */
void
vf_free_func( void *pointer,            // free pointer
              VERIFY_SOURCE_PARAMS)     // debug info
{
    if( pointer ) {
        STD_FREE( pointer );
    } else {
        LECHO(41, "Verifier: {0}: null pointer for free" << "vf_free_func" );
        vf_error();
    }

#if VERIFY_TRACE_MEMORY
    // trace memory
    VERIFY_TRACE("memory", VERIFY_REPORT_SOURCE
        << "(free) free memory addr: " << (void*)pointer );
#endif // VERIFY_TRACE_MEMORY

    return;
} // vf_free_func

/**
 * Function creates wide memory pool structure.
 */
static inline vf_VerifyPoolInternal_t *
vf_create_pool_element( unsigned size,             // initial pool size
                        VERIFY_SOURCE_PARAMS)      // debug info
{
    vf_VerifyPoolInternal_t *result;

    // create pool new entry and allocate memory for it
    result = (vf_VerifyPoolInternal_t*)
        vf_malloc_func( sizeof(vf_VerifyPoolInternal_t) + size, VERIFY_SOURCE_ARGS1 );
    result->m_memory = (char*)result + sizeof(vf_VerifyPoolInternal_t);
    result->m_free = (char*)result->m_memory;
    result->m_freesize = size;
    result->m_next = NULL;
    memset(result->m_memory, 0, size);

#if VERIFY_TRACE_MEMORY
    // trace memory
    VERIFY_TRACE("memory.pool.element", VERIFY_REPORT_SOURCE
        << "(pool) create pool element: " << result 
        << ", memory: " << result->m_memory << ", size: " << result->m_freesize);
#endif // VERIFY_TRACE_MEMORY

    return result;
} // vf_create_pool_element

/**
 * Function creates memory pool structure.
 */
vf_VerifyPool_t *
vf_create_pool_func( VERIFY_SOURCE_PARAMS )
{
    // create new pool element
    vf_VerifyPoolInternal_t *pool =
        vf_create_pool_element( sizeof(vf_VerifyPool_t) + VERIFY_POOL_ENTRY_SIZE,
                                 VERIFY_SOURCE_ARGS1 );
    // set head pool
    vf_VerifyPool_t *result = (vf_VerifyPool_t*)pool->m_free;
    pool->m_memory = pool->m_free + sizeof(vf_VerifyPool_t);
    pool->m_free = (char*)pool->m_memory;
    pool->m_freesize = VERIFY_POOL_ENTRY_SIZE;
    result->m_pool = pool;
    result->m_memory = sizeof(vf_VerifyPoolInternal_t) + sizeof(vf_VerifyPool_t)
                        + VERIFY_POOL_ENTRY_SIZE;
    result->m_used = 0;
    result->m_maxuse = 0;

#if VERIFY_TRACE_MEMORY
    // trace memory
    VERIFY_TRACE("memory.pool", VERIFY_REPORT_SOURCE
        << "(pool) create pool: " << result );
#endif // VERIFY_TRACE_MEMORY

    return result;
} // vf_create_pool_func

/**
 * Function allocates memory block in current pool.
 */
void *
vf_alloc_pool_memory_func( vf_VerifyPool_t *hpool,     // a given pool
                           unsigned size,              // memory size
                           VERIFY_SOURCE_PARAMS)       // debug info
{
    const unsigned align = sizeof(void*) - 1;
    void *result = NULL;
    vf_VerifyPoolInternal_t *pool = hpool->m_pool;

    // align allocate size
    size = (size + align) & (~align);
    // find free space
    if( size > VERIFY_POOL_ENTRY_SIZE ) {
        // create new wide pool entry
        pool = vf_create_pool_element( size, VERIFY_SOURCE_ARGS1 );
        pool->m_next = hpool->m_pool;
        hpool->m_pool = pool;
        hpool->m_memory += sizeof(vf_VerifyPoolInternal_t) + size;
    } else if( pool->m_freesize < size ) {
        vf_VerifyPoolInternal_t *last = NULL;
        vf_VerifyPoolInternal_t *entry = pool->m_next;
        while( entry ) {
            last = pool;
            pool = entry;
            if( pool->m_freesize >= size ) {
                // found free space 
                break;
            }
            entry = pool->m_next;
        }
        if( !entry ) {
            // create new pool element
            pool = vf_create_pool_element( VERIFY_POOL_ENTRY_SIZE, VERIFY_SOURCE_ARGS1 );
            pool->m_next = hpool->m_pool;
            hpool->m_pool = pool;
            hpool->m_memory += sizeof(vf_VerifyPoolInternal_t) + VERIFY_POOL_ENTRY_SIZE;
        } else {
            assert( last != NULL );
            last->m_next = pool->m_next;
            pool->m_next = hpool->m_pool;
            hpool->m_pool = pool;
        }
    }
    assert( hpool->m_used + size < hpool->m_memory );
    result = pool->m_free;
    pool->m_free += size;
    pool->m_freesize -= size;
    hpool->m_used += size;

#if VERIFY_TRACE_MEMORY
    // trace memory
    VERIFY_TRACE("memory.pool", VERIFY_REPORT_SOURCE
        << "(pool) allocate memory in pool: " << hpool
        << ", memory: " << result << ", size: " << size 
        << ", element: " << pool << ", free: " << pool->m_freesize);
#endif // VERIFY_TRACE_MEMORY

    return result;
} // vf_alloc_pool_memory_func

/**
 * Function cleans given pool.
 */
void
vf_clean_pool_memory_func( vf_VerifyPool_t *hpool,   // memory pool
                           VERIFY_SOURCE_PARAMS)     // debug info
{
    // set max used value
    if( hpool->m_used > hpool->m_maxuse ) {
        hpool->m_maxuse = hpool->m_used;
    }

#if VERIFY_TRACE_MEMORY
    // trace memory
    VERIFY_TRACE("memory.pool", VERIFY_REPORT_SOURCE
        << "(pool) clean pool: " << hpool
        << ", allocated: " << hpool->m_memory << ", used: " << hpool->m_used);
#endif // VERIFY_TRACE_MEMORY

    vf_VerifyPoolInternal_t *pool = hpool->m_pool;
    while( pool ) {
        // clean pool element space
        unsigned used_size = pool->m_free - (char*)pool->m_memory;
        memset(pool->m_memory, 0, used_size);
        pool->m_free = (char*)pool->m_memory;
        pool->m_freesize += used_size;
        hpool->m_used -= used_size;

#if VERIFY_TRACE_MEMORY
        // trace memory
        VERIFY_TRACE("memory.pool.element", VERIFY_REPORT_SOURCE
            << "(pool) clean pool element: " << pool
            << ", size: " << used_size);
#endif // VERIFY_TRACE_MEMORY

        // get next pool entry
        pool = pool->m_next;
    }
    return;
} // vf_clean_pool_memory_func

/**
 * Function releases memory from given pool.
 */
void
vf_delete_pool_func( vf_VerifyPool_t *hpool,    // memory pool
                     VERIFY_SOURCE_PARAMS)      // debug info
{
#if VERIFY_TRACE_MEMORY
    // trace memory
    VERIFY_TRACE("memory.pool", VERIFY_REPORT_SOURCE
        << "(pool) delete pool: " << hpool
        << ", allocated: " << hpool->m_memory 
        << ", used: "
        << (hpool->m_used > hpool->m_maxuse ? hpool->m_used : hpool->m_maxuse) );
#endif // VERIFY_TRACE_MEMORY

    vf_VerifyPoolInternal_t *pool = hpool->m_pool;
    while( pool ) {
#if VERIFY_TRACE_MEMORY
        // trace memory
        VERIFY_TRACE("memory.pool.element", VERIFY_REPORT_SOURCE
            << "(pool) delete pool element: " << pool);
#endif // VERIFY_TRACE_MEMORY

        // store pool element
        vf_VerifyPoolInternal_t *entry = pool;
        // get next pool element
        pool = pool->m_next;
        // free pool element
        vf_free_func( entry, VERIFY_SOURCE_ARGS1 );
    }
    return;
} // vf_delete_pool_func

} // namespace Verifier

/**
 * Function provides final constraint checks for a given class.
 */
Verifier_Result
vf_verify_class_constraints( class_handler klass,      // a given class
                             unsigned verifyAll,       // verification level flag
                             char **message)           // verifier error message
{
    assert( klass );
    assert( message );

    // create context
    vf_Context_t context;
    context.m_class = klass;
    context.m_dump.m_verify = verifyAll ? 1 : 0;

    // verified constraint for a given method
    Verifier_Result result = Verifier::vf_verify_class_constraints( &context );
    *message = context.m_error;

#if _VERIFY_DEBUG
    if( result != VER_OK ) {
        VERIFY_DEBUG( "VerifyError: " << context.m_error );
    }
#endif // _VERIFY_DEBUG

    return result;
} // vf_verify_class_constraints

/**
 * Function releases verify data in class loader.
 */
void
vf_release_verify_data( void *data )
{
    vf_ClassLoaderData_t *cl_data = (vf_ClassLoaderData_t*)data;

    delete cl_data->string;
    vf_delete_pool( cl_data->pool );
    return;
} // vf_release_verify_data
