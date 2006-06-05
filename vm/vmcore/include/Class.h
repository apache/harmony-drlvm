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
 * @author Pavel Pervov
 * @version $Revision: 1.1.2.7.2.1.2.5 $
 */  
#ifndef _CLASS_H_
#define _CLASS_H_
/**
 * @file
 * interfaces to class functionality.
 */

#include "jit_intf.h"
// The following is included for CL_PROP_* defines.
// Should they be moved somewhere and made constants?
#include "open/vm_gc.h"
#include "open/gc.h"
#include "String_Pool.h"
#include "type.h"

//
// forward declarations
//
class Class_Member;
struct Field;
struct Method;
struct Method_Signature;
class Package;
typedef struct Class Class;
class JIT;
struct ClassLoader;
class ByteReader;
struct Class_Extended_Notification_Record;
class DynoptInfo;
class Lock_Manager;

//
// external declarations
//
struct Global_Env;  // defined in Environment.h




///////////////////////////////////////////////////////////////////////////////
// Constant Java values
///////////////////////////////////////////////////////////////////////////////
union Const_Java_Value {
    uint32 i;
    int64 j;
    struct {
        uint32 lo_bytes;
        uint32 hi_bytes;
    } l;
    float f;
    double d;
    String *string;
    void *object;
    Const_Java_Value() {l.lo_bytes=l.hi_bytes=0;}
};



///////////////////////////////////////////////////////////////////////////////
// Raw and compressed reference pointers
///////////////////////////////////////////////////////////////////////////////
#define RAW_REFERENCE         ManagedObject *
#define COMPRESSED_REFERENCE  uint32

VMEXPORT bool is_compressed_reference(COMPRESSED_REFERENCE value);
VMEXPORT bool is_null_compressed_reference(COMPRESSED_REFERENCE value);

VMEXPORT COMPRESSED_REFERENCE  compress_reference(ManagedObject *obj);
VMEXPORT ManagedObject        *uncompress_compressed_reference(COMPRESSED_REFERENCE compressed_ref);
VMEXPORT ManagedObject        *get_raw_reference_pointer(ManagedObject **slot_addr);

// Store the reference "VALUE" in the slot at address "SLOT_ADDR" in the object "CONTAINING_OBJECT".
// Signature: void store_reference(ManagedObject *CONTAINING_OBJECT, ManagedObject **SLOT_ADDR, ManagedObject *VALUE);
#define STORE_REFERENCE(CONTAINING_OBJECT, SLOT_ADDR, VALUE)                                  \
    {                                                                                         \
        if (VM_Global_State::loader_env->compress_references) {                              \
            gc_heap_slot_write_ref_compressed((Managed_Object_Handle)(CONTAINING_OBJECT),     \
                                              (uint32 *)(SLOT_ADDR),                          \
                                              (Managed_Object_Handle)(VALUE));                \
        } else {                                                                              \
            gc_heap_slot_write_ref((Managed_Object_Handle)(CONTAINING_OBJECT),                \
                                   (Managed_Object_Handle *)(SLOT_ADDR),                      \
                                   (Managed_Object_Handle)(VALUE));                           \
        }                                                                                     \
    }


// Store the reference "VALUE" in the static field or other global slot at address "SLOT_ADDR".
// Signature: void store_global_reference(COMPRESSED_REFERENCE *SLOT_ADDR, ManagedObject *VALUE);
#define STORE_GLOBAL_REFERENCE(SLOT_ADDR, VALUE)                                              \
    {                                                                                         \
        if (VM_Global_State::loader_env->compress_references) {                              \
            gc_heap_write_global_slot_compressed((uint32 *)(SLOT_ADDR),                       \
                                                 (Managed_Object_Handle)(VALUE));             \
        } else {                                                                              \
            gc_heap_write_global_slot((Managed_Object_Handle *)(SLOT_ADDR),                   \
                                      (Managed_Object_Handle)(VALUE));                        \
        }                                                                                     \
    }



///////////////////////////////////////////////////////////////////////////////
// Constant pool entries.
///////////////////////////////////////////////////////////////////////////////

union Const_Pool {
    unsigned char  *tags;           // entry 0 of const pool only
    struct {                        // CONSTANT_Class
        union {
            Class* klass;       // resolved class
            struct {            // resolution error
                Const_Pool* next;       // next resolution error in this constant pool
                ManagedObject* cause;
            } error;
        };
        uint16 name_index;
    } CONSTANT_Class;
    struct {                        // CONSTANT_String
        String *string;     // resolved entry
        uint16 string_index;
    }  CONSTANT_String;
    struct {                        // CONSTANT_{Field,Method,InterfaceMethod}ref
        union {
            Field*  field;  // resolved entry for CONSTANT_Fieldref
            Method* method; // resolved entry for CONSTANT_{Interface}Methodref
            struct {        // resolution error
                Const_Pool* next;       // next resolution error in this constant pool
                ManagedObject* cause;
            } error;
        };
        uint16 class_index;
        uint16 name_and_type_index;
    } CONSTANT_ref;

    struct {    // shortcut to resolution error in CONSTANT_Class and CONSTANT_ref
        Const_Pool* next;       // next resolution error in this constant pool
        ManagedObject* cause;
    } error;

    uint32      int_value;          // CONSTANT_Integer
    float       float_value;        // CONSTANT_Float
    struct {
        uint32 low_bytes;   // each Const_Pool element is 64bit in this case 
        uint32 high_bytes;  // we pack all 8 bytes of long/double in one
                            // Const_Pool element (and leave the second
                            // Const_Pool element of the long/double unused)
    } CONSTANT_8byte;
    struct {                        // CONSTANT_NameAndType
        String *name;        // resolved entry
        String *descriptor;  // resolved entry
        uint16 name_index;
        uint16 descriptor_index;
    } CONSTANT_NameAndType;
    struct {
        String *string;             // CONSTANT_Utf8
        uint16 dummy;
    } CONSTANT_Utf8;
};




///////////////////////////////////////////////////////////////////////////////
// Types of constant pool entries.  These are defined by a seperate byte array
// pointed to by the first constant pool entry.
///////////////////////////////////////////////////////////////////////////////
enum Const_Pool_Tags {
    CONSTANT_Tags               = 0,    // pointer to tags array
    CONSTANT_Utf8               = 1,
    CONSTANT_Integer            = 3,
    CONSTANT_Float              = 4,
    CONSTANT_Long               = 5,
    CONSTANT_Double             = 6,
    CONSTANT_Class              = 7,
    CONSTANT_String             = 8,
    CONSTANT_Fieldref           = 9,
    CONSTANT_Methodref          = 10,
    CONSTANT_InterfaceMethodref = 11,
    CONSTANT_NameAndType        = 12,
};

#define TAG_MASK        0x0F    // 4 bits is sufficient for tag
#define RESOLVED_MASK   0x80    // msb is resolved flag
#define ERROR_MASK      0x40    // this entry contains resolution error information

