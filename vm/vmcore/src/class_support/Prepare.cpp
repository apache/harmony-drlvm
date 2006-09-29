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
 * @author Pavel Pervov
 * @version $Revision: 1.1.2.8.4.5 $
 */  


#define LOG_DOMAIN util::CLASS_LOGGER
#include "cxxlog.h"

#include <assert.h>
#include "String_Pool.h"
#include "Class.h"
#include "classloader.h"
#include "nogc.h"
#include "Package.h"

#include "vm_strings.h"
#include "open/vm_util.h"
#include "open/gc.h"
#include "environment.h"
#include "compile.h"
#include "interpreter_exports.h"
#include "interpreter.h"
#include "lil.h"
#include "lil_code_generator.h"

#ifdef _DEBUG
#include "jni.h"
#include "jvmti_direct.h"
#endif

#include "dump.h"

static void class_initialize_if_no_side_effects(Class *clss);


static Boolean should_class_not_be_verified(Class* UNREF clss)
{
    return FALSE;
}



                
// For Java currently, fields are not packed: an int16 occupies a full 32 bit word.
// "do_field_compaction" is true, e.g., for packed ("sequential") layout.
// If the "clss" pointer is non-NULL, the type must be that of an instance field and any
// "padding" bytes for the field are added to the class's total number of field padding bytes.
// If "clss" is NULL, no padding information is gathered.
static unsigned sizeof_field_type(Field *field, bool do_field_compaction, Class *clss)
{
    unsigned sz = 0;
    unsigned pad_bytes = 0;

    const String *descriptor = field->get_descriptor();
    switch (descriptor->bytes[0]) {
    case 'B':
    case 'Z':
        if (do_field_compaction) {
            sz = 1;
        } else {
            sz = 4;
            pad_bytes = 3;
        }
        break;
    case 'C':
    case 'S':
        if (do_field_compaction) {
            sz = 2;
        } else {
            sz = 4;
            pad_bytes = 2;
        }
        break;
    case 'F':
    case 'I':
        sz = 4;
        break;
    case 'L': 
    case '[': 
        {
            // compress static reference fields.
            if (VM_Global_State::loader_env->compress_references) {
                sz = sizeof(COMPRESSED_REFERENCE);
            } else {
                sz = OBJECT_REF_SIZE;
            }
        }
        break;
    case 'D':
    case 'J':
        sz = 8;
        break;
    default:
        ABORT("Invalid type descriptor");
    }

    if (clss != NULL) {
        clss->num_field_padding_bytes += pad_bytes;
    }
    return sz;
} //sizeof_field_type




// Given a clss that is an primitive array return the size
// of an element in an instance of the class.
// Beware the the class is not fully formed at this time.
unsigned sizeof_primitive_array_element(Class *p_class) 
{
    const String *elt_type = p_class->name;
    char elt = elt_type->bytes[1];
    unsigned int sz;
    switch (elt) {
    case 'C':
        sz = 2;
        break;
    case 'B':
        sz = 1;
        break;
    case 'D':
        sz = 8;
        break;
    case 'F':
        sz = 4;
        break;
    case 'I':
        sz = 4;
        break;
    case 'J':
        sz = 8;
        break;
    case 'S':
        sz = 2;
        break;
    case 'Z': // boolean
        sz = 1;
        break;
    case '[':
        sz = 4; // This works for the H32, V64, R32 version.
        assert(OBJECT_REF_SIZE == 4);
        break;
    default:
        ABORT("Unexpected type descriptor");
        return 0;
    }
    return sz;
} //sizeof_primitive_array_element


//
// Is this array class a one dimensional array (vector) with a primitive component type.
//
bool
is_vector_of_primitives(Class* p_class)
{
    // I parse the following character of the class name
    // to see if it is an array of arrays.
    if(p_class->name->bytes[1] == '[') // An array of array
        return false;
    if(p_class->name->bytes[1] == 'L') // An array of objects
        return false;
    if(p_class->is_array_of_primitives == 0) // base type is not primitive
        return false;
    if(p_class->is_array)
        return true;
    ABORT("Should never be called unless p_class is an array");
    return true;
}



void assign_instance_field_offset(Class *clss, Field *field, bool do_field_compaction)
{
    if (!field->is_static() && !field->is_offset_computed()) {
        int sz = sizeof_field_type(field, do_field_compaction, clss);
        int offset = clss->unpadded_instance_data_size;
        // 20020927 We must continue to align fields on natural boundaries: e.g., Java ints on a 4 byte boundary.
        // This is required for IPF and can improve IA32 performance.
        int inc = sz;
        int delta = offset % sz;
        if (delta != 0) {
            int pad_bytes = (sz - delta);
            offset += pad_bytes;
            inc    += pad_bytes;
            clss->num_field_padding_bytes += pad_bytes;
        }
        field->_offset = offset;
        clss->unpadded_instance_data_size += inc;

        char c_type = *(field->get_descriptor()->bytes);
        if ((c_type == '[') || (c_type == 'L')) {
            clss->n_instance_refs += 1;
        }

        field->set_offset_computed(true);
    }
} //assign_instance_field_offset


// "field_ptrs" is an array of pointers to the class's fields.
void assign_offsets_to_instance_fields(Class *clss, Field **field_ptrs, bool do_field_compaction)
{
    int i, sz;
    if (clss->n_fields == 0) {
        return;
    }

    // Try to align the first field on a 4 byte boundary. It might not be if -compact_fields was specified on the command line.
    // See whether there are any short instance fields towards the end of the field array (since that is where -sort_fields puts them)
    // and try to fill in some bytes before the "first" field.
    if (Class::sort_fields && Class::compact_fields) {
        if ((clss->unpadded_instance_data_size % 4) != 0) {
            int delta = (clss->unpadded_instance_data_size % 4);
            int pad_bytes = (4 - delta);   // the number of bytes remaining to fill in
            int last_field = (clss->n_fields - 1);
            while (pad_bytes > 0) {
                // Find a field to allocate
                int field_to_allocate = -1;
                for (i = last_field;  i >= clss->n_static_fields;  i--) {
                    Field* field = field_ptrs[i];
                    if (!field->is_static() && !field->is_offset_computed()) {
                        sz = sizeof_field_type(field, do_field_compaction, clss);
                        if (sz > pad_bytes) {
                            break;  // field is too big
                        }
                        field_to_allocate = i;
                        break;
                    }
                }
                // Now allocate that field, if one was found
                if (field_to_allocate == -1) {
                    // No field could be found to fill in. "pad_bytes" is the number of padding bytes to insert.
                    clss->unpadded_instance_data_size += pad_bytes;
                    clss->num_field_padding_bytes += pad_bytes;
                    break;
                } else {
                    last_field = (i - 1);
                    Field* victim_field = field_ptrs[field_to_allocate];
                    assign_instance_field_offset(clss, victim_field, do_field_compaction);
                    delta = (clss->unpadded_instance_data_size % 4);
                    pad_bytes = ((delta > 0)? (4 - delta) : 0);
                }
            }
        }
    }

    // Place the remaining instance fields.
    for (i = clss->n_static_fields;  i < clss->n_fields;  i++) {
        assign_instance_field_offset(clss, field_ptrs[i], do_field_compaction);
    }
} //assign_offsets_to_instance_fields