#define cp_tag(cp,i)            (cp[0].tags[i] & TAG_MASK)
#define cp_is_resolved(cp,i)    (cp[0].tags[i] & RESOLVED_MASK)
#define cp_set_resolved(cp,i)   (cp[0].tags[i] |= RESOLVED_MASK)

#define cp_in_error(cp, i)      (cp[0].tags[i] & ERROR_MASK)
#define cp_set_error(cp, i)     (cp[0].tags[i] |= ERROR_MASK)

#define cp_is_utf8(cp,i)        (cp_tag(cp,i) == CONSTANT_Utf8)
#define cp_is_class(cp,i)       (cp_tag(cp,i) == CONSTANT_Class)
#define cp_is_constant(cp,i)    ((cp_tag(cp,i) >= CONSTANT_Integer      \
                                    && cp_tag(cp,i) <= CONSTANT_Double) \
                                    || cp_tag(cp,i) == CONSTANT_String)

#define cp_is_string(cp,i)              (cp_tag(cp,i) == CONSTANT_String)
#define cp_is_fieldref(cp,i)            (cp_tag(cp,i) == CONSTANT_Fieldref)
#define cp_is_methodref(cp,i)           (cp_tag(cp,i) == CONSTANT_Methodref)
#define cp_is_interfacemethodref(cp,i)  (cp_tag(cp,i) == CONSTANT_InterfaceMethodref)

#define cp_resolve_to_class(cp,i,c) cp_set_resolved(cp,i); cp[i].CONSTANT_Class.klass=c;

#define cp_resolve_to_field(cp,i,f) cp_set_resolved(cp,i); cp[i].CONSTANT_ref.field=f;

#define cp_resolve_to_method(cp,i,m) cp_set_resolved(cp,i); cp[i].CONSTANT_ref.method=m;

#define MAX_FAST_INSTOF_DEPTH 5

///////////////////////////////////////////////////////////////////////////////
// virtual method table of a class
///////////////////////////////////////////////////////////////////////////////
extern "C" {

typedef struct {
    unsigned char   **table;// pointer into methods array of VTable below
    unsigned intfc_id;      // id of interface
} Intfc_Table_Entry;

typedef struct Intfc_Table  {
#ifdef POINTER64
    // see INTFC_TABLE_OVERHEAD
    uint32 dummy;   // padding
#endif
    uint32 n_entries;
    Intfc_Table_Entry entry[1];
} Intfc_Table;


#define INTFC_TABLE_OVERHEAD    (sizeof(void *))


#ifdef POINTER64
#define OBJECT_HEADER_SIZE 0
// The size of an object reference. Used by arrays of object to determine
// the size of an element.
#define OBJECT_REF_SIZE 8
#else // POINTER64
#define OBJECT_HEADER_SIZE 0
#define OBJECT_REF_SIZE 4
#endif // POINTER64


#define GC_BYTES_IN_VTABLE (sizeof(void *))

typedef struct VTable {
    
    Byte _gc_private_information[GC_BYTES_IN_VTABLE];

    Class           *clss;          // the class - see above before change
    //
    // See the masks in vm_for_gc.h.
    //
    uint32 class_properties;


    // Offset from the top by CLASS_ALLOCATED_SIZE_OFFSET
    // The number of bytes allocated for this object. It is the same as
    // instance_data_size with the constraint bit cleared. This includes
    // the OBJECT_HEADER_SIZE as well as the OBJECT_VTABLE_POINTER_SIZE
    unsigned int allocated_size;

    unsigned short array_element_size;
    unsigned short array_element_shift;
    Intfc_Table     *intfc_table;   // interface table; NULL if no intfc table
//#ifdef FAST_INSTOF
    Class *superclasses[MAX_FAST_INSTOF_DEPTH]; //::
//#endif
    unsigned char   *methods[1];    // code for method
} VTable;


#define VTABLE_OVERHEAD (sizeof(VTable) - sizeof(void *))
// The "- sizeof(void *)" part subtracts out the "unsigned char *methods[1]" contribution.

VTable *create_vtable(Class *p_class, unsigned n_vtable_entries);

} // extern "C"


///////////////////////////////////////////////////////////////////////////////
// A Java class
///////////////////////////////////////////////////////////////////////////////
extern "C" {

//
// state of this class
//
enum Class_State {
    ST_Start,                   // initial state
    ST_LoadingAncestors,        // loading super class and super interfaces
    ST_Loaded,                  // successfully loaded
    ST_InstanceSizeComputed,    // still preparing the class but size of
                                // its instances is known
    ST_Prepared,                // successfully prepared
    ST_Initializing,            // initializing the class
    ST_Initialized,             // class initialized
    ST_Error                    // bad class or the initializer failed
};

typedef union {
        String *name;
        Class  *clss;
} Class_Superinterface;

typedef struct Class {

/////////////////////////////////////////////////////////////////////
//////// The first few fields can not be changed without reflecting
//////// the changes in the type Partial_Class in vm_for_gc.h
////////////////////////////////////////////////////////////////////

    //
    // super class of this class; initially, it is the string name of super
    // class; after super class is loaded, it becomes a pointer to class
    // structure of the super class.
    //
    // The offset of this field is returned by class_get_super_offset.
    // Make sure to update this function if the field is moved around.
    //
    union {
        String  *super_name;
        Class   *super_class;
    };

    const String * name;   // class name in internal (VM, class-file) format
    String * java_name;    // class canonical (Java) name

    //
    // See the masks in vm_for_gc.h.
    //

    uint32 class_properties;

    // Offset from the top by CLASS_ALLOCATED_SIZE_OFFSET
    // The number of bytes allocated for this object. It is the same as
    // instance_data_size with the constraint bit cleared. This includes
    // the OBJECT_HEADER_SIZE as well as the OBJECT_VTABLE_POINTER_SIZE
    unsigned int allocated_size;

    unsigned int array_element_size;
 

/////////////////////////////////////////////////////////////////////
//////// Fields above this point can not be moved without redefining
//////// the data structure Partial_Class in vm_for_gc.h
/////////////////////////////////////////////////////////////////////

    //
    // How should objects of this class be aligned by GC.
    //
    int alignment;


    //
    // unique class id
    //
    unsigned id;


    //
    // The class loader used to load this class.
    //
    ClassLoader* class_loader;


    //
    // Does it represent a primitive type?
    //
    unsigned is_primitive : 1;

    //
    // array information
    //
    unsigned is_array : 1;

    //
    // This is even TRUE for multidimensional arrays as long
    // as the type of the last dimension is primitve.
    //
    unsigned is_array_of_primitives : 1;


    //
    // Does the class have a finalizer that is not inherited from
    // java.lang.Object?
    //
    unsigned has_finalizer : 1;

    // Should this class not be verified (needed for certain special classes)
    unsigned is_not_verified : 1;

    //
    // Is this class verified by verifier
    // ??? FIXME - have to be in Class_State
    unsigned is_verified : 2;

    //
    // Can instances of this class be allocated using a fast inline sequence containing
    // no calls to other routines.
    //
    unsigned char is_fast_allocation_possible;

    //
    // number of dimensions in array; current VM limitation is 255
    //
    // Note, that you can derive the base component type of the array 
    // by looking at name->bytes[n_dimensions].
    //
    unsigned char n_dimensions;

    //
    // for non-primitive arrays only, array_base_class is the base class 
    // of an array
    //
    Class *array_base_class;


    Class *array_element_class;
    TypeDesc* array_element_type_desc;

    uint16 access_flags;
    uint16 cp_size;
    uint16 n_superinterfaces;
    uint16 n_fields;
    uint16 n_static_fields;
    uint16 n_methods;

    // for inner class support  
    uint16 declaringclass_index;
    uint16 n_innerclasses;
    uint16 *innerclass_indexes;

    Const_Pool *const_pool; // constant pool array; size is cp_size
    Field   *fields;            // array of fields; size is n_fields
    Method  *methods;           // array of methods; size is n_methods
    //
    // array of interfaces this class implements; size is n_superinterfaces
    // initially, it is an array of string names of interfaces and then
    // after superinterfaces are loaded, this becomes an array pointers
    // to superinterface class structures.
    //
    Class_Superinterface *superinterfaces;

    const String    *class_file_name;   // string name of file from which 
                                        // this class has been loaded
    const String    *src_file_name;     // string name of file from which 
                                        // this class has been compiled
    Class_State   state;                // state of this class

    Package *package;           // package to which this class belongs

    //
    // the following sizes are all in bytes
    //

    unsigned n_instance_refs;  // number of instance variables that are references

    unsigned n_virtual_method_entries;  // number virtual methods in vtable
    unsigned n_intfc_method_entries;    // number interface methods in vtable
    unsigned n_intfc_table_entries;     // number intfc tables in _intfc_table
                                        // same as _vtable->_intfc_table->n_entries

    Method  *finalize_method;           // NULL if none exists

    Method  *static_initializer;        // if it exists, NULL otherwise

    Method  *default_constructor;       // for performance

    unsigned static_data_size;      // size of this class' static data block
    void *static_data_block;    // block containing array of static data fields

    unsigned static_method_size;    // size in bytes of this class' static method block
    unsigned char **static_method_block;    // array of pointers to code of
                                            // static methods

    // This is the size of an instance without any alignment padding. 
    // It can be used while calculating the field offsets of subclasses.
    // It does not include the OBJECT_HEADER_SIZE but does include the
    // OBJECT_VTABLE_POINTER_SIZE.
    // The allocated_size field will be this field properly aligned.
    unsigned unpadded_instance_data_size;

    // Size of java/lang/Class instances in bytes. This variable is used during bootstrapping to allocate 
    // the three classes (i.e. Class instances) loaded before java.lang.Class: java.lang.Object, 
    // java.io.Serializable and java.lang.Class.
    //static unsigned sizeof_class_class;

    // Try to keep instance_data_size near vtable since they are used at the same time
    // by the allocation routines and sharing a cache line seem to help.
    //
    // The next to high bit is set if allocation needs to consider class_properties.
    // (mumble->instance_data_size & NEXT_TO_HIGH_BIT_CLEAR_MASK) will always return the
    // actual size of and instance of class mumble. 
    // Use get_instance_data_size() to get the actual size of an instance.
    // Use set_instance_data_size_constraint_bit() to set this bit.

    unsigned instance_data_size;    // For most classes the size of a class instance's 
                                    // data block. This is what is passed to the GC.
                                    // See above for details.
    // ppervov: FIXME: the next two should be joined into a union
    VTable  *vtable;            // virtual method table; NULL for interfaces
    Allocation_Handle allocation_handle;

    //
    // _vtable_descriptor is an array of pointers to Method descriptors, one
    // descriptor for each corresponding entry in _vtable.methods[].
    //
    Method  **vtable_descriptors;
    //
    // _intfc_table_descriptor is an array of pointers to Class descriptors,
    // one descriptor for each corresponding entry in intf_table.entry[];
    //
    Class   **intfc_table_descriptors;      // Class struture of interface
    // ppervov: FIXME: to remove
    void *class_object;

    bool printed_in_dump_jit;

    void *p_initializing_thread;            // this really points to VM thread data struct
    ManagedObject *p_error;          // enumeratable as static field

#ifdef VM_STATS
    uint64 num_class_init_checks;

    // For subclasses of java.lang.Throwable only.
    uint64 num_throws;

    // Number of instanceof/checkcast calls both from the user code
    // and the VM that were not subsumed by the fast version of instanceof.
    uint64 num_instanceof_slow;

    // Number of times an instance of the class has been created using new, newarray, etc.
    uint64 num_allocations;

    // Number of times an instance of the class has been created using new, newarray, etc.
    uint64 num_allocations_from_newInstance;

    // Number of bytes allocated for instances of the class.
    uint64 num_bytes_allocated;
#endif

    // Number of "padding" bytes curently added per class instance to its fields to 
    // make each field at least 32 bits.
    uint32 num_field_padding_bytes;

    // If set true by the "-compact_fields" command line option, the VM will not pad out fields of
    // less than 32 bits to four bytes. However, fields will still be aligned to a natural boundary,
    // and the num_field_padding_bytes field will reflect those alignment padding bytes.
    static bool compact_fields;

    // If set true by the "-sort_fields" command line option, the VM will sort fields by size before 
    // assigning their offset during class preparation.
    static bool sort_fields;

    // Notify JITs whenever this class is extended by calling their JIT_extended_class_callback callback function,
    Class_Extended_Notification_Record *notify_extended_records;   

    int depth;
    // The field is_suitable_for_fast_instanceof should be 0 if depth==0 or depth>=vm_max_fast_instanceof_depth()
    // or is_array or access_flags&ACC_INTERFACE.  It should be 1 otherwise.
    int is_suitable_for_fast_instanceof;

    // This points to the location where java.lang.Class associated with the current class resides.
    // Similarly, java.lang.Class has a field that points to the corresponding struct Class data structure.
    ManagedObject** class_handle;

    // 20030318 Gotten from the GC after gc_init() is called.
    static Byte *heap_base;
    static Byte *heap_end;

    // 2003-05-13.  This will be set to either NULL or heap_base depending
    // on whether compressed references are used.
    static Byte *managed_null;

//#ifdef VM_STATS
    // 20020923 Total number of allocations and total number of bytes for class-related data structures. 
    // This includes any rounding added to make each item aligned (current alignment is to the next 16 byte boundary).
    // Might need to make these uint64 for some data structures.
    static unsigned num_statics_allocations;
    static unsigned num_nonempty_statics_allocations;
    static unsigned num_vtable_allocations;
    static unsigned num_hot_statics_allocations;
    static unsigned num_hot_vtable_allocations;

    static unsigned total_statics_bytes;
    static unsigned total_vtable_bytes;
    static unsigned total_hot_statics_bytes;
    static unsigned total_hot_vtable_bytes;
//#endif //VM_STATS

    Class *cha_first_child;
    Class *cha_next_sibling;

    // New field definitions start here

    // SourceDebugExtension support
    String* sourceDebugExtension;

    // class operations lock
    Lock_Manager* m_lock;
    
    // List of constant pool entries, which resolution had failed
    // Required for fast enumeration of error objects
    Const_Pool* m_failedResolution;

    // struct Class accessibility
    unsigned m_markBit:1;

    // verify data
    void *verify_data;
} Class; // typedef struct Class


} // extern "C"