// "field_ptrs" is an array of pointers to the class's fields.
void assign_offsets_to_static_fields(Class *clss, Field **field_ptrs, bool do_field_compaction)
{
    for (int i=0; i<clss->n_static_fields; i++) {
        Field* field = field_ptrs[i];
        assert(field->is_static());

        // static (i.e., class) data field
        // is this needed for interface static constants?
        int field_size;
        field_size = sizeof_field_type(field, do_field_compaction, /*clss*/ NULL); // NULL clss since this is a static field

        // Align the static field if necessary.
#ifdef POINTER64
        if (field_size==8) {
#else  // not POINTER64
        if (field->get_descriptor()->bytes[0] == 'D') {
#endif // not POINTER64
            if ((clss->static_data_size%8)!=0) {
                clss->static_data_size += 4;
                assert((clss->static_data_size%8)==0);
            }
        }

        field->_offset = clss->static_data_size;
        field->set_offset_computed(true);
        clss->static_data_size += field_size;
    }
} //assign_offsets_to_static_fields


// Return the field's size before any padding: e.g., 1 for a Byte, 2 for a Char.
static int field_size(Field *field, Class * UNREF clss, bool doing_instance_flds) {
    if (doing_instance_flds && field->is_static()) {
        return 0x7FFFFFFF; // INT_MAX
    }

    int sz;
    sz = sizeof_field_type(field, /*do_field_compaction*/ true, /*clss*/ NULL); // NULL clss since no padding is wanted
    return sz;
} //field_size


static bool is_greater(Field *f1, Field *f2, Class *clss, bool doing_instance_flds) {
    int f1_size = field_size(f1, clss, doing_instance_flds);
    int f2_size = field_size(f2, clss, doing_instance_flds);
    if (f1_size > f2_size) {
        return true;
    }
    if (f1_size < f2_size) {
        return false;
    }
    // f1 and f2 have the same size. If f1 is a reference, and f2 is not, then f1 is greater than f2.
    char f1_kind = f1->get_descriptor()->bytes[0];
    char f2_kind = f2->get_descriptor()->bytes[0];
    bool f1_is_ref = (f1_kind == 'L') || (f1_kind == '[');
    bool f2_is_ref = (f2_kind == 'L') || (f2_kind == '[');
    if (f1_is_ref && !f2_is_ref) {
        return true;
    }
    return false;
} //is_greater


static int partition(Field *A[], int l, int r, Class *clss, bool doing_instance_flds) {
    Field *v = A[(l+r)/2];
    int i = l - 1;
    int j = r + 1;
    while (true) {
        do {j = j - 1;} while (is_greater(v, A[j], clss, doing_instance_flds));
        do {i = i + 1;} while (is_greater(A[i], v, clss, doing_instance_flds));
        if (i < j) {
            Field *tmp;
            tmp = A[i];  A[i] = A[j];  A[j] = tmp;
        } else
            return j;
    }
} //partition


// Sort in decreasing order by size
static void qsort(Field *A[], int l, int r, Class *clss, bool doing_instance_flds) {
    if (l < r) {
        int q = partition(A, l, r, clss, doing_instance_flds);
        qsort(A, l,   q, clss, doing_instance_flds);
        qsort(A, q+1, r, clss, doing_instance_flds);
    }
} //qsort

void assign_offsets_to_class_fields(Class *clss)
{
    assert(clss->state != ST_InstanceSizeComputed);
    bool do_field_compaction = Class::compact_fields;
    bool do_field_sorting    = Class::sort_fields;

    // Create a temporary array of pointers to the class's fields. We do this to support sorting the fields
    // by size if the command line option "-sort_fields" is given, and because elements of the clss->fields array
    // cannot be rearranged without copying their entire Field structure.
    Field **field_ptrs = new Field*[clss->n_fields];
    for (int i=0; i<clss->n_fields; i++) {
        field_ptrs[i] = &(clss->fields[i]);
    }

    if (clss->state != ST_InstanceSizeComputed) {
        // Sort the instance fields by size before allocating their offsets. But not if doing sequential layout!
        // Note: we must sort the instance fields separately from the static fields since for some classes the offsets
        // of statics can only be determined after the offsets of instance fields are found.
        if (do_field_sorting && (clss->n_fields > 0)) {
            qsort(field_ptrs, clss->n_static_fields, (clss->n_fields - 1),
                clss, /*doing_instance_flds:*/ true);
        }

        // We have to assign offsets to a type's instance fields first because
        // a static field of that type needs the instance size if it's a value type.
        assign_offsets_to_instance_fields(clss, field_ptrs, do_field_compaction);

#ifdef DEBUG_FIELD_SORTING
        if (do_field_sorting) {
            printf("\nInstance fields for %s, size=%d\n", clss->name->bytes, clss->unpadded_instance_data_size);
            if (clss->super_class != NULL) {
                printf("  super_class: %s\n", clss->super_class->name->bytes);
            }
            for (int i=0; i<clss->n_fields; i++) {
                Field *field = field_ptrs[i];
                if (!field->is_static()) {
                    const String *typeDesc = field->get_descriptor();
                    int sz = field_size(field, clss, /*doing_instance_flds:*/ true);
                    printf("   %40s  %c %4d %4d\n", field->get_name()->bytes, typeDesc->bytes[0], sz, field->_offset);                    
                    fflush(stdout);
                }
            }
        }
#endif // DEBUG_FIELD_SORTING

        // Set class to ST_InstanceSizeComputed state.
        clss->state = ST_InstanceSizeComputed;
    }

    // Sort the static fields by size before allocating their offsets.
    if (do_field_sorting && (clss->n_static_fields > 0)) {
        qsort(field_ptrs, 0, (clss->n_static_fields - 1),
            clss, /*doing_instance_flds:*/ false);
    }
    assign_offsets_to_static_fields(clss, field_ptrs, do_field_compaction);

#ifdef DEBUG_FIELD_SORTING
    if (do_field_sorting) {
        printf("Static fields for %s, size=%d\n", clss->name->bytes, clss->static_data_size);
        for (int i=0; i<clss->n_fields; i++) {
            Field *field = field_ptrs[i];
            if (field->is_static()) {
                const String *typeDesc = field->get_descriptor();
                int sz = field_size(field, clss, /*doing_instance_flds:*/ false);
                printf("   %40s  %c %4d %4d\n", field->get_name()->bytes, typeDesc->bytes[0], sz, field->_offset);
                fflush(stdout);
            }
        }
    }
#endif // DEBUG_FIELD_SORTING
    delete[] field_ptrs;
} //assign_offsets_to_class_fields


// Required for reflection. See class_prepare STEP20 for further explanation.
bool assign_values_to_class_static_final_fields(Class *clss)
{
    ASSERT_RAISE_AREA;
    bool do_field_compaction = Class::compact_fields;

    for (int i=0; i<clss->n_fields; i++) {
        Field& field = clss->fields[i];
        if (field.is_static()) {
            Java_Type field_type = field.get_java_type();
            void *field_addr = field.get_address();

            // If static field is constant it should be initialized by its constant value,...
            if (field.get_const_value_index()) {
                Const_Java_Value cvalue = field.get_const_value();
                switch(field_type) {
                case '[':
                case 'L':
                {
                    // compress static reference fields.
                    if (cvalue.object == NULL) { //cvalue.string == NULL
                        // We needn't deal with this case, because the object field must be set in static initializer.
                        if (VM_Global_State::loader_env->compress_references) {
                            // initialize the field explicitly.
                            *((COMPRESSED_REFERENCE *)field_addr) = 0;  // i.e., null
                        }
                        break;
                    }
                    static const String* jlstring_desc_string =
                        VM_Global_State::loader_env->
                            string_pool.lookup("Ljava/lang/String;");
                    if (field.get_descriptor() == jlstring_desc_string) {
                        tmn_suspend_disable();
                        // ------------------------------------------------------------vv
                        Java_java_lang_String *str = vm_instantiate_cp_string_resolved(cvalue.string);

                        if (!str) {
                            assert(exn_raised());
                            tmn_suspend_enable();
                            TRACE2("classloader.prepare", "failed instantiate final field : " 
                                << clss->name->bytes << "." << field.get_name()->bytes);
                            return false;
                        }
                        STORE_GLOBAL_REFERENCE((COMPRESSED_REFERENCE *)field_addr, str);
                        // ------------------------------------------------------------^^
                        tmn_suspend_enable();
                    } else {
                        ABORT("Unexpected type descriptor");
                    }
                    break;
                }
                default:
                    int field_size = sizeof_field_type(&field, do_field_compaction, /*clss*/ NULL); // NULL clss since field is static
                    memmove(field_addr, (void*)&cvalue, field_size);
                }

                // ... if field isn't constant it will be initialized by 0, and <clinit> shall initialize it in future.
            } else {
                if ((field_type == '[') || (field_type == 'L')) {
                    if (VM_Global_State::loader_env->compress_references) {
                        // initialize the field explicitly.
                        *((COMPRESSED_REFERENCE *)field_addr) = 0;  // i.e., null
                    }
                }
            }
        } // end if static field
    }
    return true;
} //assign_values_to_class_static_final_fields


void build_gc_class_ref_map(Global_Env* env, Class *clss)
{
    // Add 1 to the size of the information since it includes a zero delimiter.
    // Please note where this get deleted when we unload a class!!!!?
    // It should be done by a call to the gc side of the interface.
    unsigned *local_gc_information = new unsigned[clss->n_instance_refs + 1]; 
    unsigned int current_index = 0;

    // Copy the superclasses gc_information into this refs_offset_map incrementing current_index as needed.
    if (clss->super_class) { // We might be in initialization.
        assert (clss->n_instance_refs >= clss->super_class->n_instance_refs);

        // Ask the GC to fill the local_gc_information with the super class offsets.
        current_index = clss->super_class->n_instance_refs;
    } else {
        assert (current_index == 0);

        //extern bool bootstrapped;
        //// gloss over bootstrap inconsistency
        //if (bootstrapped == true) { 
        if(env->InBootstrap()) { 
            assert(clss->n_instance_refs == 0);
        }
    }
    assert (current_index <= clss->n_instance_refs);
    
    for (int i = 0; i<clss->n_fields; i++) {
        Field& field = clss->fields[i];
        if (field.is_static()) {
            // static (i.e., class) data field
            // Since refs_offset only deals with instance fields 
            // this can be skipped. We don't change current offset for statics.
        } else {
            // instance data field
            //
            char c_type = *(field.get_descriptor()->bytes);
            if ((c_type == '[') || (c_type == 'L')) {
                assert (field.get_offset() != 0); // Only the vtable can have this offset.
                local_gc_information[current_index] = field.get_offset();
                current_index = current_index + 1;
            } 
        }
    }
    assert (current_index == (clss->n_instance_refs));
    local_gc_information[current_index] = 0; 
    // delimit with 0 since and offsetwill never be zero, that is where the vtable is we are OK.
    delete[] local_gc_information;
    // gc_information is not created, and populated and zero deliminted.
    // Pass this to the GC since it responsible for the format of the
    // information saved in clss->gc_information.
} //build_gc_class_ref_map

//
// create_intfc_table
//
Intfc_Table *create_intfc_table(Class* clss, unsigned n_entries)
{
    unsigned size = INTFC_TABLE_OVERHEAD + (n_entries * sizeof(Intfc_Table_Entry));
    Intfc_Table *table = (Intfc_Table*) clss->class_loader->Alloc(size);
    memset(table,0,size);
    return table;
}


void build_class_interface_table_descriptors(Class *clss)
{
    // Compute the number of methods that are in the interface part of the vtable. Also, create the array of
    // interfaces (clss->intfc_table_descriptors[]) this class directly or indirectly implements. Note that 
    // _n_intfc_table_entries was initialized earlier. This is an upperbound because we eliminate duplicate 
    // entries in the table below.
    unsigned i, k;
    for (i = 0;  i < clss->n_superinterfaces;  i++) {
        Class *intfc = clss->superinterfaces[i].clss;
        clss->n_intfc_table_entries += intfc->n_intfc_table_entries;
    }
    // allocate the class's intfc_table_descriptors[] array
    if (clss->n_intfc_table_entries != 0) {
        clss->intfc_table_descriptors = (Class **) clss->class_loader->Alloc(clss->n_intfc_table_entries * sizeof(Class *));
    } else {
        clss->intfc_table_descriptors = NULL;
    }
    for (k = 0;  k < clss->n_intfc_table_entries;  k++) {
        clss->intfc_table_descriptors[k] = NULL;
    }
    // fill in intfc_table_descriptors with the descriptors from the superclass and the superinterfaces
    unsigned intfc_table_entry = 0;
    if (clss->super_class != NULL) {
        for (unsigned i = 0; i < clss->super_class->n_intfc_table_entries; i++) {
            clss->intfc_table_descriptors[intfc_table_entry] = clss->super_class->intfc_table_descriptors[i];
            intfc_table_entry++;
        }
    }
    for (k = 0;  k < clss->n_superinterfaces;  k++) {
        Class *intfc = clss->superinterfaces[k].clss;
        for (i = 0;  i < intfc->n_intfc_table_entries;  i++) {
            clss->intfc_table_descriptors[intfc_table_entry] = intfc->intfc_table_descriptors[i];
            intfc_table_entry++;
        }
    }
    // if this class is an interface, add it to the interface table
    if (class_is_interface(clss)) {
        clss->intfc_table_descriptors[intfc_table_entry] = clss;
        intfc_table_entry++;
    }
    // sort the interfaces in intfc_table_descriptors, eliminating duplicate entries
    unsigned last_min_id = 0;
    for (i = 0;  i < clss->n_intfc_table_entries;  i++) {
        //
        // select the next interface C with smallest id and insert 
        // into i'th position; delete entry of C if C is the same
        // as i-1'th entry
        //
        Class *intfc = clss->intfc_table_descriptors[i];
        unsigned min_index = i;         // index of intfc with min id
        unsigned min_id = intfc->id;    // id of intfc with min id
        for (unsigned k = i+1;  k < clss->n_intfc_table_entries;  k++) {
            unsigned id = clss->intfc_table_descriptors[k]->id;
            if (id < min_id) {
                // new min
                min_index = k;
                min_id = id;
                continue;
            }
        }
        // if the id of the min is the same as the i-1'th entry's id, then we have a duplicate
        if (min_id == last_min_id) {
            // duplicate found -- insert the last entry in place of the duplicate's entry
            clss->intfc_table_descriptors[min_index] = clss->intfc_table_descriptors[clss->n_intfc_table_entries-1];
            clss->n_intfc_table_entries--;
            continue;
        }
        last_min_id = min_id;
        if (min_index == i) {
            continue;
        }
        // swap i'th entry with min entry
        Class *min_intfc = clss->intfc_table_descriptors[min_index];
        clss->intfc_table_descriptors[min_index] = clss->intfc_table_descriptors[i];
        clss->intfc_table_descriptors[i] = min_intfc;
    }
} //build_class_interface_table_descriptors


// Returns the method matching the Signature "sig" that is implemented directly or indirectly by "clss", or NULL if not found.
Method *find_method_impl_by_class(Class *clss, const String* name, const String* desc)
{
    assert(clss);
    Method *m = NULL;
    for(;  ((clss != NULL) && (m == NULL));  clss = clss->super_class) {
        m = class_lookup_method(clss, name, desc);
    }
    return m;
} //find_method_impl_by_class

// Add the new fake methods to class.
void inline add_new_fake_method( Class *clss, Class *example, unsigned *next)
{
    for (unsigned i = 0;  i < clss->n_superinterfaces;  i++) {
        Class *intf_clss = clss->superinterfaces[i].clss;
        add_new_fake_method(intf_clss, example, next);
        for (unsigned k = 0;  k < intf_clss->n_methods;  k++) {
            Method &intf_method = intf_clss->methods[k];
            if (intf_method.is_clinit()) {
                continue;
            }
            // See if the interface method "intf_method" is implemented by clss. 
            const String* intf_name = intf_method.get_name();
            const String* intf_desc = intf_method.get_descriptor();
            Method *impl_method = find_method_impl_by_class(example, intf_name, intf_desc);

            if (impl_method == NULL) {
#ifdef DEBUG_FAKE_METHOD_ADDITION
                printf("**    Adding fake method to class %s for unimplemented method %s of interface %s.\n", 
                    example->name->bytes, intf_name->bytes, intf_clss->name->bytes);
#endif
                Method *fake_method = &(example->methods[(*next)]);
                (*next)++;

                fake_method->_class = example;
                // 20021119 Is this the correct signature?
                fake_method->_name = (String*)intf_name; 
                fake_method->_descriptor = (String*)intf_desc; 
                fake_method->_state = Method::ST_NotCompiled;
                fake_method->_access_flags = (ACC_PUBLIC | ACC_ABSTRACT);
                // Setting its "_intf_method_for_fake_method" field marks the method as being fake.
                fake_method->_intf_method_for_fake_method = &intf_method;
                // The rest of the method's fields were zero'd above 
            }
        }
    }
    return;
} // add_new_fake_method

// Count the fake methods.
// These are the interface methods that are not implemented by the class.
unsigned inline count_fake_interface_method( Class *clss, Class *example ) 
{
    unsigned count = 0;
    for (unsigned i = 0;  i < clss->n_superinterfaces;  i++) {
        Class *intf_clss = clss->superinterfaces[i].clss;
        count += count_fake_interface_method(intf_clss, example);
        for (unsigned k = 0;  k < intf_clss->n_methods;  k++) {
            Method &intf_method = intf_clss->methods[k];
            if (intf_method.is_clinit()) {
                continue;
            }
            // See if the interface method "intf_method" is implemented by clss. 
            const String *intf_name = intf_method.get_name();
            const String *intf_desc = intf_method.get_descriptor();
            Method *impl_method = find_method_impl_by_class(example, intf_name, intf_desc);
            if (impl_method == NULL) {
                count++;
            }
        }
    }
    return count;
} // count_fake_interface_method

// Add any required "fake" methods to a class. These are interface methods inherited by an abstract class that
// are not implemented by that class or any superclass. Such methods will never be called, but they are added
// so they have the correct offset in the virtual method part of the vtable (i.e., the offset of the "real" method
// in the vtable for a concrete class). 
void add_any_fake_methods(Class *clss)
{
    assert(class_is_abstract(clss));

    // First, count the fake methods. These are the interface methods that are not implemented by the class.
    unsigned num_fake_methods = count_fake_interface_method(clss, clss);

    // If no fake methods are needed, just return.
    if (num_fake_methods == 0) {
        return;
    }

    // Reallocate the class's method storage block, creating a new method area with room for the fake methods.
#ifdef DEBUG_FAKE_METHOD_ADDITION
    printf("\n** %u fake methods needed for class %s \n", num_fake_methods, clss->name->bytes);
#endif
    unsigned new_num_methods = (clss->n_methods + num_fake_methods);
    Method *new_meth_array = new Method[new_num_methods];
    if (clss->methods != NULL) {
        memcpy(new_meth_array, clss->methods, (clss->n_methods * sizeof(Method)));
    }
    unsigned next_fake_method_idx = clss->n_methods;
    memset(&(new_meth_array[next_fake_method_idx]), 0, (num_fake_methods * sizeof(Method)));

    // Regenerate the existing compile-me/delegate/unboxer stubs and redirect the class's static_initializer and default_constructor fields
    // since they refer to the old method block. We  regenerate the stubs because any code to update the addresses in
    // the existing stubs would be very fragile, fake methods very rarely need to be added, and the stubs are small.
    for (unsigned i = 0;  i < clss->n_methods;  i++) {   // Note that this is still the old number of methods.
        Method *m      = &clss->methods[i];
        Method *m_copy = &new_meth_array[i];
        if (m_copy->get_method_sig()) {
            m_copy->get_method_sig()->method = m_copy;
        }
        if (m->is_clinit()) {
            clss->static_initializer = m_copy;
        }
        if (m->get_name() == VM_Global_State::loader_env->Init_String && m->get_descriptor() == VM_Global_State::loader_env->VoidVoidDescriptor_String) {
            clss->default_constructor = m_copy;
        }
    }

    // Free the old storage area for the class's methods, and have the class point to the new method storage area.
    if (clss->methods != NULL) {
        delete [] clss->methods;
    }
    clss->methods   = new_meth_array;

    // Add the new fake methods.
    add_new_fake_method( clss, clss, &next_fake_method_idx );
    // some methods could be counted several times as "fake" methods (count_fake_interface_method())
    // however they are added only once. So we adjust the number of added methods.
    assert(next_fake_method_idx <= new_num_methods);
    clss->n_methods = (uint16)next_fake_method_idx;
} //add_any_fake_methods

void assign_offsets_to_class_methods(Class *clss)
{
    // At this point we have an array of the interfaces implemented by this class. We also know the number of 
    // methods in the interface part of the vtable. We now need to find the number of virtual methods that are in 
    // the virtual method part of the vtable, before we can allocate _vtable and _vtable_descriptors.
    
    // First, if the class is abstract, add any required "fake" methods: these are abstract methods inherited 
    // by an abstract class that are not implemented by that class or any superclass. 
    if (class_is_abstract(clss) && !class_is_interface(clss)) {
        add_any_fake_methods(clss);
    }

    Method **super_vtable_descriptors = NULL;
    unsigned n_super_virtual_method_entries = 0;
    if (clss->super_class != NULL) {
        super_vtable_descriptors       = clss->super_class->vtable_descriptors;
        n_super_virtual_method_entries = clss->super_class->n_virtual_method_entries;
    }
    // Offset of the next entry in the vtable to use.
#ifdef POINTER64
    unsigned next_vtable_offset = clss->n_virtual_method_entries << 3;  
#else
    unsigned next_vtable_offset = clss->n_virtual_method_entries << 2;  
#endif
    if (!class_is_interface(clss)) {
        // Classes have an additional overhead for the class pointer and interface table.
        next_vtable_offset += VTABLE_OVERHEAD;
    }
    unsigned i, j;
    for (i = 0;  i < clss->n_methods;  i++) {
        Method& method = clss->methods[i];
        
        // check if the method hasn't already been initialized or even compiled
        assert(method.get_code_addr() == NULL);
        // initialize method's code address
        method.set_code_addr((char*)compile_gen_compile_me(&method));

        if (method.is_static()) {
            // A static method
            if (method.is_clinit()) {
                method._offset = 0;
                method._index = 0;
            } else {
                // the class better not be an interface!
                // To do : make sure this is not an interface
                assert(!class_is_interface(clss));
                method._offset = clss->static_method_size;
#ifdef POINTER64
                clss->static_method_size += 8;
#else
                clss->static_method_size += 4;
#endif
            }
        } else {
            // A virtual method. Look it up in virtual method tables of the super classes; if not found, then assign a new offset.
            
            // Ignore initializers.
            if (method.is_init()) {
                continue;
            }

#ifdef REMOVE_FINALIZE_FROM_VTABLES
            // skip over finalize() method, but remember it
            if (method.is_finalize()) {
                clss->finalize_method = &method;
                continue;
            }
#endif
            unsigned off   = 0;
            unsigned index = 0;
            if (super_vtable_descriptors != NULL) {
                bool same_runtime_package_as_super =
                    (clss->package == clss->super_class->package);

                const String *name = method.get_name();
                const String *desc = method.get_descriptor();
                for (j = 0;  j < n_super_virtual_method_entries;  j++) {
                    Method *m = super_vtable_descriptors[j];
                    if (name == m->get_name() && desc == m->get_descriptor()) {
                        if(m->is_final()) {
                            if(m->is_private()
                                || (m->is_package_private() 
                                    && m->get_class()->package != method.get_class()->package))
                            {
                                // We allow to override private final and
                                // default (package private) final methods
                                // from superclasses since they are not accessible
                                // from descendants.
                                // Note: for package private methods this statement
                                // is true only for classes from different packages
                            } else {
                                REPORT_FAILED_CLASS_CLASS(clss->class_loader, clss,
                                    "java/lang/VerifyError",
                                    "An attempt is made to override final method "
                                    << m->get_class()->name->bytes << "."
                                    << m->get_name()->bytes << m->get_descriptor()->bytes);
                                return;
                            }
                        }
                        // method doesn't override m if method has package access
                        // and is in a different runtime package than m.
                        if(same_runtime_package_as_super
                            || m->is_public()
                            || m->is_protected()
                            || m->is_private())
                        {
                            off   = m->get_offset();
                            index = m->get_index();
                            // mark superclass' method "m" as being overwridden
                            m->method_was_overridden();

                            // notify interested JITs that "m" was overwridden by "method"
                            m->do_jit_overridden_method_callbacks(&method);
                        }
                        break;
                    }
                }
            }
            if (off == 0 || class_is_interface(clss)) {
                // Didn't find a matching signature in any super class; add a new entry to this class' vtable.
                off   = next_vtable_offset;
                index = clss->n_virtual_method_entries;
#ifdef POINTER64
                next_vtable_offset += 8;  
#else
                next_vtable_offset += 4;  
#endif
                clss->n_virtual_method_entries++;
            }
            method._offset = off;
            method._index = index;
        }
    }

    // Figure out which methods don't do anything
    for (i = 0;  i < clss->n_methods;  i++) {
        Method& method = clss->methods[i];
        method._set_nop();
    }

    // Decide whether it is possible to allocate instances of this class using a fast inline sequence containing
    // no calls to other routines. This means no calls to raise exceptions or to invoke constructors. It will also be
    // necessary that the allocation itself can be done without needing to call a separate function.
    bool is_not_instantiable = (     // if true, will raise java_lang_InstantiationException
        (clss->default_constructor == NULL) || 
        (clss->is_primitive || clss->is_array || class_is_interface(clss) || class_is_abstract(clss)) ||
        (clss == VM_Global_State::loader_env->Void_Class));
    if (!is_not_instantiable && clss->default_constructor->is_nop()) {
        clss->is_fast_allocation_possible = TRUE;
    }
} //assign_offsets_to_class_methods


bool initialize_static_fields_for_interface(Class *clss)
{
    ASSERT_RAISE_AREA;
    tmn_suspend_disable();
    // Initialize static fields
    clss->state = ST_Prepared;
    unsigned i;
    for (i=0; i<clss->n_fields; i++) {
        Field& field = clss->fields[i];
        if (field.is_static() && field.get_const_value_index()) {
            char *field_addr = ((char *)clss->static_data_block) + field.get_offset();
            Const_Java_Value field_const_value = field.get_const_value();
            switch(field.get_java_type()) {
            case JAVA_TYPE_INT:
                *((int32 *)field_addr) = field_const_value.i;
                break;
            case JAVA_TYPE_SHORT:
            case JAVA_TYPE_CHAR:
                *((int16 *)field_addr) = (int16)field_const_value.i;
                break;
            case JAVA_TYPE_BYTE:
            case JAVA_TYPE_BOOLEAN:
                *((int8 *)field_addr) = (int8)field_const_value.i;
                break;
            case JAVA_TYPE_LONG:
                *(((int32 *)field_addr))     = field_const_value.l.lo_bytes;
                *(((int32 *)field_addr) + 1) = field_const_value.l.hi_bytes;
                break;
            case JAVA_TYPE_DOUBLE:
                *(((int32 *)field_addr))     = field_const_value.l.lo_bytes;
                *(((int32 *)field_addr) + 1) = field_const_value.l.hi_bytes;
                break;
            case JAVA_TYPE_FLOAT:
                *((float *)field_addr) = field_const_value.f;
                break;
            case JAVA_TYPE_CLASS:
            {
                // compress static reference fields.
                // It must be a String
                assert(strcmp(field.get_descriptor()->bytes, "Ljava/lang/String;") == 0);
                Java_java_lang_String *str
                        = vm_instantiate_cp_string_resolved(field_const_value.string);

                if (!str) {
                    assert(exn_raised());
                    tmn_suspend_enable();
                    TRACE2("classloader.prepare", "failed instantiate final field : " 
                        << clss->name->bytes << "." << field.get_name()->bytes);
                    return false;
                }
                STORE_GLOBAL_REFERENCE((COMPRESSED_REFERENCE *)field_addr, str);
                break;
            }
            default:
                // This should never happen.
                ABORT("Unexpected java type");
                break;
            }
        }
    }
    clss->n_virtual_method_entries = 0; // interfaces don't have vtables
    for (i=0; i<clss->n_methods; i++) {
        Method *method = &clss->methods[i];
        if (method->is_clinit()) {
            assert(clss->static_initializer == method);
        }
    }
    tmn_suspend_enable();
    TRACE2("classloader.prepare", "interface " << clss->name->bytes << " prepared");
    class_initialize_if_no_side_effects(clss);

    return true;
} //initialize_static_fields_for_interface


void populate_vtable_descriptors_table_and_override_methods(Class *clss)    
{
    // Populate _vtable_descriptors first with _n_virtual_method_entries from super class
    if (clss->super_class != NULL) {
        for (unsigned i = 0; i < clss->super_class->n_virtual_method_entries; i++) {
            clss->vtable_descriptors[i] = clss->super_class->vtable_descriptors[i];
        }
    }
    // NOW OVERRIDE with this class' methods
    unsigned i;
    for (i = 0;  i < clss->n_methods;  i++) {
        Method *method = &clss->methods[i];
        if (method->is_clinit()) {
            assert(clss->static_initializer == method);
        } 
        if(method->is_static()
            || method->is_init()
#ifdef REMOVE_FINALIZE_FROM_VTABLES
            || method->is_finalize()
#endif
            )
            continue;
        clss->vtable_descriptors[method->get_index()] = method;
    }
    // finally, the interface methods
    unsigned index = clss->n_virtual_method_entries;
    for (i = 0;  i < clss->n_intfc_table_entries;  i++) {
        Class *intfc = clss->intfc_table_descriptors[i];
        for (unsigned k = 0;  k < intfc->n_methods;  k++) {
            if (intfc->methods[k].is_clinit()) {
                continue;
            }

            // Find method with matching signature and replace
            const String *sig_name = intfc->methods[k].get_name();
            const String *sig_desc = intfc->methods[k].get_descriptor();
            Method *method = NULL;
            for (unsigned j = 0; j < clss->n_virtual_method_entries; j++) {
                if (clss->vtable_descriptors[j]->get_name() == sig_name && clss->vtable_descriptors[j]->get_descriptor() == sig_desc) {
                    method = clss->vtable_descriptors[j];
                    break;      // a match!
                }

            }
            if (method == NULL && !class_is_abstract(clss)) {
                // wgs: I think we should comment out this assert, because there're many cases VM/Classpath 
                // will run apps built on previous JDK version, and without implementations of newly added methods 
                // for specific interfaces, we allow them to continue to run
                TRACE2("classloader.prepare", "No implementation in class " << clss->name->bytes
                    << " for method " << sig_name->bytes << " of interface " << intfc->name->bytes
                    << ". \n\nCheck whether you used another set of class library.\n");
            }
            clss->vtable_descriptors[index] = method;
            index++;
        }
    }
} //populate_vtable_descriptors_table_and_override_methods


void point_class_vtable_entries_to_stubs(Class *clss)
{

    for (unsigned i = 0; i < clss->n_virtual_method_entries; i++) {
        assert(clss->vtable_descriptors[i]);
        Method& method = *(clss->vtable_descriptors[i]);
        assert(!method.is_static());
        //if (!method.is_static() && !is_ignored_method) {
        if(!method.is_static()) {
            unsigned meth_idx = method.get_index();
            // 2003-03-17: Make this assert independent of POINTER64.  There are already several
            // assumptions in the code that the width of each method pointer is the same as void* .
            assert((method.get_offset() - VTABLE_OVERHEAD) / sizeof(void *) == method.get_index());  
            clss->vtable->methods[meth_idx] =
                (unsigned char *)method.get_code_addr();
            method.add_vtable_patch(&(clss->vtable->methods[meth_idx]));
            assert(method.is_fake_method() || interpreter_enabled() || method.get_code_addr());
        }
    }
}

extern bool dump_stubs;

// It's a rutime helper. So should be named as rth_prepare_throw_abstract_method_error
void prepare_throw_abstract_method_error(Class_Handle clss, Method_Handle method)
{
    char* buf = (char*)STD_ALLOCA(clss->name->len + method->get_name()->len
        + method->get_descriptor()->len + 2); // . + \0
    sprintf(buf, "%s.%s%s", clss->name->bytes,
        method->get_name()->bytes, method->get_descriptor()->bytes);
    tmn_suspend_enable();

    // throw exception here because it's a helper
    exn_throw_by_name("java/lang/AbstractMethodError", buf);
    tmn_suspend_disable();
}

NativeCodePtr prepare_gen_throw_abstract_method_error(Class_Handle clss, Method_Handle method)
{
    NativeCodePtr addr = NULL;
    void (*p_throw_ame)(Class_Handle, Method_Handle) =
        prepare_throw_abstract_method_error;
    LilCodeStub* cs = lil_parse_code_stub("entry 0:rth::void;"
        "push_m2n 0, 0;"
        "m2n_save_all;"
        "out platform:pint,pint:void;"
        "o0=%0i:pint;"
        "o1=%1i:pint;"
        "call.noret %2i;",
        clss, method, p_throw_ame);
    assert(cs && lil_is_valid(cs));
    addr = LilCodeGenerator::get_platform()->compile(cs);
    
    DUMP_STUB(addr, "prepare_throw_abstract_method_error", lil_cs_get_code_size(cs));

    lil_free_code_stub(cs);

    return addr;
}

// It's a rutime helper. So should be named as rth_prepare_throw_illegal_access_error
void prepare_throw_illegal_access_error(Class_Handle to, Method_Handle from)
{
    char* buf = (char*)STD_ALLOCA(from->get_class()->name->len
        + to->name->len + from->get_name()->len
        + from->get_descriptor()->len + 12); // from + to + . + \0
    sprintf(buf, "from %s to %s.%s%s", from->get_class()->name->bytes,
        to->name->bytes,
        from->get_name()->bytes, from->get_descriptor()->bytes);
    tmn_suspend_enable();

    // throw exception here because it's a helper
    exn_throw_by_name("java/lang/IllegalAccessError", buf);
    tmn_suspend_disable();
}

NativeCodePtr prepare_gen_throw_illegal_access_error(Class_Handle to, Method_Handle from)
{
    NativeCodePtr addr = NULL;
    void (*p_throw_iae)(Class_Handle, Method_Handle) =
        prepare_throw_illegal_access_error;
    LilCodeStub* cs = lil_parse_code_stub("entry 0:rth::void;"
        "push_m2n 0, 0;"
        "m2n_save_all;"
        "out platform:pint,pint:void;"
        "o0=%0i:pint;"
        "o1=%1i:pint;"
        "call.noret %2i;",
        to, from, p_throw_iae);
    assert(cs && lil_is_valid(cs));
    addr = LilCodeGenerator::get_platform()->compile(cs);
    
    DUMP_STUB(addr, "rth_throw_linking_exception", lil_cs_get_code_size(cs));

    lil_free_code_stub(cs);

    return addr;
}

Intfc_Table *create_populate_class_interface_table(Class *clss)     
{
    Intfc_Table *intfc_table;
    if (clss->n_intfc_table_entries != 0) {
        unsigned vtable_offset = clss->n_virtual_method_entries;
        // shouldn't it be called vtable_index?
        intfc_table = create_intfc_table(clss, clss->n_intfc_table_entries);
        unsigned i;
        for (i = 0; i < clss->n_intfc_table_entries; i++) {
            Class *intfc = clss->intfc_table_descriptors[i];
            intfc_table->entry[i].intfc_id = intfc->id;
            intfc_table->entry[i].table = &clss->vtable->methods[vtable_offset];
            vtable_offset += intfc->n_methods;
            if(intfc->static_initializer) {
                // Don't count static initializers of interfaces.
                vtable_offset--;
            }
        }
        // Set the vtable entries to point to the code address.
        unsigned meth_idx = clss->n_virtual_method_entries;
        for (i = 0; i < clss->n_intfc_table_entries; i++) {
            Class *intfc = clss->intfc_table_descriptors[i];
            for (unsigned k = 0; k < intfc->n_methods; k++) {
                if (intfc->methods[k].is_clinit()) {
                    continue;
                }
                Method *method = clss->vtable_descriptors[meth_idx];
                if(method == NULL || method->is_abstract()) {
                    TRACE2("classloader.prepare.ame", "Inserting Throw_AbstractMethodError stub for method\n\t"
                        << clss->name->bytes << "."
                        << intfc->methods[k].get_name()->bytes << intfc->methods[k].get_descriptor()->bytes);
                    clss->vtable->methods[meth_idx] =
                        (unsigned char*)prepare_gen_throw_abstract_method_error(clss, &intfc->methods[k]);
                } else if(method->is_public()) {
                    clss->vtable->methods[meth_idx] =
                        (unsigned char *)method->get_code_addr();
                    method->add_vtable_patch(&(clss->vtable->methods[meth_idx]));
                } else {
                    TRACE2("classloader.prepare.iae", "Inserting Throw_IllegalAccessError stub for method\n\t"
                        << method->get_class()->name->bytes << "."
                        << method->get_name()->bytes << method->get_descriptor()->bytes);
                    clss->vtable->methods[meth_idx] =
                        (unsigned char*)prepare_gen_throw_illegal_access_error(intfc, method);
                }
                meth_idx++;
            }
        }
    } else {
        intfc_table = NULL;
    }
    return intfc_table;
} //create_populate_class_interface_table


void initialize_interface_class_data(Class *clss)
{
    // this is an interface
    clss->instance_data_size = 0;   // no instance data
    clss->unpadded_instance_data_size = 0;
    clss->allocated_size = 0;
    clss->n_instance_refs      = 0;
    clss->n_virtual_method_entries = 0; // thus no virtual method entries
    clss->n_intfc_table_entries = 1;    // need table entry for this interface
} //initialize_interface_class_data


void initialize_java_lang_object_class(Class *clss)
{
    // java.lang.Object -- Java ROOT.
    clss->instance_data_size = 0; // set below use the unpadded_instace_data_size.
    clss->allocated_size = 0;     // set below.
    clss->unpadded_instance_data_size = /*sizeof(ManagedObject)*/(unsigned)ManagedObject::get_size();
    clss->n_instance_refs    = 0;
    clss->n_virtual_method_entries = clss->n_intfc_table_entries = 0;
} //initialize_java_lang_object_class


static void initialize_regular_class_data(Global_Env* env, Class *clss)
{
    clss->instance_data_size = 0; // set below.
    clss->allocated_size = 0;     // set below.
    // Roll over instance size, instance refs, static fields #, and num_field_padding_bytes from the super class.
    clss->unpadded_instance_data_size = clss->super_class->unpadded_instance_data_size;
    if (clss->name == env->JavaLangClass_String) {
        clss->unpadded_instance_data_size = 
            ( (/*sizeof(ManagedObject)*/(unsigned)ManagedObject::get_size() + (GC_OBJECT_ALIGNMENT - 1)) / GC_OBJECT_ALIGNMENT) * GC_OBJECT_ALIGNMENT;
    }
    clss->n_instance_refs         = clss->super_class->n_instance_refs;
    clss->num_field_padding_bytes = clss->super_class->num_field_padding_bytes;
    // Roll over all virtual methods and interface methods of super class.
    clss->n_virtual_method_entries = clss->super_class->n_virtual_method_entries;
    clss->n_intfc_table_entries = clss->super_class->n_intfc_table_entries;
} //initialize_regular_class_data


//
// prepares a class:
//  (1) assign offsets
//      - offset of instance data fields
//      - virtual methods in vtable
//      - static data fields in static data block
//      - static methods in static method block
//
//  (2) create class vtable
//  (3) create static field block
//  (4) create static method block
//
//
//

bool class_prepare(Global_Env* env, Class *clss)
{
    ASSERT_RAISE_AREA;
    // fast path
    switch(clss->state)
    {
    case ST_Prepared:
    case ST_Initializing:
    case ST_Initialized:
    case ST_Error:
        return true;
    default:
        break;
    }

    LMAutoUnlock autoUnlocker(clss->m_lock);

    //
    //
    // STEP 1 ::: SIMPLY RETURN IF already prepared, initialized, or currently initializing.
    //
    //
    switch(clss->state)
    {
    case ST_Prepared:
    case ST_Initializing:
    case ST_Initialized:
    case ST_Error:
        return true;
    default:
        break;
    }

    TRACE2("classloader.prepare", "BEGIN class prepare, class name = " << clss->name->bytes);
    assert(clss->is_verified);

    //
    //
    // STEP 2 ::: PREPARE SUPER-INTERFACES
    //
    //
    unsigned i;
    for (i=0; i<clss->n_superinterfaces; i++) {
        if(!class_is_interface(clss->superinterfaces[i].clss)) {
            REPORT_FAILED_CLASS_CLASS(clss->class_loader, clss,
                "java/lang/IncompatibleClassChangeError",
                clss->name->bytes << ": "
                << clss->superinterfaces[i].clss->name->bytes << " is not an interface");
            return false;
        }
        if(!class_prepare(env, clss->superinterfaces[i].clss)) {
            REPORT_FAILED_CLASS_CLASS(clss->class_loader, clss,
                VM_Global_State::loader_env->JavaLangNoClassDefFoundError_String->bytes,
                clss->name->bytes << ": error preparing superinterface "
                << clss->superinterfaces[i].clss->name->bytes);
            return false;
        }
    }

    //
    //
    // STEP 3 ::: PREPARE SUPERCLASS if needed; simply initialize if interface.
    //
    //
    if (class_is_interface(clss)) {
        initialize_interface_class_data(clss);
    } else if (clss->super_class != NULL) {
        // Regular class with super-class.
        if(!class_prepare(env, clss->super_class)) {
            REPORT_FAILED_CLASS_CLASS(clss->class_loader, clss,
                VM_Global_State::loader_env->JavaLangNoClassDefFoundError_String->bytes,
                clss->name->bytes << ": error preparing superclass "
                << clss->super_class->name->bytes);
            return false;
        }
        // CLASS_VTABLE_REWORK - these will eventually be moved into the vtable but we don't have one yet.
        // Before we start adding properties make sure they are clear.
        assert(clss->class_properties == 0);
        if(clss->super_class->has_finalizer) {
            clss->has_finalizer = 1;
        }
        initialize_regular_class_data(env, clss);
    } else {
        initialize_java_lang_object_class(clss);
    }
    clss->static_data_size = 0;
    clss->static_method_size = 0;
    //
    //
    // STEP 4 :::: ASSIGN OFFSETS to the class and instance data FIELDS.
    //             This SETs class to ST_InstanceSizeComputed state.
    //   
    //
    assign_offsets_to_class_fields(clss);
    assert(clss->state == ST_InstanceSizeComputed);
    //
    //
    // STEP 5 :::: Build GC REFERENCE OFFSET MAP
    //
    //
    build_gc_class_ref_map(env, clss);
    //
    //
    // STEP 6 :::: Calculate # of INTERFACES METHODS and build interface table DESCRIPTORS for C
    //
    //
    build_class_interface_table_descriptors(clss);
    //
    //
    // STEP 7 :::: ASSIGN OFFSETS to the class and virtual METHODS
    //
    //
    assign_offsets_to_class_methods(clss);
    if(clss->state == ST_Error) {
        return false;
    }
    //
    //
    // STEP 8 :::: Create the static field and method blocks
    //
    //
    clss->static_data_block = (char *) clss->class_loader->Alloc(clss->static_data_size);
    memset(clss->static_data_block, 0, clss->static_data_size);

#ifdef VM_STATS
    // 20020923 Total number of allocations and total number of bytes for class-related data structures.
    // This includes any rounding added to make each item aligned (current alignment is to the next 16 byte boundary).
    unsigned num_bytes = (clss->static_data_size + 15) & ~15;
    Class::num_statics_allocations++;
    if (clss->static_data_size > 0) {
        Class::num_nonempty_statics_allocations++;
    }
    Class::total_statics_bytes += num_bytes;
#endif
    assert(clss->static_data_block); 
    assert(( ((POINTER_SIZE_INT)(clss->static_data_block)) % 8) == 0);    // block must be on a 8 byte boundary
    memset(clss->static_data_block, 0, clss->static_data_size);

    clss->static_method_block = (unsigned char**) new char[clss->static_method_size];    
    memset(clss->static_method_block, 0, clss->static_method_size);
    //
    //
    // STEP 9 :::: For INTERFACES intialize static fields and return.
    //
    //
    if (class_is_interface(clss)) {
        bool init_fields = initialize_static_fields_for_interface(clss);
        //if((env->java_io_Serializable_Class != NULL && clss->name != env->java_io_Serializable_Class->name))
        if(!env->InBootstrap())
        {
            autoUnlocker.ForceUnlock();
            assert(hythread_is_suspend_enabled());
            if (init_fields) {
                jvmti_send_class_prepare_event(clss);
            }
        }
        // DONE for interfaces
        TRACE2("classloader.prepare", "END class prepare, class name = " << clss->name->bytes);
        return init_fields;
    }
    //
    //
    // STEP 10 :::: COMPUTE number of interface method entries.
    //
    //
    for (i = 0; i < clss->n_intfc_table_entries; i++) {
        Class *intfc = clss->intfc_table_descriptors[i];
        clss->n_intfc_method_entries += intfc->n_methods;
        if (intfc->static_initializer) {
            // Don't count static initializers of interfaces.
            clss->n_intfc_method_entries--;
        }
    }
    //
    //
    // STEP 11 :::: ALLOCATE the Vtable descriptors array 
    //
    //
    unsigned n_vtable_entries = clss->n_virtual_method_entries + clss->n_intfc_method_entries;
    if (n_vtable_entries != 0) {
        clss->vtable_descriptors = new Method*[n_vtable_entries];
        // ppervov: FIXME: should throw OOME
    } else {
        clss->vtable_descriptors = NULL;
    }
    //
    //
    // STEP 12 :::: POPULATE with interface descriptors and virtual method descriptors.
    //              Also, OVERRIDE superclass' methods with those of this one's
    //
    populate_vtable_descriptors_table_and_override_methods(clss);   
    //
    //
    // STEP 13 :::: CREATE VTABLE and set the Vtable entries to point to the 
    //              code address (a stub or jitted code) 
    //
    //
    clss->vtable = create_vtable(clss, n_vtable_entries);
    for (i = 0; i < n_vtable_entries; i++) {
        // need to populate with pointers to stubs or compiled code
        clss->vtable->methods[i] = NULL;    // for now
    }
    if (vm_vtable_pointers_are_compressed())
    {
        clss->allocation_handle = (Allocation_Handle) ((POINTER_SIZE_INT)clss->vtable - vm_get_vtable_base());
    }
    else
    {
        clss->allocation_handle = (Allocation_Handle) clss->vtable;
    }
    clss->vtable->clss = clss;

    // Set the vtable entries to point to the code address (a stub or jitted code)
    point_class_vtable_entries_to_stubs(clss);

    //
    //
    // STEP 14 :::: CREATE and POPULATE the CLASS INTERFACE TABLE
    //
    //
    clss->vtable->intfc_table = create_populate_class_interface_table(clss);        
    
    //
    //
    // STEP 15 :::: HANDLE JAVA CLASSCLASS separately   
    //
    //

    // Make sure on one hasn't prematurely set these fields since all calculations
    // up to this point should be based on clss->unpadded_instance_data_size.
    assert (clss->instance_data_size == 0);
    assert (clss->allocated_size == 0);
    // Add any needed padding including the OBJECT_HEADER which is used to hold
    // things like gc forwarding pointers, mark bits, hashes and locks..
    clss->allocated_size = 
        (((clss->unpadded_instance_data_size + (GC_OBJECT_ALIGNMENT - 1)) 
          / GC_OBJECT_ALIGNMENT) * GC_OBJECT_ALIGNMENT) + OBJECT_HEADER_SIZE;
    // Move the size to the vtable.
    clss->vtable->allocated_size = clss->allocated_size;
    clss->instance_data_size = clss->allocated_size;

    //
    //
    // STEP 16 :::: HANDLE PINNING and Class PROPERTIES if needed.
    //
    //

    if (clss->super_class) {
        if (get_prop_pinned (clss->super_class->class_properties)) {
            // If the super class is pinned then this class is pinned.
            set_prop_pinned (clss);
        }
    }
    // Set up the class_properties field.
    if (clss->is_array == 1) {
        clss->array_element_size = (vm_references_are_compressed() ? sizeof(COMPRESSED_REFERENCE) : sizeof(RAW_REFERENCE));
        set_prop_array (clss);
        if (is_vector_of_primitives (clss)) {
            clss->array_element_size = sizeof_primitive_array_element (clss);
            set_prop_non_ref_array (clss);
        }
        clss->vtable->array_element_size = (unsigned short)clss->array_element_size;
        switch (clss->vtable->array_element_size)
        {
        case 1:
            clss->vtable->array_element_shift = 0;
            break;
        case 2:
            clss->vtable->array_element_shift = 1;
            break;
        case 4:
            clss->vtable->array_element_shift = 2;
            break;
        case 8:
            clss->vtable->array_element_shift = 3;
            break;
        default:
            clss->vtable->array_element_shift = 65535;
            ASSERT(0, "Unexpected array element size: " << clss->vtable->array_element_size);
            break;
        }
    }else{
        clss->array_element_size = 0;
    }
    
    if (clss->has_finalizer) {
        set_prop_finalizable(clss);
    }

#ifndef POINTER64
    if(!strcmp("[D", clss->name->bytes)) {
        // In IA32, Arrays of Doubles need to be eight byte aligned to improve 
        // performance. In IPF all objects (arrays, class data structures, heap objects)
        // get aligned on eight byte boundaries. So, this special code is not needed.
        clss->alignment = ((GC_OBJECT_ALIGNMENT<8)?8:GC_OBJECT_ALIGNMENT);;
 
        // align doubles on 8, clear alignment field and put
        // in 8.
        set_prop_alignment_mask (clss, 8);
        // Set high bit in size so that gc knows there are constraints
    }
#endif

    //
    //
    // STEP 17 :::: HANDLE ALIGNMENT and Class FINALIZER if needed.
    //
    //
    if (clss->alignment) {
        if (clss->alignment != GC_OBJECT_ALIGNMENT) { 
            // The GC will align on 4 byte boundaries by default on IA32....
#ifdef POINTER64
            ASSERT(0, "Allignment is supposed to be appropriate");
#endif
            // Make sure it is a legal mask.
            assert ((clss->alignment & CL_PROP_ALIGNMENT_MASK) <= CL_PROP_ALIGNMENT_MASK);
            set_prop_alignment_mask (clss, clss->alignment);
            // make sure constraintbit was set.
            assert (get_instance_data_size(clss) != clss->instance_data_size);
        }
    }

    //
    //
    // STEP 18 :::: SET Class ALLOCATED SIZE to INSTANCE SIZE
    //
    //

    // Finally set the allocated size field.
    clss->allocated_size = get_instance_data_size(clss);
 

    //
    //
    // STEP 18a: Determine if class should have special verification treatment.
    //           This is needed to handle magic classes that are not verifiable but needed for, eg, reflection implementation
    //
    //
    if (should_class_not_be_verified(clss))
        clss->is_not_verified = TRUE;

    //
    //
    // STEP 19 :::: SET class to ST_Prepared state.
    //
    //
    gc_class_prepared(clss, clss->vtable);
    assert(clss->state == ST_InstanceSizeComputed);
    clss->state = ST_Prepared;
    TRACE2("classloader.prepare","class " << clss->name->bytes << " prepared");
    class_initialize_if_no_side_effects(clss);

    //
    // STEP 20 :::: ASSIGN VALUE to static final fields
    //
    // Generally speaking final value is inlined, so we needn't worry about the 
    //     initialization of to those static final fields. But when we use reflection 
    //     mechanisms-Field.getXXX()- to access them, we got null values. Consider this,
    //     We must initialize those static final fields.
    // Also related to this is Binary Compatibility chapter of the JLS.  
    //       Section 13.4.8 
    // 
    if (!assign_values_to_class_static_final_fields(clss)) 
    { 
        //OOME happened
        assert(hythread_is_suspend_enabled());
        return false;
    }

    //
    // STEP 21 :::: Link java.lang.Class to struct Class
    //
    // VM adds an extra field, 'vm_class', to all instances of
    // java.lang.Class (see an entry in vm_extra_fields).
    // This field is set to point to the corresponding struct Class.
    //
    // The code below stores the offset to that field in the VM environment.
    // Note that due to the complexity of bootstrapping, Java classes loaded
    // before java.lang.Class must be handled in a special way.  At the time
    // of writing this comment (2002-04-19), there are two classes loaded
    // before java.lang.Class: java.lang.Object and java.io.Serializable.
    if(clss->name == env->JavaLangClass_String) {
        String *name   = env->string_pool.lookup("vm_class");
        String* desc   = env->string_pool.lookup("J");
        Field *vm_class_field = class_lookup_field(clss, name, desc);
        assert(vm_class_field != NULL);
        env->vm_class_offset = vm_class_field->get_offset();
    }

    assert(clss->class_properties == clss->vtable->class_properties);
    assert(clss->allocated_size == clss->vtable->allocated_size);
    assert(clss->array_element_size == clss->vtable->array_element_size);

    //if( (env->JavaLangObject_Class != NULL && clss->name != env->JavaLangObject_Class->name)
    //    && (env->JavaLangClass_Class != NULL && clss->name != env->JavaLangClass_Class->name) )
    if(!env->InBootstrap())
    {
        autoUnlocker.ForceUnlock();
        assert(hythread_is_suspend_enabled());
        jvmti_send_class_prepare_event(clss);
    }
    TRACE2("classloader.prepare", "END class prepare, class name = " << clss->name->bytes);

    return true;
} //class_prepare


static void class_initialize_if_no_side_effects(Class *clss)
{
    if(clss->state == ST_Initialized) {
        return;
    }

    Class *c;
    for(c = clss; c; c = c->super_class) {
        if(c->state == ST_Initialized) {
            // 1. c and all its superclasses have been initialized.
            // 2. all the superclases of clss which are a subclass of c
            //    have no static constructors.
            break;
        }
        if(c->static_initializer) {
            // c is not initialized and has a static constructor.
            // Side effects are possible.
            return;
        }
    }

    // If we get here, initializing clss has no side effects
    for(c = clss; c; c = c->super_class) {
        if(c->state == ST_Initialized) {
            break;
        }
        c->state = ST_Initialized;
    }
} //class_initialize_if_no_side_effects