ManagedObject *struct_Class_to_java_lang_Class(Class *clss);
jclass struct_Class_to_jclass(Class *clss);
Class *jclass_to_struct_Class(jclass jc);
Class *jobject_to_struct_Class(jobject jobj);
jobject struct_Class_to_java_lang_Class_Handle(Class* clss);
Class *java_lang_Class_to_struct_Class(ManagedObject *jlc);
void set_struct_Class_field_in_java_lang_Class(const Global_Env* env,
    ManagedObject** jlc, Class* clss);
void class_report_failure(Class* target, uint16 cp_index, jthrowable exn);
jthrowable class_get_linking_error(Class_Handle ch, unsigned index);
jthrowable class_get_error(ClassLoaderHandle cl, const char* name);

void class_set_error_cause(Class *c, jthrowable exn);
jthrowable class_get_error_cause(Class *c);

String* class_get_java_name(Class* clss, Global_Env* env);
//
// access modifiers
//
#define class_is_public(clss)       ((clss)->access_flags & ACC_PUBLIC)
#define class_is_final(clss)        ((clss)->access_flags & ACC_FINAL)
#define class_is_super(clss)        ((clss)->access_flags & ACC_SUPER)
#define class_is_interface(clss)    ((clss)->access_flags & ACC_INTERFACE)
#define class_is_abstract(clss)     ((clss)->access_flags & ACC_ABSTRACT)

//
// Look up of methods and fields in class.
// Functions with the "_recursive" suffix also check superclasses.
//
VMEXPORT Field *class_lookup_field(Class *clss, const String* name, const String* desc);
VMEXPORT Field *class_lookup_field_recursive(Class *clss, const char *name, const char *descr);
Field *class_lookup_field_recursive(Class *clss, const String* name, const String* desc);
VMEXPORT Method *class_lookup_method(Class *clss, const String* name, const String* desc);
VMEXPORT Method *class_lookup_method_recursive(Class *clss, const String* name, const String* desc);
VMEXPORT Method *class_lookup_method_recursive(Class *clss, const char *name, const char *descr);
Method *class_lookup_method_init(Class*, const char*);
Method *class_lookup_method_clinit(Class*);
VMEXPORT Method *class_lookup_method(Class *clss, const char *name, const char *descr);

VMEXPORT Java_Type class_get_cp_const_type(Class *clss, unsigned cp_index);
VMEXPORT const void *class_get_addr_of_constant(Class *clss, unsigned cp_index);
VMEXPORT Field *class_resolve_field(Class *clss, unsigned cp_index);
//VMEXPORT Field *class_resolve_static_field(Class *clss, unsigned cp_index);
VMEXPORT Field *class_resolve_nonstatic_field(Class *clss, unsigned cp_index);
//VMEXPORT Method *class_resolve_static_method(Class *clss, unsigned cp_index);
//VMEXPORT Method *class_resolve_nonstatic_method(Class *clss, unsigned cp_index);
VMEXPORT Method *class_resolve_method(Class *clss, unsigned cp_index);
VMEXPORT Class *class_resolve_class(Class *clss, unsigned cp_index);

// Can "other_clss" access the field or method "member"
Boolean check_member_access(Class_Member *member, Class *other_clss);
// Can "other_clss" access the "inner_clss"
Boolean check_inner_class_access(Global_Env *env, Class *inner_clss, Class *other_clss);
// get class name from constant pool
extern String *cp_check_class(Const_Pool *cp, unsigned cp_size, unsigned class_index);

//
// parses in class description from a class file format
//
bool class_parse(Global_Env* env,
                 Class* clss,
                 unsigned* super_class_cp_index,
                 ByteReader& cfs);

const String* class_extract_name(Global_Env* env,
                                 uint8* buffer, unsigned offset, unsigned length);


//
// preparation phase of class loading
//
bool class_prepare(Global_Env* env, Class *clss);



//
// Load a class and perform the first two parts of the link process: verify
// and prepare.  The last stage of linking, resolution, is done at JIT-time.
// See the JVM spec 2.16.3.
//

VMEXPORT Class *class_load_verify_prepare_by_loader_jni(Global_Env* env,
                                                     const String* classname,
                                                     ClassLoader* cl);

VMEXPORT Class *class_load_verify_prepare_from_jni(Global_Env* env,
                                                   const String* classname);

unsigned class_calculate_size(const Class*);
void mark_classloader(ClassLoader*);
VMEXPORT void vm_notify_live_object_class(Class_Handle);

//
// execute static initializer of class
//

// Alexei
// migrating to C interfaces
#if (defined __cplusplus) && (defined PLATFORM_POSIX)
extern "C" {
#endif
VMEXPORT void class_initialize_from_jni(Class *clss, bool throw_exception);
#if (defined __cplusplus) && (defined PLATFORM_POSIX)
}
#endif
VMEXPORT void class_initialize_ex(Class *clss, bool throw_exception);
VMEXPORT void class_initialize(Class *clss);


// Notify JITs whenever this class is extended by calling their JIT_extended_class_callback callback function,
void class_register_jit_extended_class_callback(Class *clss, JIT *jit_to_be_notified, void *callback_data);
void do_jit_extended_class_callbacks(Class *clss, Class *new_subclass);

///////////////////////////////////////////////////////////////////////////////
// A class' members are its fields and methods.  Class_Member is the base
// class for Field and Method, and factors out the commonalities in these
// two classes.
///////////////////////////////////////////////////////////////////////////////
// VMEXPORT // temporary solution for interpreter unplug
class VMEXPORT Class_Member {
public:
    //
    // access modifiers
    //
    bool is_public()            {return (_access_flags&ACC_PUBLIC)?true:false;} 
    bool is_private()           {return (_access_flags&ACC_PRIVATE)?true:false;} 
    bool is_protected()         {return (_access_flags&ACC_PROTECTED)?true:false;} 
    bool is_static()            {return (_access_flags&ACC_STATIC)?true:false;} 
    bool is_final()             {return (_access_flags&ACC_FINAL)?true:false;} 
    bool is_strict()            {return (_access_flags&ACC_STRICT)?true:false;}
    bool is_synthetic()         {return _synthetic;}
    bool is_deprecated()        {return _deprecated;}
    unsigned get_access_flags() {return _access_flags;}

    //
    // field get/set methods
    //
    unsigned get_offset() const {return _offset;}
    Class *get_class() const    {return _class;}
    String *get_name() const    {return _name;}

    // Get the type descriptor (Sec. 4.3.2)
    String *get_descriptor() const {return _descriptor;}

    friend void assign_instance_field_offset(Class *clss, Field *field, bool do_field_compaction);
    friend void assign_offsets_to_static_fields(Class *clss, Field **field_ptrs, bool do_field_compaction);
    friend void assign_offsets_to_class_fields(Class *);
    friend void add_new_fake_method(Class *clss, Class *example, unsigned *next);
    friend void add_any_fake_methods(Class *);

    /**
     * Allocate a memory from a class loader pool using the class
     * loader lock.
     */
    void* Alloc(size_t size);

protected:
    Class_Member() 
    {
        _access_flags = 0;
        _class = NULL;
        _offset = 0;
#ifdef VM_STATS
        num_accesses = 0;
        num_slow_accesses = 0;
#endif
        _synthetic = _deprecated = false;
    }


    // offset of class member; 
    //   for virtual  methods, the method's offset within the vtable
    //   for static   methods, the method's offset within the class' static method table
    //   for instance data,    offset within the instance's data block
    //   for static   data,    offset within the class' static data block
    unsigned _offset;


    uint16 _access_flags;
    String      *_name;
    String      *_descriptor; // descriptor
    Class       *_class;
    bool parse(Class* clss, Const_Pool* cp, unsigned cp_size, ByteReader& cfs);

public:
#ifdef VM_STATS
    uint64 num_accesses;
    uint64 num_slow_accesses;
#endif
    bool _synthetic;
    bool _deprecated;
}; // Class_Member



///////////////////////////////////////////////////////////////////////////////
// Fields within Class structures.
///////////////////////////////////////////////////////////////////////////////
struct Field : public Class_Member{
public:
    //-----------------------

    // For all fields
    bool is_offset_computed() { return (_offset_computed != 0); }
    void set_offset_computed(bool is_computed) { _offset_computed = is_computed? 1 : 0; }

    // For static fields
    VMEXPORT void* get_address();

    // Return the type of this field.
    Java_Type get_java_type() {
        return (Java_Type)(get_descriptor()->bytes[0]); 
    };

    Const_Java_Value get_const_value() { return const_value; };
    uint16 get_const_value_index() { return _const_value_index; };

    //-----------------------

    Field() {
        _const_value_index = 0;
        _field_type_desc = 0;
        _offset_computed = 0;
        _is_injected = 0;
    }

    void Reset() { }

    void set(Class *cl, String* name, String* desc, unsigned short af) {
        _class = cl; _access_flags = af; _name = name; _descriptor = desc;
    }
    Field& operator = (const Field& fd) {
        // copy Class_Member fields
        _access_flags = fd._access_flags;
        _class = fd._class;
        _offset = fd._offset;
        _name = fd._name;
        _descriptor = fd._descriptor;
        _deprecated = fd._deprecated;
        _synthetic = fd._synthetic;
                
        // copy Field fields
        _const_value_index = fd._const_value_index;
        _field_type_desc = fd._field_type_desc;
        _is_injected = fd._is_injected;
        _offset_computed = fd._offset_computed;
        const_value = fd.const_value;
        return *this;
    }
    //
    // access modifiers
    //
    unsigned is_volatile()  {return (_access_flags&ACC_VOLATILE);} 
    unsigned is_transient() {return (_access_flags&ACC_TRANSIENT);} 
 
    bool parse(Class* clss, Const_Pool* cp, unsigned cp_size, ByteReader& cfs);

    unsigned calculate_size() {
        unsigned size = sizeof(Class_Member) + sizeof(Field);
        size += sizeof(TypeDesc);
        return size;
    }

    TypeDesc* get_field_type_desc() { return _field_type_desc; }
    void set_field_type_desc(TypeDesc* td) { _field_type_desc = td; }

    Boolean is_injected() {return _is_injected;}
    void set_injected() { _is_injected = 1; }

private:
    //
    // The initial values of static fields.  This is defined by the 
    // ConstantValue attribute in the class file.  
    //
    // If there was not ConstantValue attribute for that field then _const_value_index==0
    //
    uint16 _const_value_index;
    Const_Java_Value const_value;
    TypeDesc* _field_type_desc;
    unsigned _is_injected : 1;
    unsigned _offset_computed : 1;
}; // Field



///////////////////////////////////////////////////////////////////////////////
// Handler represents a catch block in a method's code array
///////////////////////////////////////////////////////////////////////////////
class Handler {
public:
    Handler();
    bool parse(Const_Pool *cp, unsigned cp_size,
        unsigned code_length, ByteReader &cfs);
    uint32 get_start_pc() {return _start_pc;}
    uint32 get_end_pc() {return _end_pc;}
    uint32 get_handler_pc() {return _handler_pc;}
    uint32 get_catch_type_index() {return _catch_type_index;}


private:
    uint32 _start_pc;
    uint32 _end_pc;
    uint32 _handler_pc;
    uint32 _catch_type_index;  // CP idx
    String *_catch_type;

}; //Handler



// Representation of target handlers in the generated code.
class Target_Exception_Handler {
public:
    Target_Exception_Handler(NativeCodePtr start_ip, NativeCodePtr end_ip, NativeCodePtr handler_ip, Class_Handle exn_class, bool exn_is_dead);

    NativeCodePtr get_start_ip();
    NativeCodePtr get_end_ip();
    NativeCodePtr get_handler_ip();
    Class_Handle  get_exc();
    bool          is_exc_obj_dead();

    bool is_in_range(NativeCodePtr eip, bool is_ip_past);
    bool is_assignable(Class_Handle exn_class);

    void update_catch_range(NativeCodePtr new_start_ip, NativeCodePtr new_end_ip);
    void update_handler_address(NativeCodePtr new_handler_ip);

private:
    NativeCodePtr _start_ip;
    NativeCodePtr _end_ip;
    NativeCodePtr _handler_ip;
    Class_Handle _exc;
    bool _exc_obj_is_dead;
}; //Target_Exception_Handler

typedef Target_Exception_Handler *Target_Exception_Handler_Ptr;


#define MAX_VTABLE_PATCH_ENTRIES 10

class VTable_Patches {
public:
    void *patch_table[MAX_VTABLE_PATCH_ENTRIES];
    VTable_Patches *next;
};



/////////////////////////////////////////////////////////////////
// begin multiple-JIT support

int get_index_of_jit(JIT *jit);


struct JIT_Data_Block {
    JIT_Data_Block *next;
    char bytes[1];
};


// Each callee for a given code chunk can have multiple Callee_Info structures, one for each call site in the caller.
typedef struct Callee_Info {
    void          *caller_ip;   // the IP in the caller where the call was made
    CodeChunkInfo *callee;      // which code chunk was called
    uint64         num_calls;
} Callee_Info;


#define NUM_STATIC_CALLEE_ENTRIES 8


class CodeChunkInfo {
    friend struct Method;
public:
    CodeChunkInfo();

    void     set_jit(JIT *jit)               { _jit = jit; }
    JIT     *get_jit()                       { return _jit; }

    void     set_method(Method *m)           { _method = m; }
    Method  *get_method()                    { return _method; }

    void     set_id(int id)                  { _id = id; }
    int      get_id()                        { return _id; }

    void     set_relocatable(Boolean r)      { _relocatable = r; }
    Boolean  get_relocatable()               { return _relocatable; }

    void     set_heat(unsigned heat)         { _heat = heat; }
    unsigned get_heat()                      { return _heat; }

    void    *get_code_block_addr()           { return _code_block; }
    void     set_code_block_addr(void *addr) { _code_block = addr; }

    size_t   get_code_block_size()           { return _code_block_size; }
    size_t   get_code_block_alignment()      { return _code_block_alignment; }

    void     set_loaded_for_vtune(bool v)    { _has_been_loaded_for_vtune = v; }
    bool     get_loaded_for_vtune()          { return _has_been_loaded_for_vtune; }

    unsigned get_num_callees()               { return _num_callees; }
    Callee_Info *get_callees()               { return _callee_info; }

    int      get_jit_index()                 { return get_index_of_jit(_jit); }

    // Note: _data_blocks can only be used for inline info for now
    Boolean  has_inline_info()               { return _data_blocks != NULL; }
    void    *get_inline_info()               { return &_data_blocks->bytes[0]; }

    unsigned get_num_target_exception_handlers();
    Target_Exception_Handler_Ptr get_target_exception_handler_info(unsigned eh_num);

    void     record_call_to_callee(CodeChunkInfo *callee, void *caller_return_ip);
    uint64   num_calls_to(CodeChunkInfo *other_chunk);

    void     print_name();
    void     print_name(FILE *file);
    void     print_info(bool print_ellipses=false);   // does not print callee information; see below
    void     print_callee_info();                     // prints the callee information; usually called after print_info()

    static void initialize_code_chunk(CodeChunkInfo *chunk);

public:
    // The section id of the main code chunk for a method. Using an enum avoids a VC++ bug on Windows.
    enum {main_code_chunk_id = 0};

    // A predicate that returns true iff this is the main code chunk for a method: i.e, it 1) contains the method's entry point, 
    // and 2) contains the various flavors of JIT data for that method.
    static bool is_main_code_chunk(CodeChunkInfo *chunk)  { assert(chunk);  return (chunk->get_id() == main_code_chunk_id); }

    // A predicate that returns true iff "id" is the section id of the main code chunk for a method. 
    static bool is_main_code_chunk_id(int id)             { return (id == main_code_chunk_id); }

private:
    // The triple (_jit, _method, _id) uniquely identifies a CodeChunkInfo.
    JIT            *_jit;
    Method         *_method;
    int             _id;
    bool            _relocatable;

    // "Target" handlers.
    unsigned        _num_target_exception_handlers;
    Target_Exception_Handler_Ptr *_target_exception_handlers;

    bool            _has_been_loaded_for_vtune;

    // 20040224 This records information about the methods (actually, CodeChunkInfo's) called by this CodeChunkInfo. 
    // 20040405 This now records for each callee, the number of times it was called by each call IP in the caller.
    // That is, this is a list of Callee_Info structures, each giving a call IP
    Callee_Info    *_callee_info;       // points to an array of max_callees Callee_Info entries for this code chunk
    unsigned        _num_callees;
    unsigned        _max_callees;
    Callee_Info     _static_callee_info[NUM_STATIC_CALLEE_ENTRIES]; // Array used if a small number of callers to avoid mallocs & frees

public:
    unsigned        _heat;
    void           *_code_block;
    void           *_jit_info_block;
    size_t          _code_block_size;
    size_t          _jit_info_block_size;
    size_t          _code_block_alignment;
    JIT_Data_Block *_data_blocks;
    DynoptInfo     *_dynopt_info;
    CodeChunkInfo  *_next;

#ifdef VM_STATS
    uint64          num_throws;
    uint64          num_catches;
    uint64          num_unwind_java_frames_gc;
    uint64          num_unwind_java_frames_non_gc;
#endif
}; //CodeChunkInfo


// end multiple-JIT support
/////////////////////////////////////////////////////////////////



// Used to notify interested JITs whenever a method is changed: overwritten, recompiled, 
// or initially compiled.
struct Method_Change_Notification_Record {
    Method *method_of_interest;
    JIT    *jit;
    void   *callback_data;
    Method_Change_Notification_Record *next;

    bool equals(Method *method_of_interest_, JIT *jit_, void *callback_data_) {
        if ((method_of_interest == method_of_interest_) &&
            (jit == jit_) &&
            (callback_data == callback_data_)) {
            return true;
        }
        return false;
    }
    // Optimized equals method. Most callbacks know method of interest, so we could skip one check.
    inline bool equals(JIT *jit_, void *callback_data_) {
        if ((callback_data == callback_data_) &&
            (jit == jit_)) {
            return true;
        }
        return false;
    }
};


struct Inline_Record;


// 20020222 This is only temporary to support the new JIT interface.
// We will reimplement the signature support.
struct Method_Signature {
public:
    TypeDesc* return_type_desc;
    unsigned num_args;
    TypeDesc** arg_type_descs;
    Method *method;
    String *sig;


    void initialize_from_method(Method *method);
    void reset();

private:
    void initialize_from_java_method(Method *method);
};



///////////////////////////////////////////////////////////////////////////////
// Methods defined in a class.
///////////////////////////////////////////////////////////////////////////////

// VMEXPORT // temporary solution for interpreter unplug
struct VMEXPORT Method : public Class_Member {
    //-----------------------
public:
    //
    // state of this method
    //
    enum State {
        ST_NotCompiled,                 // initial state
        ST_NotLinked = ST_NotCompiled,  // native not linked to implementation
        ST_BeingCompiled,               // jitting
        ST_Compiled,                    // compiled by JIT
        ST_Linked = ST_Compiled         // native linked to implementation
    };
    State get_state()                   {return _state;}
    void set_state(State st)            {_state=st;}

    // "Bytecode" exception handlers, i.e., those from the class file
    unsigned num_bc_exception_handlers();
    Handler *get_bc_exception_handler_info(unsigned eh_number);

    // "Target" exception handlers, i.e., those in the code generated by the JIT.
    void set_num_target_exception_handlers(JIT *jit, unsigned n);
    unsigned get_num_target_exception_handlers(JIT *jit);

    // Arguments:
    //  ...
    //  catch_clss  -- class of the exception or null (for "catch-all")
    //  ...
    void set_target_exception_handler_info(JIT *jit,
                                           unsigned eh_number,
                                           void *start_ip,
                                           void *end_ip,
                                           void *handler_ip,
                                           Class *catch_clss,
                                           bool exc_obj_is_dead = false);

    Target_Exception_Handler_Ptr get_target_exception_handler_info(JIT *jit, unsigned eh_num);

    unsigned num_exceptions_method_can_throw();
    String *get_exception_name (int n);

    // Address of the memory block containing bytecodes.  For best performance
    // the bytecodes should not be destroyed even after the method has been
    // jitted to allow re-compilation.  However the interface allows for such
    // deallocation.  The effect would be that re-optimizing JITs would not
    // show their full potential, but that may be acceptable for low-end systems
    // where memory is at a premium.
    // The value returned by getByteCodeAddr may be NULL in which case the
    // bytecodes are not available (presumably they have been garbage collected by VM).
    const Byte  *get_byte_code_addr()   {return _byte_codes;}
    size_t       get_byte_code_size()   {return _byte_code_length;}
 
    // From the class file (Sec. 4.7.4)
    unsigned get_max_stack()                       { return _max_stack; }
    unsigned get_max_locals()                      { return _max_locals; }

    // Returns an iterator for the argument list.
    Arg_List_Iterator get_argument_list();

    // Returns number of bytes of arguments pushed on the stack.
    // This value depends on the descriptor and the calling convention.
    unsigned get_num_arg_bytes();

    // Returns number of arguments.  For non-static methods, the this pointer
    // is included in this number
    unsigned get_num_args();

    // Number of arguments which are references.
    unsigned get_num_ref_args();


    // Return the return type of this method.
    Java_Type get_return_java_type();

    // For non-primitive types (i.e., classes) get the class type information.
    Class *get_return_class_type();

    // Address of the memory location containing the address of the code.
    // Used for static and special methods which have been resolved but not jitted.
    // The call would be:
    //      call dword ptr [addr]
    void *get_indirect_address()                   { return &_code; }

    // Entry address of the method.  Points to an appropriate stub or directly
    // to the code if no stub is necessary.
    void *get_code_addr()                          { return _code; }
    void set_code_addr(void *code_addr)            { _code = code_addr; }

    void add_vtable_patch(void *);
    void apply_vtable_patches();

    /**
     * This returns a block for jitted code. It is not used for native methods.
     * It is safe to call this function from multiple threads.
     */
    void *allocate_code_block_mt(size_t size, size_t alignment, JIT *jit, unsigned heat,
        int id, Code_Allocation_Action action);

    void *allocate_rw_data_block(size_t size, size_t alignment, JIT *jit);

    // The JIT can store some information in a JavaMethod object.
    void *allocate_jit_info_block(size_t size, JIT *jit);

    // JIT-specific data blocks.
    // Access should be protected with _lock.
    // FIXME
    // Think about moving lock aquisition inside public methods.
    void *allocate_JIT_data_block(size_t size, JIT *jit, size_t alignment);
    CodeChunkInfo *get_first_JIT_specific_info()   { return _jits; };
    CodeChunkInfo *get_JIT_specific_info_no_create(JIT *jit);
    /**
     * Find a chunk info for specific JIT. If no chunk exist for this JIT,
     * create and return one. This method is safe to call
     * from multiple threads.
     */
    CodeChunkInfo *get_chunk_info_mt(JIT *jit, int id);

    /**
     * Find a chunk info for specific JIT, or <code>NULL</code> if
     * no chunk info is created for this JIT. This method is safe to call
     * from multiple threads.
     */
    CodeChunkInfo *get_chunk_info_no_create_mt(JIT *jit, int id);

    /**
     * Allocate a new chunk info. This method is safe to call
     * from multiple threads.
     */
    CodeChunkInfo *create_code_chunk_info_mt();

    // Notify JITs whenever this method is overridden by a newly loaded class.
    void register_jit_overridden_method_callback(JIT *jit_to_be_notified, void *callback_data);
    void do_jit_overridden_method_callbacks(Method *overriding_method);

    // Notify JITs whenever this method is recompiled or initially compiled.
    void register_jit_recompiled_method_callback(JIT *jit_to_be_notified, void *callback_data);
    void do_jit_recompiled_method_callbacks();

    Method_Side_Effects get_side_effects()         { return _side_effects; };
    void set_side_effects(Method_Side_Effects mse) { _side_effects = mse; };

    Method_Signature *get_method_sig()             { return _method_sig; };
    void set_method_sig(Method_Signature *msig)    { _method_sig = msig; };

private:
    State _state;
    void *_code;
    VTable_Patches *_vtable_patch; 

    NativeCodePtr _counting_stub;

    CodeChunkInfo *_jits;

    Method_Side_Effects _side_effects;
    Method_Signature *_method_sig;

public:
    Method();
    // destructor should be instead of this function, but it's not allowed to use it because copy for Method class is
    // done with memcpy, and old value is destroyed with delete operator.
    void MethodClearInternals(); 

    //
    // access modifiers
    //
    bool is_synchronized()  {return (_access_flags&ACC_SYNCHRONIZED)?true:false;} 
    bool is_native()        {return (_access_flags&ACC_NATIVE)?true:false;} 
    bool is_abstract()      {return (_access_flags&ACC_ABSTRACT)?true:false;} 

    // method flags
    bool is_init()          {return _flags.is_init?true:false;}
    bool is_clinit()        {return _flags.is_clinit?true:false;}
    bool is_finalize()      {return _flags.is_finalize?true:false;}
    bool is_overridden()    {return _flags.is_overridden?true:false;}
    bool is_registered()    {return _flags.is_registered?true:false;}
    Boolean  is_nop()       {return _flags.is_nop;}

    void set_registered( bool flag ) { _flags.is_registered = flag; }

    unsigned get_index()    {return _index;}

    // Fake methods are interface methods inherited by an abstract class that are not (directly or indirectly) 
    // implemented by that class. They are added to the class to ensure they have thecorrect vtable offset.
    // These fake methods point to the "real" interface method for which they are surrogates; this information
    // is used by reflection methods.
    bool is_fake_method()           {return (_intf_method_for_fake_method != NULL);}
    Method *get_real_intf_method()  {return _intf_method_for_fake_method;}

    bool parse(Global_Env& env, Class* clss,
        Const_Pool* cp, unsigned cp_size, ByteReader& cfs);

    unsigned calculate_size() {
        unsigned size = sizeof(Class_Member) + sizeof(Method);
        if(_local_vars_table)
            size += sizeof(uint16) + _local_vars_table->length*sizeof(Local_Var_Entry);
        if(_line_number_table)
            size += sizeof(uint16) + _line_number_table->length*sizeof(Line_Number_Entry);
        size += _n_exceptions*sizeof(String*);
        size += _n_handlers*sizeof(Handler);
        size += _byte_code_length;
        return size;
    }

    friend void assign_offsets_to_class_methods(Class* clss);
    friend void add_new_fake_method(Class* clss, Class* example, unsigned* next);
    friend void add_any_fake_methods(Class* clss);

private:
    unsigned _index;                // index in method table
    uint16 _max_stack;
    uint16 _max_locals;
    uint16 _n_exceptions;           // num exceptions method can throw
    uint16 _n_handlers;             // num exception handlers in byte codes
    String  **_exceptions;          // array of exceptions method can throw
    uint32 _byte_code_length;       // num bytes of byte code
    Byte    *_byte_codes;           // method's byte codes
    Handler *_handlers;             // array of exception handlers in code
    Method *_intf_method_for_fake_method;
    struct {
        unsigned is_init        : 1;
        unsigned is_clinit      : 1;
        unsigned is_finalize    : 1;    // is finalize() method
        unsigned is_overridden  : 1;    // has this virtual method been overridden by a loaded subclass?
        unsigned is_nop         : 1;
        unsigned is_registered  : 1;    // the method is registred native method
    } _flags;

    //
    // private methods for parsing methods
    //
    bool _parse_code(Const_Pool *cp, unsigned cp_size, unsigned code_attr_len, ByteReader &cfs);

    bool _parse_line_numbers(unsigned attr_len, ByteReader &cfs);

    bool _parse_local_vars(Const_Pool *cp, unsigned cp_size, 
        unsigned attr_len, ByteReader &cfs);

    bool _parse_exceptions(Const_Pool *cp, unsigned cp_size, unsigned attr_len,
        ByteReader &cfs);

    void _set_nop();

    //
    // debugging info
    //
    struct Line_Number_Entry {
        uint16 start_pc;
        uint16 line_number;
    };

    struct Line_Number_Table {
        uint16 length;
        Line_Number_Entry table[1];
    };

    struct Local_Var_Entry {
        uint16 start_pc;
        uint16 length;
        uint16 index;
        String* name;
        String* type;
    };

    struct Local_Var_Table {
        uint16 length;
        Local_Var_Entry table[1];
    };

    Line_Number_Table *_line_number_table;
    Local_Var_Table *_local_vars_table;
public:

    unsigned get_line_number_table_size() {
        return (_line_number_table) ? _line_number_table->length : 0;
    }
    bool get_line_number_entry(unsigned index, jlong* pc, jint* line);
    unsigned get_local_var_table_size() {
        return (_local_vars_table) ? _local_vars_table->length : 0;
    }
    bool get_local_var_entry(unsigned index, jlong* pc, 
        jint* length, jint* slot, String** name, String** type);

    // Returns number of line in the source file, to which the given bytecode offset
    // corresponds, or -1 if it is unknown.
    int get_line_number(uint16 bc_offset);   

    Inline_Record *inline_records;
    void set_inline_assumption(JIT *jit, Method *caller);
    void method_was_overridden();

    Method_Change_Notification_Record *_notify_override_records;

    // Records JITs to be notified when a method is recompiled or initially compiled.
    Method_Change_Notification_Record *_notify_recompiled_records;

    void lock();
    void unlock();
}; // Method



///////////////////////////////////////////////////////////////////////////////
// class file attributes
///////////////////////////////////////////////////////////////////////////////
enum Attributes {
    ATTR_SourceFile,            // Class (no more than 1 in each class file)
    ATTR_InnerClasses,          // inner class
    ATTR_ConstantValue,         // Field (no more than 1 for each field)
    ATTR_Code,                  // Method
    ATTR_Exceptions,            // Method
    ATTR_LineNumberTable,       // Code
    ATTR_LocalVariableTable,    // Code
    ATTR_Synthetic,             // Field/Method
    ATTR_Deprecated,            // Field/Method
    ATTR_SourceDebugExtension,  // Class (no more than 1 in each class file)
    N_ATTR,
    ATTR_UNDEF,
    ATTR_ERROR
};

#define N_FIELD_ATTR    3
#define N_METHOD_ATTR   4
#define N_CODE_ATTR     2
#define N_CLASS_ATTR    3

//
// magic number, and major/minor version numbers of class file
//
#define CLASSFILE_MAGIC 0xCAFEBABE
#define CLASSFILE_MAJOR 45
// Supported class files up to this version
#define CLASSFILE_MAJOR_MAX 49
#define CLASSFILE_MINOR 3


VMEXPORT Class* load_class(Global_Env *env, const String *classname);
Class* find_loaded_class(Global_Env *env, const String *classname);

#define BITS_PER_BYTE 8
// We want to use signed arithmetic when we do allocation pointer/limit compares.
// In order to do this all sizes must be positive so when we want to overflow instead of 
// setting the high bit we set the next to high bit. If we set the high bit and the allocation buffer
// is at the top of memory we might not detect an overflow the unsigned overflow would produce
// a small positive number that is smaller then the limit.

#define NEXT_TO_HIGH_BIT_SET_MASK (1<<((sizeof(unsigned) * BITS_PER_BYTE)-2))
#define NEXT_TO_HIGH_BIT_CLEAR_MASK ~NEXT_TO_HIGH_BIT_SET_MASK

inline unsigned int get_instance_data_size (Class *c) 
{
    return (c->instance_data_size & NEXT_TO_HIGH_BIT_CLEAR_MASK);
}

inline void set_instance_data_size_constraint_bit (Class *c)
{
    c->instance_data_size = (c->instance_data_size | NEXT_TO_HIGH_BIT_SET_MASK);
}

// Setter functions for the class property field.
inline void set_prop_alignment_mask (Class *c, unsigned int the_mask)
{
    c->class_properties = (c->class_properties | the_mask);
    c->vtable->class_properties = c->class_properties;
    set_instance_data_size_constraint_bit (c);
}
inline void set_prop_non_ref_array (Class *c)
{
    c->class_properties = (c->class_properties | CL_PROP_NON_REF_ARRAY_MASK);
    c->vtable->class_properties = c->class_properties;
}
inline void set_prop_array (Class *c)
{
    c->class_properties = (c->class_properties | CL_PROP_ARRAY_MASK);
    c->vtable->class_properties = c->class_properties;
}
inline void set_prop_pinned (Class *c)
{
    c->class_properties = (c->class_properties | CL_PROP_PINNED_MASK);
    c->vtable->class_properties = c->class_properties;
    set_instance_data_size_constraint_bit (c);
}
inline void set_prop_finalizable (Class *c)
{
    c->class_properties = (c->class_properties | CL_PROP_FINALIZABLE_MASK);
    c->vtable->class_properties = c->class_properties;
    set_instance_data_size_constraint_bit (c);
}

// get functions for the class property field.
inline unsigned int get_prop_alignment (unsigned int class_properties)
{
    return (unsigned int)(class_properties & CL_PROP_ALIGNMENT_MASK);
}
inline unsigned int get_prop_non_ref_array (unsigned int class_properties)
{
    return (class_properties & CL_PROP_NON_REF_ARRAY_MASK);
}
inline unsigned int get_prop_array (unsigned int class_properties)
{
    return (class_properties & CL_PROP_ARRAY_MASK);
}
inline unsigned int get_prop_pinned (unsigned int class_properties)
{
    return (class_properties & CL_PROP_PINNED_MASK);
}
inline unsigned int get_prop_finalizable (unsigned int class_properties)
{
    return (class_properties & CL_PROP_FINALIZABLE_MASK);
}

VMEXPORT Class_Handle vtable_get_class(VTable_Handle vh);

/**
 * Function registers a number of native methods to a given class.
 *
 * @param klass       - specified class
 * @param methods     - array of methods
 * @param num_methods - number of methods
 *
 * @return <code>FALSE</code> if methods resistration is successful,
 *         otherwise - <code>TRUE</code>.
 *
 * @note Function raises <code>NoSuchMethodError</code> with method name in exception message
 *       if one of the methods in JNINativeMethod* array is not present in a specified class.
 */
bool
class_register_methods(Class_Handle klass, const JNINativeMethod* methods, int num_methods);

/**
 * Function unregisters a native methods of a given class.
 *
 * @param klass       - specified class
 *
 * @return <code>FALSE</code> if methods unresistration is successful,
 *         otherwise - <code>TRUE</code>.
 */
bool
class_unregister_methods(Class_Handle klass);

#endif // _CLASS_H_
