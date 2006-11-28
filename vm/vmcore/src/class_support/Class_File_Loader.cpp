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
 * @author Pavel Pervov, Alexey V. Varlamov
 * @version $Revision: 1.1.2.6.4.6 $
 */  

#define LOG_DOMAIN util::CLASS_LOGGER
#include "cxxlog.h"


#include "port_filepath.h"
#include <assert.h>

#include "environment.h"
#include "classloader.h"
#include "Class.h"
#include "class_member.h"
#include "vm_strings.h"
#include "open/vm_util.h"
#include "bytereader.h"
#include "compile.h"
#include "interpreter_exports.h"
#include "jarfile_util.h"

#ifdef _IPF_
#include "vm_ipf.h"
#endif //_IPF_

/*
 *  References to th JVM spec below are the references to Java Virtual Machine
 *  Specification, Second Edition
 */

//
// To do: 
//
//    (1) Check that ConstantValue attribute's type matches field's type
//    (2) Set the value field of Field, when ConstantValue is parsed
//    (3) verify correctness of Method access_flags (see pg 115 of the JVM spec)
//    (4) verify correctness of Field access_flags (pg 113 of the JVM spec)
//    (5) verify Class access_flags (pg 96 of the JVM spec)
//    (6) verify that for interfaces, super is always java.lang.Object (pg 36)
//    (7) interfaces cannot have super classes (assumed in preparation)
//

#define REPORT_FAILED_CLASS_FORMAT(klass, msg)   \
    {                                                               \
    std::stringstream ss;                                       \
    ss << klass->get_name()->bytes << " : " << msg;                                               \
    klass->get_class_loader()->ReportFailedClass(klass, "java/lang/ClassFormatError", ss);              \
    }

#define valid_cpi(clss, idx, type) \
    (clss->get_constant_pool().is_valid_index(idx) \
    && clss->get_constant_pool().get_tag(idx) == type)



static String* cp_check_utf8(ConstantPool& cp, unsigned utf8_index)
{
    if(!cp.is_valid_index(utf8_index) || !cp.is_utf8(utf8_index)) {
        return NULL;
    }
    return cp.get_utf8_string(utf8_index);
} // cp_check_utf8



static String* cp_check_class(ConstantPool& cp, unsigned class_index)
{
    if(!cp.is_valid_index(class_index) || !cp.is_class(class_index)) {
#ifdef _DEBUG
        WARN("cp_check_class: illegal const pool class index" << class_index);
#endif
        return NULL;
    }    
    return cp.get_utf8_string(cp.get_class_name_index(class_index));
} //cp_check_class


#define N_COMMON_ATTR   5
#define N_FIELD_ATTR    1
#define N_METHOD_ATTR   5
#define N_CODE_ATTR     3
#define N_CLASS_ATTR    4

static String *common_attr_strings[N_COMMON_ATTR+1];
static Attributes common_attrs[N_COMMON_ATTR];

static String *field_attr_strings[N_FIELD_ATTR+1];
static Attributes field_attrs[N_FIELD_ATTR];

static String *method_attr_strings[N_METHOD_ATTR+1];
static Attributes method_attrs[N_METHOD_ATTR];

static String *class_attr_strings[N_CLASS_ATTR+1];
static Attributes class_attrs[N_CLASS_ATTR];

static String *code_attr_strings[N_CODE_ATTR+1];
static Attributes code_attrs[N_CODE_ATTR];

static String *must_recognize_attr_strings[5];
static Attributes must_recognize_attrs[4];

//
// initialize string pool by preloading it with commonly used strings
//
static bool preload_attrs(String_Pool& string_pool)
{
    common_attr_strings[0] = string_pool.lookup("Synthetic");
    common_attrs[0] = ATTR_Synthetic;

    common_attr_strings[1] = string_pool.lookup("Deprecated");
    common_attrs[1] = ATTR_Deprecated;

    common_attr_strings[2] = string_pool.lookup("Signature");
    common_attrs[2] = ATTR_Signature;
    
    common_attr_strings[3] = string_pool.lookup("RuntimeVisibleAnnotations");
    common_attrs[3] = ATTR_RuntimeVisibleAnnotations;

    common_attr_strings[4] = string_pool.lookup("RuntimeInvisibleAnnotations");
    common_attrs[4] = ATTR_RuntimeInvisibleAnnotations;

    common_attr_strings[5] = NULL;

    method_attr_strings[0] = string_pool.lookup("Code");
    method_attrs[0] = ATTR_Code;

    method_attr_strings[1] = string_pool.lookup("Exceptions");
    method_attrs[1] = ATTR_Exceptions;

    method_attr_strings[2] = string_pool.lookup("RuntimeVisibleParameterAnnotations");
    method_attrs[2] = ATTR_RuntimeVisibleParameterAnnotations;

    method_attr_strings[3] = string_pool.lookup("RuntimeInvisibleParameterAnnotations");
    method_attrs[3] = ATTR_RuntimeInvisibleParameterAnnotations;

    method_attr_strings[4] = string_pool.lookup("AnnotationDefault");
    method_attrs[4] = ATTR_AnnotationDefault;

    method_attr_strings[5] = NULL;

    field_attr_strings[0] = string_pool.lookup("ConstantValue");
    field_attrs[0] = ATTR_ConstantValue;

    field_attr_strings[1] = NULL;

    class_attr_strings[0] = string_pool.lookup("SourceFile");
    class_attrs[0] = ATTR_SourceFile;

    class_attr_strings[1] = string_pool.lookup("InnerClasses");
    class_attrs[1] = ATTR_InnerClasses;

    class_attr_strings[2] = string_pool.lookup("SourceDebugExtension");
    class_attrs[2] = ATTR_SourceDebugExtension;

    class_attr_strings[3] = string_pool.lookup("EnclosingMethod");
    class_attrs[3] = ATTR_EnclosingMethod;
    
    class_attr_strings[4] = NULL;

    code_attr_strings[0] = string_pool.lookup("LineNumberTable");
    code_attrs[0] = ATTR_LineNumberTable;

    code_attr_strings[1] = string_pool.lookup("LocalVariableTable");
    code_attrs[1] = ATTR_LocalVariableTable;

    code_attr_strings[2] = string_pool.lookup("LocalVariableTypeTable");
    code_attrs[2] = ATTR_LocalVariableTypeTable;

    code_attr_strings[3] = NULL;

    must_recognize_attr_strings[0] = string_pool.lookup("Code");
    must_recognize_attrs[0] = ATTR_Code;

    must_recognize_attr_strings[1] = string_pool.lookup("ConstantValue");
    must_recognize_attrs[1] = ATTR_ConstantValue;

    must_recognize_attr_strings[2] = string_pool.lookup("Exceptions");
    must_recognize_attrs[2] = ATTR_Exceptions;

    must_recognize_attr_strings[3] = string_pool.lookup("Signature");
    must_recognize_attrs[3] = ATTR_Signature;

    must_recognize_attr_strings[4] = NULL;

    return true;
} //init_loader


String* parse_signature_attr(ByteReader &cfs,
                             uint32 attr_len, 
                             Class* clss) 
{
    if (attr_len != 2) {
        REPORT_FAILED_CLASS_FORMAT(clss, 
            "unexpected length of Signature attribute : " << attr_len);
        return NULL;
    }
    uint16 idx;
    if (!cfs.parse_u2_be(&idx)) {
        REPORT_FAILED_CLASS_FORMAT(clss, 
            "cannot parse Signature index");
        return NULL;
    }
    String* sig = cp_check_utf8(clss->get_constant_pool(), idx);
    if(!sig) {
        REPORT_FAILED_CLASS_FORMAT(clss, "invalid Signature index : " << idx);
    }
    return sig;
}


Attributes parse_attribute(ByteReader &cfs,
                           ConstantPool& cp,
                           String *attr_strings[],
                           Attributes attrs[],
                           uint32 *attr_len, 
                           bool use_common = true)
{
    static bool UNUSED init = preload_attrs(VM_Global_State::loader_env->string_pool);

    uint16 attr_name_index;
    bool result = cfs.parse_u2_be(&attr_name_index);
    if (!result)
        return ATTR_ERROR;

    result = cfs.parse_u4_be(attr_len);
    if (!result)
        return ATTR_ERROR;

    String* attr_name = cp_check_utf8(cp, attr_name_index);
    if (attr_name == NULL) {
#ifdef _DEBUG
        WARN("parse_attribute: illegal const pool attr_name_index");
#endif
        return ATTR_ERROR;
    }
    unsigned i;
    if (use_common) {
        for (i=0; common_attr_strings[i] != NULL; i++) {
            if (common_attr_strings[i] == attr_name)
                return common_attrs[i];
        }
    }
    for (i=0; attr_strings[i] != NULL; i++) {
        if (attr_strings[i] == attr_name)
            return attrs[i];
    }
    //
    // unrecognized attribute; skip
    //
    uint32 length = *attr_len;
    while ( length-- > 0) {
        uint8 val;
        bool result = cfs.parse_u1(&val);
        if(!result)
            return ATTR_ERROR;
    }
    // Code, ConstantValue, Exceptions must be recognized even if illegal in
    // a particular context
    for (i = 0; must_recognize_attr_strings[i] != NULL; i++) {
        if (must_recognize_attr_strings[i] == attr_name) {
            return must_recognize_attrs[i];
        }
    }

    return ATTR_UNDEF;
} //parse_attribute

// forward declaration
uint32 parse_annotation_value(AnnotationValue& value, ByteReader& cfs, Class* clss);

// returns number of read bytes, 0 if error occurred
uint32 parse_annotation(Annotation** value, ByteReader& cfs, Class* clss) 
{
    uint16 type_idx;
    if (!cfs.parse_u2_be(&type_idx)) {
        REPORT_FAILED_CLASS_FORMAT(clss, 
            "cannot parse type index of annotation");
        return 0;
    }
    String* type = cp_check_utf8(clss->get_constant_pool(), type_idx);
    if (type == NULL) {
        REPORT_FAILED_CLASS_FORMAT(clss, 
            "invalid type index of annotation : " << type_idx);
        return 0;
    }

    uint16 num_elements;
    if (!cfs.parse_u2_be(&num_elements)) {
        REPORT_FAILED_CLASS_FORMAT(clss, 
            "cannot parse number of annotation elements");
        return 0;
    }
    
    Annotation* antn = (Annotation*) clss->get_class_loader()->Alloc(
        sizeof(Annotation) + num_elements * sizeof(AnnotationElement));
    antn->type = type;
    antn->num_elements = num_elements;
    antn->elements = (AnnotationElement*)((POINTER_SIZE_INT)antn + sizeof(Annotation));
    *value = antn;

    uint32 read_len = 4;

    for (unsigned j = 0; j < num_elements; j++)
    {
        uint16 name_idx;
        if (!cfs.parse_u2_be(&name_idx)) {
            REPORT_FAILED_CLASS_FORMAT(clss, 
                "cannot parse element_name_index of annotation element");
            return 0;
        }
        antn->elements[j].name = cp_check_utf8(clss->get_constant_pool(), name_idx);
        if (antn->elements[j].name == NULL) {
            REPORT_FAILED_CLASS_FORMAT(clss, 
                "invalid element_name_index of annotation : " << name_idx);
            return 0;
        }

        uint32 size = parse_annotation_value(antn->elements[j].value, cfs, clss);
        if (size == 0) {
            return 0;
        }
        read_len += size + 2;
    }

    return read_len;
}

// returns number of read bytes, 0 if error occurred
uint32 parse_annotation_value(AnnotationValue& value, ByteReader& cfs, Class* clss)
{
    uint8 tag;
    if (!cfs.parse_u1(&tag)) {
        REPORT_FAILED_CLASS_FORMAT(clss, 
            "cannot parse annotation value tag");
        return 0;
    }
    value.tag = (AnnotationValueType)tag;
    uint32 read_len = 1;

    ConstantPool& cp = clss->get_constant_pool();
    unsigned cp_size = cp.get_size();

    char ctag = (char)tag;
    switch(ctag) {
    case AVT_BOOLEAN:
    case AVT_BYTE:
    case AVT_SHORT:
    case AVT_CHAR:
    case AVT_INT:
    case AVT_LONG:
    case AVT_FLOAT:
    case AVT_DOUBLE:
    case AVT_STRING:
        {
            uint16 const_idx;
            if (!cfs.parse_u2_be(&const_idx)) {
                REPORT_FAILED_CLASS_FORMAT(clss, 
                    "cannot parse const index of annotation value");
                return 0;
            }
            switch (ctag) {
            case AVT_BOOLEAN:
            case AVT_BYTE:
            case AVT_SHORT:
            case AVT_CHAR:
            case AVT_INT: 
                if (valid_cpi(clss, const_idx, CONSTANT_Integer)) {
                    value.const_value.i = cp.get_int(const_idx);
                    break;
                }
            case AVT_FLOAT: 
                if (valid_cpi(clss, const_idx, CONSTANT_Float)) {
                    value.const_value.f = cp.get_float(const_idx);
                    break;
                }
            case AVT_LONG: 
                if (valid_cpi(clss, const_idx, CONSTANT_Long)) {
                    value.const_value.l.lo_bytes = cp.get_8byte_low_word(const_idx);
                    value.const_value.l.hi_bytes = cp.get_8byte_high_word(const_idx);
                    break;
                }
            case AVT_DOUBLE: 
                if (valid_cpi(clss, const_idx, CONSTANT_Double)) {
                    value.const_value.l.lo_bytes = cp.get_8byte_low_word(const_idx);
                    value.const_value.l.hi_bytes = cp.get_8byte_high_word(const_idx);
                    break;
                }
            case AVT_STRING: 
                if (valid_cpi(clss, const_idx, CONSTANT_Utf8)) {
                    value.const_value.string = cp.get_utf8_string(const_idx);
                    break;
                }
            default:
                REPORT_FAILED_CLASS_FORMAT(clss, 
                    "invalid const index " << const_idx
                    << " of annotation value of type " <<ctag);
                return 0;
            }
            read_len += 2;
        }
    	break;

    case AVT_CLASS:
        {
            uint16 class_idx;
            if (!cfs.parse_u2_be(&class_idx)) {
                REPORT_FAILED_CLASS_FORMAT(clss, 
                    "cannot parse class_info_index of annotation value");
                return 0;
            }
            value.class_name = cp_check_utf8(cp, class_idx);
            if (value.class_name == NULL) {
                REPORT_FAILED_CLASS_FORMAT(clss, 
                    "invalid class_info_index of annotation value: " << class_idx);
                return 0;
            }
            read_len += 2;
        }
        break;

    case AVT_ENUM:
        {
            uint16 type_idx;
            if (!cfs.parse_u2_be(&type_idx)) {
                REPORT_FAILED_CLASS_FORMAT(clss, 
                    "cannot parse type_name_index of annotation enum value");
                return 0;
            }
            value.enum_const.type = cp_check_utf8(cp, type_idx);
            if (value.enum_const.type == NULL) {
                REPORT_FAILED_CLASS_FORMAT(clss, 
                    "invalid type_name_index of annotation enum value: " << type_idx);
                return 0;
            }
            uint16 name_idx;
            if (!cfs.parse_u2_be(&name_idx)) {
                REPORT_FAILED_CLASS_FORMAT(clss, 
                    "cannot parse const_name_index of annotation enum value");
                return 0;
            }
            value.enum_const.name = cp_check_utf8(cp, name_idx);
            if (value.enum_const.name == NULL) {
                REPORT_FAILED_CLASS_FORMAT(clss, 
                    "invalid const_name_index of annotation enum value: " << name_idx);
                return 0;
            }
            read_len += 4;
        }
        break;

    case AVT_ANNOTN:
        {
            uint32 size = parse_annotation(&value.nested, cfs, clss);
            if (size == 0) {
                return 0;
            }
            read_len += size;
        }
        break;

    case AVT_ARRAY:
        {
            uint16 num;
            if (!cfs.parse_u2_be(&num)) {
                REPORT_FAILED_CLASS_FORMAT(clss, 
                    "cannot parse num_values of annotation array value");
                return 0;
            }
            read_len += 2;
            value.array.length = num;
            if (num) {
                value.array.items = (AnnotationValue*) clss->get_class_loader()->Alloc(
                    num * sizeof(AnnotationValue));
                for (int i = 0; i < num; i++) {
                    uint32 size = parse_annotation_value(value.array.items[i], cfs, clss);
                    if (size == 0) {
                        return 0;
                    }
                    read_len += size;
                }
            }
        }
        break;

    default:
        REPORT_FAILED_CLASS_FORMAT(clss, 
            "unrecognized annotation value tag : " << ctag);
        return 0;
    }

    return read_len;
}

// returns number of read bytes, 0 if error occurred
uint32 parse_annotation_table(AnnotationTable ** table, ByteReader& cfs, Class* clss) 
{
    uint16 num_annotations;
    if (!cfs.parse_u2_be(&num_annotations)) {
        REPORT_FAILED_CLASS_FORMAT(clss, 
            "cannot parse number of Annotations");
        return 0;
    }
    uint32 read_len = 2;

    if (num_annotations) {
        *table = (AnnotationTable*) clss->get_class_loader()->Alloc(
            sizeof (AnnotationTable) + (num_annotations - 1)*sizeof(Annotation*));
        (*table)->length = num_annotations;

        for (unsigned i = 0; i < num_annotations; i++)
        {
            uint32 size = parse_annotation((*table)->table + i, cfs, clss);
            if (size == 0) {
                return 0;
            }
            read_len += size;
        }
    } else {
        *table = NULL;
    }

    return read_len;
}


Attributes Class_Member::process_common_attribute(Attributes attr, uint32 attr_len, ByteReader& cfs) 
{
    switch(attr) {
    case ATTR_Synthetic:
        if(attr_len != 0) {
            REPORT_FAILED_CLASS_FORMAT(_class,
                "non-zero length of Synthetic attribute for class member "
                << _name->bytes << " " <<_descriptor->bytes );
            return ATTR_ERROR;
        }
        _synthetic = true;
        _access_flags |= ACC_SYNTHETIC;
        break;

    case ATTR_Deprecated:
        if(attr_len != 0) {
            REPORT_FAILED_CLASS_FORMAT(_class,
                "non-zero length of Deprecated attribute for class member "
                << _name->bytes << " " <<_descriptor->bytes );
            return ATTR_ERROR;
        }
        _deprecated = true;
        break;

    case ATTR_Signature:
        {
            if (!(_signature = parse_signature_attr(cfs, attr_len, _class))) {
                return ATTR_ERROR;
            }
        }
        break;

    case ATTR_RuntimeVisibleAnnotations:
        {
            uint32 read_len = parse_annotation_table(&_annotations, cfs, _class);
            if (read_len == 0) {
                return ATTR_ERROR;
            } else if (attr_len != read_len) {
                REPORT_FAILED_CLASS_FORMAT(_class, 
                    "error parsing Annotations attribute for class member "
                    << _name->bytes << " " <<_descriptor->bytes 
                    << "; declared length " << attr_len
                    << " does not match actual " << read_len);
                return ATTR_ERROR;
            }
        }
        break;

    case ATTR_RuntimeInvisibleAnnotations:
        {
            if(!cfs.skip(attr_len)) {
                REPORT_FAILED_CLASS_FORMAT(_class,
                    "failed to skip RuntimeInvisibleAnnotations attribute");
                return ATTR_ERROR;
            }
        }
        break;

    default:
        return ATTR_UNDEF;
    }
    return attr;
}

void* Class_Member::Alloc(size_t size) {
    ClassLoader* cl = get_class()->get_class_loader();
    assert(cl);
    return cl->Alloc(size);
}


bool Class_Member::parse(Class* clss, ByteReader &cfs)
{
    if (!cfs.parse_u2_be(&_access_flags)) {
        REPORT_FAILED_CLASS_FORMAT(clss, "cannot parse member access flags");
        return false;
    }

    _class = clss;
    uint16 name_index;
    if (!cfs.parse_u2_be(&name_index)) {
        REPORT_FAILED_CLASS_FORMAT(clss, "cannot parse member name index");
        return false;
    }

    uint16 descriptor_index;
    if (!cfs.parse_u2_be(&descriptor_index)) {
        REPORT_FAILED_CLASS_FORMAT(clss, "cannot parse member descriptor index");
        return false;
    }

    ConstantPool& cp = clss->get_constant_pool();
    //
    // look up the name_index and descriptor_index 
    // utf8 string const pool entries
    //
    String* name = cp_check_utf8(cp, name_index);
    String* descriptor = cp_check_utf8(cp, descriptor_index);
    if (name == NULL || descriptor == NULL) {
        REPORT_FAILED_CLASS_FORMAT(clss, 
            "some of member name or descriptor indexes is not CONSTANT_Utf8 entry : " 
            << name_index << " or " << descriptor_index);
        return false;
    } 
    _name = name;
    _descriptor = descriptor;
    return true;
} //Class_Member::parse

// JVM spec:
// Unqualified names must not contain the characters ’.’, ’;’, ’[’ or ’/’. Method names are
// further constrained so that, with the exception of the special method names (§3.9)
// <init> and <clinit>, they must not contain the characters ’<’ or ’>’.
static inline bool
check_field_name(const String *name)
{
    for (const char* ch = name->bytes; ch[0] != '\0'; ++ch) {
        switch(ch[0]){
        case '.': 
        case ';':
        case '[':
        case '/':
            return false;
        }
    }
    return true;
}

static inline bool
check_method_name(const String *name, const Global_Env& env)
{
    if (name == env.Init_String || name == env.Clinit_String)
        return true;

    for (const char* ch = name->bytes; ch[0] != '\0'; ++ch) {
        switch(ch[0]){
        case '.': 
        case ';':
        case '[':
        case '/':
        case '<':
        case '>':
            return false;
        }
    }
    return true;
}

static inline bool
check_field_descriptor( const char *descriptor,
                        const char **next,
                        bool is_void_legal)
{
    switch (*descriptor) 
    {
    case 'B':
    case 'C':
    case 'D':
    case 'F':
    case 'I':
    case 'J':
    case 'S':
    case 'Z':
        *next = descriptor + 1;
        return true;
    case 'V':
        if( is_void_legal ) {
            *next = descriptor + 1;
            return true;
        } else {
            return false;
        }
    case 'L':
        {
            const char* iterator = descriptor + 1;
            while( *iterator != ';' ) {
                iterator++;
                if( *iterator == '\0' ) {
                    // bad Java descriptor
                    return false;
                }
            }
            *next = iterator + 1;
            return true;
        }
    case '[':
        {
            unsigned dim = 1;
            while(*(++descriptor) == '[') dim++;
            if (dim > 255) return false;
            if(!check_field_descriptor(descriptor, next, is_void_legal ))
                return false;
            return true;
        }
    default:
        // bad Java descriptor
        return false;
    }
    // DIE( "unreachable code!" ); // exclude remark #111: statement is unreachable
}

bool Field::parse(Class *clss, ByteReader &cfs)
{
    if(!Class_Member::parse(clss, cfs))
        return false;

    if(!check_field_name(_name)) {
        REPORT_FAILED_CLASS_FORMAT(clss, "illegal field name : " << _name->bytes);
        return false;
    }

    // check field descriptor
    const char* next;
    if(!check_field_descriptor(_descriptor->bytes, &next, false) || *next != '\0') {
        REPORT_FAILED_CLASS_FORMAT(clss, "illegal field descriptor : " << _descriptor->bytes);
        return false;
    }
    // check interface fields access flags
    if( clss->is_interface() ) {
        if(!(is_public() && is_static() && is_final())){
            REPORT_FAILED_CLASS_FORMAT(clss, "interface field " << get_name()->bytes
                << " does not have one of ACC_PUBLIC, ACC_STATIC, or ACC_FINAL access flags set");
            return false;
        }
        if(_access_flags & ~(ACC_FINAL | ACC_PUBLIC | ACC_STATIC | ACC_SYNTHETIC)){
            REPORT_FAILED_CLASS_FORMAT(clss, "interface field " << get_name()->bytes
                << " has illegal access flags set : " << _access_flags); //FIXME to literal form
            return false;
        }
    } else if((is_public() && is_protected() 
        || is_protected() && is_private() 
        || is_public() && is_private())
        || (is_final() && is_volatile())) {
        REPORT_FAILED_CLASS_FORMAT(clss, " field " << get_name()->bytes 
            << " has invalid combination of access flags : " << _access_flags); //FIXME to literal form
        return false;
    }

    uint16 attr_count;
    if(!cfs.parse_u2_be(&attr_count)) {
        REPORT_FAILED_CLASS_CLASS(clss->get_class_loader(), clss, "java/lang/ClassFormatError",
            clss->get_name()->bytes << ": could not parse attribute count for field " << get_name());
        return false;
    }

    _offset_computed = 0;

    unsigned n_constval_attr = 0;

    uint32 attr_len = 0;

    ConstantPool& cp = clss->get_constant_pool();

    for (unsigned j=0; j<attr_count; j++) 
    {
        Attributes cur_attr = parse_attribute(cfs, cp, field_attr_strings, field_attrs, &attr_len);
        switch (cur_attr) {
        case ATTR_ConstantValue:
        {    // constant value attribute
            // JVM spec (4.7.2) says that we should silently ignore the
            // ConstantValue attribute for non-static fields.

            // a field can have at most 1 ConstantValue attribute
            if (++n_constval_attr > 1) {
                REPORT_FAILED_CLASS_CLASS(clss->get_class_loader(), clss, "java/lang/ClassFormatError",
                    clss->get_name()->bytes << ": field " << get_name() << " has more then one ConstantValue attribute");
                return false;
            }
            // attribute length must be two (vm spec reference 4.7.3)
            if (attr_len != 2) {
                REPORT_FAILED_CLASS_CLASS(clss->get_class_loader(), clss, "java/lang/ClassFormatError",
                    clss->get_name()->bytes << ": ConstantValue attribute has invalid length for field " << get_name());
                return false;
            }

            if(!cfs.parse_u2_be(&_const_value_index)) {
                REPORT_FAILED_CLASS_CLASS(clss->get_class_loader(), clss,
                    "java/lang/ClassFormatError",
                    clss->get_name()->bytes << ": could not parse "
                    << "ConstantValue index for field " << get_name());
                return false;
            }

            if(_const_value_index == 0 || _const_value_index >= cp.get_size()) {
                REPORT_FAILED_CLASS_CLASS(clss->get_class_loader(), clss, "java/lang/ClassFormatError",
                    clss->get_name()->bytes << ": invalid ConstantValue index for field " << get_name());
                return false;
            }

            Java_Type java_type = get_java_type();

            switch(cp.get_tag(_const_value_index)) {
            case CONSTANT_Long:
                {
                    if (java_type != JAVA_TYPE_LONG) {
                        REPORT_FAILED_CLASS_CLASS(clss->get_class_loader(), clss, "java/lang/ClassFormatError",
                            clss->get_name()->bytes
                            << ": data type CONSTANT_Long of ConstantValue does not correspond to the type of field "
                            << get_name());
                        return false;
                    }
                    const_value.l.lo_bytes = cp.get_8byte_low_word(_const_value_index);
                    const_value.l.hi_bytes = cp.get_8byte_high_word(_const_value_index);
                    break;
                }
            case CONSTANT_Float:
                {
                    if (java_type != JAVA_TYPE_FLOAT) {
                        REPORT_FAILED_CLASS_CLASS(clss->get_class_loader(), clss, "java/lang/ClassFormatError",
                            clss->get_name()->bytes
                            << ": data type CONSTANT_Float of ConstantValue does not correspond to the type of field "
                            << get_name());
                        return false;
                    }
                    const_value.f = cp.get_float(_const_value_index);
                    break;
                }
            case CONSTANT_Double:
                {
                    if (java_type != JAVA_TYPE_DOUBLE) {
                        REPORT_FAILED_CLASS_CLASS(clss->get_class_loader(), clss, "java/lang/ClassFormatError",
                            clss->get_name()->bytes
                            << ": data type CONSTANT_Double of ConstantValue does not correspond to the type of field "
                            << get_name());
                        return false;
                    }
                    const_value.l.lo_bytes = cp.get_8byte_low_word(_const_value_index);
                    const_value.l.hi_bytes = cp.get_8byte_high_word(_const_value_index);
                    break;
                }
            case CONSTANT_Integer:
                {
                if ( !(java_type == JAVA_TYPE_INT         || 
                       java_type == JAVA_TYPE_SHORT       ||
                       java_type == JAVA_TYPE_BOOLEAN     || 
                       java_type == JAVA_TYPE_BYTE        ||
                        java_type == JAVA_TYPE_CHAR) )
                    {
                        REPORT_FAILED_CLASS_CLASS(clss->get_class_loader(), clss, "java/lang/ClassFormatError",
                            clss->get_name()->bytes
                            << ": data type CONSTANT_Integer of ConstantValue does not correspond to the type of field "
                            << get_name());
                        return false;
                    }
                    const_value.i = cp.get_int(_const_value_index);
                    break;
                }
            case CONSTANT_String:
                {
                    if (java_type != JAVA_TYPE_CLASS) {
                        REPORT_FAILED_CLASS_CLASS(clss->get_class_loader(), clss,
                            "java/lang/ClassFormatError",
                            clss->get_name()->bytes
                            << ": data type " << "CONSTANT_String of "
                            << "ConstantValue does not correspond "
                            << "to the type of field " << get_name());
                        return false;
                    }
                    const_value.string = cp.get_string(_const_value_index);
                    break;
                }
            default:
                {
                    REPORT_FAILED_CLASS_CLASS(clss->get_class_loader(), clss,
                        "java/lang/ClassFormatError",
                        clss->get_name()->bytes
                        << ": invalid data type tag of ConstantValue "
                        << "for field " << get_name());
                    return false;
                }
            }
            break;
        }

        case ATTR_UNDEF:
            // unrecognized attribute; skipped
            break;
        default:
            switch(process_common_attribute(cur_attr, attr_len, cfs)) {
            case ATTR_ERROR: 
                // tried and failed
                return false;
            case ATTR_UNDEF:
                // unprocessed
                REPORT_FAILED_CLASS_CLASS(clss->get_class_loader(), clss, "java/lang/InternalError",
                    clss->get_name()->bytes
                    << ": error parsing attributes for field "
                    << get_name()->bytes
                    << "; unprocessed attribute " << cur_attr);
                return false;
            } // switch
        } // switch
    } // for

    TypeDesc* td = type_desc_create_from_java_descriptor(get_descriptor()->bytes, clss->get_class_loader());
    if( td == NULL ) {
        // ppervov: the fact we don't have td indicates we could not allocate one
        //std::stringstream ss;
        //ss << clss->get_name()->bytes << ": could not create type descriptor for field " << get_name();
        //jthrowable exn = exn_create("java/lang/OutOfMemoryError", ss.str().c_str());
        exn_raise_object(VM_Global_State::loader_env->java_lang_OutOfMemoryError);
        return false;
    }
    set_field_type_desc(td);

    return true;
} //Field::parse


bool Handler::parse(ConstantPool& cp, unsigned code_length,
                    ByteReader& cfs)
{
    uint16 start = 0;
    if(!cfs.parse_u2_be(&start))
        return false;

    _start_pc = (unsigned) start;

    if (_start_pc >= code_length)
        return false;

    uint16 end;
    if (!cfs.parse_u2_be(&end))
        return false;
    _end_pc = (unsigned) end;

    if (_end_pc > code_length)
        return false;

    if (_start_pc >= _end_pc)
        return false;

    uint16 handler;
    if (!cfs.parse_u2_be(&handler))
        return false;
    _handler_pc = (unsigned) handler;

    if (_handler_pc >= code_length)
        return false;

    uint16 catch_index;
    if (!cfs.parse_u2_be(&catch_index))
        return false;

    _catch_type_index = catch_index;

    if (catch_index == 0) {
        _catch_type = NULL;
    } else {
        _catch_type = cp_check_class(cp, catch_index);
        if (_catch_type == NULL)
            return false;
    }
    return true;
} //Handler::parse


bool Method::get_line_number_entry(unsigned index, jlong* pc, jint* line) {
    if (_line_number_table && index < _line_number_table->length) {
        *pc = _line_number_table->table[index].start_pc;
        *line = _line_number_table->table[index].line_number;
        return true;
    } else {
        return false;
    }
}

bool Method::get_local_var_entry(unsigned index, jlong* pc, 
                         jint* length, jint* slot, String** name, 
                         String** type, String** generic_type) {

    if (_line_number_table && index < _local_vars_table->length) {
        *pc = _local_vars_table->table[index].start_pc;
        *length = _local_vars_table->table[index].length;
        *slot = _local_vars_table->table[index].index;
        *name = _local_vars_table->table[index].name;
        *type = _local_vars_table->table[index].type;
        *generic_type = _local_vars_table->table[index].generic_type;
        return true;
    } else {
        return false;
    }
}

#define REPORT_FAILED_METHOD(msg) REPORT_FAILED_CLASS_CLASS(_class->get_class_loader(), \
    _class, "java/lang/ClassFormatError", \
    _class->get_name()->bytes << " : " << msg << " for method "\
    << _name->bytes << _descriptor->bytes);


bool Method::_parse_exceptions(ConstantPool& cp, unsigned attr_len,
                               ByteReader& cfs)
{
    if(!cfs.parse_u2_be(&_n_exceptions)) {
        REPORT_FAILED_CLASS_CLASS(_class->get_class_loader(), _class, "java/lang/ClassFormatError",
            _class->get_name()->bytes << ": could not parse number of exceptions for method "
            << _name->bytes << _descriptor->bytes);
        return false;
    }

    _exceptions = new String*[_n_exceptions];
    for (unsigned i=0; i<_n_exceptions; i++) {
        uint16 index;
        if(!cfs.parse_u2_be(&index)) {
            REPORT_FAILED_CLASS_CLASS(_class->get_class_loader(), _class, "java/lang/ClassFormatError",
                _class->get_name()->bytes << ": could not parse exception class index "
                << "while parsing excpetions for method "
                << _name->bytes << _descriptor->bytes);
            return false;
        }

        _exceptions[i] = cp_check_class(cp, index);
        if (_exceptions[i] == NULL) {
            REPORT_FAILED_CLASS_CLASS(_class->get_class_loader(), _class, "java/lang/ClassFormatError",
                _class->get_name()->bytes << ": exception class index "
                << index << "is not a valid CONSTANT_class entry "
                << "while parsing excpetions for method "
                << _name->bytes << _descriptor->bytes);
            return false;
        }
    }
    if (attr_len != _n_exceptions * sizeof(uint16) + sizeof(_n_exceptions) ) {
        REPORT_FAILED_CLASS_CLASS(_class->get_class_loader(), _class, "java/lang/ClassFormatError",
            _class->get_name()->bytes << ": invalid Exceptions attribute length "
            << "while parsing excpetions for method "
            << _name->bytes << _descriptor->bytes);
        return false;
    }
    return true;
} //Method::_parse_exceptions

bool Method::_parse_line_numbers(unsigned attr_len, ByteReader &cfs) {
    uint16 n_line_numbers;
    if(!cfs.parse_u2_be(&n_line_numbers)) {
        REPORT_FAILED_METHOD("could not parse line_number_table_length "
            "while parsing LineNumberTable attribute");
        return false;
    }
    unsigned real_lnt_attr_len = 2 + n_line_numbers * 4; 
    if(real_lnt_attr_len != attr_len) {
        REPORT_FAILED_METHOD("real LineNumberTable length differ "
            "from attribute_length ("
            << attr_len << " vs. " << real_lnt_attr_len << ")" );
        return false;
    }
    
    _line_number_table =
        (Line_Number_Table *)STD_MALLOC(sizeof(Line_Number_Table) +
        sizeof(Line_Number_Entry) * (n_line_numbers - 1));
    // ppervov: FIXME: should throw OOME
    _line_number_table->length = n_line_numbers;

    uint16 start_pc;
    uint16 line_number;
    for (unsigned j=0; j<n_line_numbers; j++) {

        if(!cfs.parse_u2_be(&start_pc)) {
            REPORT_FAILED_METHOD("could not parse start_pc "
                "while parsing LineNumberTable");
            return false;
        }

        if(start_pc >= _byte_code_length) {
            REPORT_FAILED_METHOD("start_pc in LineNumberTable"
                "points outside the code");
            return false;
        }

        if(!cfs.parse_u2_be(&line_number)) {
            REPORT_FAILED_METHOD("could not parse line_number "
                "while parsing LineNumberTable");
            return false;
        }
        _line_number_table->table[j].start_pc = start_pc;
        _line_number_table->table[j].line_number = line_number;
    }
    return true;
} //Method::_parse_line_numbers


bool Method::_parse_local_vars(const char* attr_name, Local_Var_Table** lvt_address,
        ConstantPool& cp, unsigned attr_len, ByteReader &cfs)
{
    uint16 n_local_vars;
    if(!cfs.parse_u2_be(&n_local_vars)) {
        REPORT_FAILED_METHOD("could not parse local variables number "
            "of " << attr_name << " attribute");
        return false;
    }

    unsigned real_lnt_attr_len = 2 + n_local_vars * 10; 
    if(real_lnt_attr_len != attr_len) {
        REPORT_FAILED_METHOD("real " << attr_name << " length differ "
            "from declared length ("
            << attr_len << " vs. " << real_lnt_attr_len << ")" );
        return false;
    }
    if (!n_local_vars) {
        return true;
    }

    Local_Var_Table* table = (Local_Var_Table *)_class->get_class_loader()->Alloc(
        sizeof(Local_Var_Table) +
        sizeof(Local_Var_Entry) * (n_local_vars - 1));
    // ppervov: FIXME: should throw OOME
    table->length = n_local_vars;

    for (unsigned j = 0; j < n_local_vars; j++) {
        uint16 start_pc;    
        if(!cfs.parse_u2_be(&start_pc)) {
            REPORT_FAILED_METHOD("could not parse start_pc "
                "in " << attr_name << " attribute");
            return false;
        }
        uint16 length;      
        if(!cfs.parse_u2_be(&length)) {
            REPORT_FAILED_METHOD("could not parse length entry "
                "in " << attr_name << " attribute");
            return false;
        }

        if( (start_pc >= _byte_code_length)
            || (start_pc + (unsigned)length) > _byte_code_length ) {
            REPORT_FAILED_METHOD(attr_name << " entry "
                "[start_pc, start_pc + length) points outside bytecode range");
            return false;
        }

        uint16 name_index;
        if(!cfs.parse_u2_be(&name_index)) {
            REPORT_FAILED_METHOD("could not parse name index "
                "in " << attr_name << " attribute");
            return false;
        }

        uint16 descriptor_index;
        if(!cfs.parse_u2_be(&descriptor_index)) {
            REPORT_FAILED_METHOD("could not parse descriptor index "
                "in " << attr_name << " attribute");
            return false;
        }

        String* name = cp_check_utf8(cp, name_index);
        if(name == NULL) {
            REPORT_FAILED_METHOD("name index is not valid CONSTANT_Utf8 entry "
                "in " << attr_name << " attribute");
            return false;
        }

        String* descriptor = cp_check_utf8(cp, descriptor_index);
        if(descriptor == NULL) {
            REPORT_FAILED_METHOD("descriptor index is not valid CONSTANT_Utf8 entry "
                "in " << attr_name << " attribute");
            return false;
        }

        uint16 index;
        if(!cfs.parse_u2_be(&index)) {
            REPORT_FAILED_METHOD("could not parse index "
                "in " << attr_name << " attribute");
            return false;
        }

        // FIXME Don't work with long and double
        if (index >= _max_locals) {
            REPORT_FAILED_METHOD("invalid local index "
                "in " << attr_name << " attribute");
            return false;
        }

        table->table[j].start_pc = start_pc;
        table->table[j].length = length;
        table->table[j].index = index;
        table->table[j].name = name;
        table->table[j].type = descriptor;
        table->table[j].generic_type = NULL;
    }

    assert(lvt_address);
    *lvt_address = table;

    return true;
} //Method::_parse_local_vars


bool Method::_parse_code(ConstantPool& cp, unsigned code_attr_len,
                         ByteReader& cfs)
{
    unsigned real_code_attr_len = 0;
    if(!cfs.parse_u2_be(&_max_stack)) {
        REPORT_FAILED_CLASS_CLASS(_class->get_class_loader(), _class, "java/lang/ClassFormatError",
            _class->get_name()->bytes << ": could not parse max_stack "
            << "while parsing Code attribute for method "
            << _name->bytes << _descriptor->bytes);
        return false;
    }

    if(!cfs.parse_u2_be(&_max_locals)) {
        REPORT_FAILED_CLASS_CLASS(_class->get_class_loader(), _class, "java/lang/ClassFormatError",
            _class->get_name()->bytes << ": could not parse max_locals "
            << "while parsing Code attribute for method "
            << _name->bytes << _descriptor->bytes);
        return false;
    }

    if(_max_locals < _arguments_size/4) {
        REPORT_FAILED_CLASS_CLASS(_class->get_class_loader(), _class, "java/lang/ClassFormatError",
            _class->get_name()->bytes << ": wrong max_locals count "
            << "while parsing Code attribute for method "
            << _name->bytes << _descriptor->bytes);
        return false;
    }

    if(!cfs.parse_u4_be(& _byte_code_length)) {
        REPORT_FAILED_CLASS_CLASS(_class->get_class_loader(), _class, "java/lang/ClassFormatError",
            _class->get_name()->bytes << ": could not parse bytecode length "
            << "while parsing Code attribute for method "
            << _name->bytes << _descriptor->bytes);
        return false;
    }

    // code length for non-abstract java methods must not be 0
    if(_byte_code_length == 0
        || (_byte_code_length >= (1<<16) && !is_native() && !is_abstract()))
    {
        REPORT_FAILED_CLASS_CLASS(_class->get_class_loader(), _class, "java/lang/ClassFormatError",
            _class->get_name()->bytes << ": bytecode length for method "
            << _name->bytes << _descriptor->bytes
            << " has zero length");
        return false;
    }
    real_code_attr_len = 8;

    //
    // allocate & parse code array
    //
    _byte_codes = new Byte[_byte_code_length];
    // ppervov: FIXME: should throw OOME

    unsigned i;
    for (i=0; i<_byte_code_length; i++) {
        if(!cfs.parse_u1((uint8 *)&_byte_codes[i])) {
            REPORT_FAILED_CLASS_CLASS(_class->get_class_loader(), _class, "java/lang/ClassFormatError",
                _class->get_name()->bytes << ": could not parse bytecode for method "
                << _name->bytes << _descriptor->bytes);
            return false;
        }
    }
    real_code_attr_len += _byte_code_length;

    if(!cfs.parse_u2_be(&_n_handlers)) {
            REPORT_FAILED_CLASS_CLASS(_class->get_class_loader(), _class, "java/lang/ClassFormatError",
                _class->get_name()->bytes << ": could not parse number of exception handlers for method "
                << _name->bytes << _descriptor->bytes);
        return false;
    }
    real_code_attr_len += 2;

    // 
    // allocate & parse exception handler table
    //
    _handlers = new Handler[_n_handlers];
    // ppervov: FIXME: should throw OOME

    for (i=0; i<_n_handlers; i++) {
        if(!_handlers[i].parse(cp, _byte_code_length, cfs)) {
            REPORT_FAILED_CLASS_CLASS(_class->get_class_loader(), _class, "java/lang/ClassFormatError",
                _class->get_name()->bytes << ": could not parse exceptions for method "
                << _name->bytes << _descriptor->bytes);
            return false;
        }
    }
    real_code_attr_len += _n_handlers*8; // for the size of exception_table entry see JVM Spec 4.7.3

    //
    // attributes of the Code attribute
    //
    uint16 n_attrs;
    if(!cfs.parse_u2_be(&n_attrs)) {
        REPORT_FAILED_CLASS_CLASS(_class->get_class_loader(), _class, "java/lang/ClassFormatError",
            _class->get_name()->bytes << ": could not parse number of attributes for method "
            << _name->bytes << _descriptor->bytes);
        return false;
    }
    real_code_attr_len += 2;

    static bool TI_enabled = VM_Global_State::loader_env->TI->isEnabled();
    Local_Var_Table * generic_vars = NULL;

    uint32 attr_len = 0;
    for (i=0; i<n_attrs; i++) {
        Attributes cur_attr = parse_attribute(cfs, cp, code_attr_strings, code_attrs, &attr_len, false);
        switch(cur_attr) {
        case ATTR_LineNumberTable:
            {
                if  (!_parse_line_numbers(attr_len, cfs)) {
                    return false;
                }
                break;
            }
        case ATTR_LocalVariableTable:
            {
                if (TI_enabled)
                {
                    if (!_parse_local_vars("LocalVariableTable", &_local_vars_table, cp, attr_len, cfs))
                    {
                        return false;
                    }
                }
                else if (!cfs.skip(attr_len))
                {
                    REPORT_FAILED_METHOD("error skipping Local_Variable_Table");
                }
                break;
            }
        case ATTR_LocalVariableTypeTable:
            {
                if (TI_enabled)
                {
                    if (!_parse_local_vars("LocalVariableTypeTable", &generic_vars, cp, attr_len, cfs))
                    {
                        return false;
                    }
                }
                else if (!cfs.skip(attr_len))
                {
                    REPORT_FAILED_METHOD("error skipping LocalVariableTypeTable");
                }
            }
            break;

        case ATTR_UNDEF:
            // unrecognized attribute; skipped
            break;
        default:
            // error occured
            REPORT_FAILED_CLASS_CLASS(_class->get_class_loader(), _class, "java/lang/InternalError",
                _class->get_name()->bytes << ": unknown error occured "
                "while parsing attributes for code of method "
                << _name->bytes << _descriptor->bytes
                << "; unprocessed attribute " << cur_attr);
            return false;
        } // switch
        real_code_attr_len += 6 + attr_len; // u2 - attribute_name_index, u4 - attribute_length
    } // for

    if(code_attr_len != real_code_attr_len) {
        REPORT_FAILED_CLASS_CLASS(_class->get_class_loader(), _class, "java/lang/ClassFormatError",
            _class->get_name()->bytes << ": Code attribute length does not match real length "
            "in class file (" << code_attr_len << " vs. " << real_code_attr_len
            << ") while parsing attributes for code of method "
            << _name->bytes << _descriptor->bytes);
        return false;
    }

    // JVM spec hints that LocalVariableTypeTable is meant to be a supplement to LocalVariableTable
    // so we have no reason to cross-check these tables
    if (generic_vars && _local_vars_table) {
        unsigned j;
        for (i = 0; i < generic_vars->length; ++i) {
            for (j = 0; j < _local_vars_table->length; ++j) {
                if (generic_vars->table[i].name == _local_vars_table->table[j].name) {
                    _local_vars_table->table[j].generic_type = generic_vars->table[i].type;
                }
            }
        }
    }

    return true;
} //Method::_parse_code

static inline bool
check_method_descriptor( const char *descriptor )
{
    const char *next;
    bool result;

    if( *descriptor != '(' ) return false;

    next = ++descriptor;
    while( descriptor[0] != ')' )
    {
        result = check_field_descriptor(descriptor, &next, false);
        if( !result || *next == '\0' ) {
            return result;
        }
        descriptor = next;
    }
    next = ++descriptor;
    result = check_field_descriptor(descriptor, &next, true);
    if( *next != '\0' ) return false;
    return result;
}


bool Method::parse(Global_Env& env, Class* clss,
                   ByteReader &cfs)
{
    if(!Class_Member::parse(clss, cfs))
        return false;

    if(!check_method_name(_name, env)) {
        REPORT_FAILED_CLASS_FORMAT(clss, "illegal method name : " << _name->bytes);
        return false;
    }

    // check method descriptor
    if(!check_method_descriptor(_descriptor->bytes)) {
        REPORT_FAILED_CLASS_CLASS(_class->get_class_loader(), _class, "java/lang/ClassFormatError",
            _class->get_name()->bytes << ": invalid descriptor "
            "while parsing method "
            << _name->bytes << _descriptor->bytes);
        return false;
    }
    calculate_arguments_size();

    uint16 attr_count;
    if (!cfs.parse_u2_be(&attr_count)) {
        REPORT_FAILED_CLASS_CLASS(_class->get_class_loader(), _class, "java/lang/ClassFormatError",
            _class->get_name()->bytes << ": could not parse attributes count for method "
            << _name->bytes << _descriptor->bytes);
        return false;
    }
    _intf_method_for_fake_method = NULL;

    // set the has_finalizer, is_clinit and is_init flags
    if (_name == env.FinalizeName_String && _descriptor == env.VoidVoidDescriptor_String) {
        _flags.is_finalize = 1;
    }
    else if (_name == env.Init_String)
        _flags.is_init = 1;
    else if (_name == env.Clinit_String)
        _flags.is_clinit = 1;
    // check method access flags
    if(!is_clinit())
    {
        if(is_private() && is_protected() || is_private() && is_public() || is_protected() && is_public())
        {
            bool bout = false;
            REPORT_FAILED_CLASS_CLASS(_class->get_class_loader(), _class, "java/lang/ClassFormatError",
                _class->get_name()->bytes << ": invalid combination of access flags ("
                << ((bout = is_public())?"ACC_PUBLIC":"")
                << (bout?"|":"")
                << ((bout |= is_protected())?"ACC_PROTECTED":"")
                << (bout?"|":"")
                << (is_private()?"ACC_PRIVATE":"")
                << ") for method "
                << _name->bytes << _descriptor->bytes);
            return false;
        }
        if(is_abstract() 
        && (is_final() || is_native() || is_private() || is_static() || is_strict() || is_synchronized()))
        {
            bool bout = false;
            REPORT_FAILED_CLASS_CLASS(_class->get_class_loader(), _class, "java/lang/ClassFormatError",
                _class->get_name()->bytes << ": invalid combination of access flags (ACC_ABSTRACT|"
                << ((bout = is_final())?"ACC_FINAL":"")
                << (bout?"|":"")
                << ((bout |= is_native())?"ACC_NATIVE":"")
                << (bout?"|":"")
                << ((bout |= is_private())?"ACC_PRIVATE":"")
                << (bout?"|":"")
                << ((bout |= is_static())?"ACC_STATIC":"")
                << (bout?"|":"")
                << ((bout |= is_strict())?"ACC_STRICT":"")
                << (bout?"|":"")
                << ((bout |= is_synchronized())?"ACC_SYNCHRONIZED":"")
                << ") for method "
                << _name->bytes << _descriptor->bytes);
            return false;
        }
        if(_class->is_interface() && !(is_abstract() && is_public())) {
            REPORT_FAILED_CLASS_CLASS(_class->get_class_loader(), _class, "java/lang/ClassFormatError",
                _class->get_name()->bytes << "." << _name->bytes << _descriptor->bytes
                << ": interface method cannot have access flags other then "
                "ACC_ABSTRACT and ACC_PUBLIC set"
                );
            return false;
        }
        if(_class->is_interface() &&
            (is_private() || is_protected() || is_static() || is_final()
            || is_synchronized() || is_native() || is_strict()))
        {
            REPORT_FAILED_CLASS_CLASS(_class->get_class_loader(), _class, "java/lang/ClassFormatError",
                _class->get_name()->bytes << "." << _name->bytes << _descriptor->bytes
                << ": interface method cannot have access flags other then "
                "ACC_ABSTRACT and ACC_PUBLIC set");
            return false;
        }
    }    
    if(is_init() && (is_static() || is_final() || is_synchronized() || is_native() || is_abstract() || is_bridge())) {
        REPORT_FAILED_CLASS_CLASS(_class->get_class_loader(), _class, "java/lang/ClassFormatError",
            _class->get_name()->bytes << "." << _name->bytes << _descriptor->bytes
            << ": constructor cannot have access flags other then "
            "ACC_STRICT and one of ACC_PUBLIC, ACC_PRIVATE, or ACC_PROTECTED set");
        return false;
    }
    if(is_clinit()) {
        // Java VM specification
        // 4.6 Methods
        // "Class and interface initialization methods (�3.9) are called
        // implicitly by the Java virtual machine; the value of their
        // access_flags item is ignored except for the settings of the
        // ACC_STRICT flag"
        _access_flags &= ACC_STRICT;
        // compiler assumes that <clinit> has ACC_STATIC
        // but VM specification does not require this flag to be present
        // so, enforce it
        _access_flags |= ACC_STATIC;
    }

    unsigned n_code_attr = 0;
    unsigned n_exceptions_attr = 0;
    uint32 attr_len = 0;
    ConstantPool& cp = clss->get_constant_pool();

    for (unsigned j=0; j<attr_count; j++) {
        //
        // only code and exception attributes are defined for Method
        //
        Attributes cur_attr = parse_attribute(cfs, cp, method_attr_strings, method_attrs, &attr_len);
        switch(cur_attr) {
        case ATTR_Code:
            n_code_attr++;
            if(!_parse_code(cp, attr_len, cfs))
                return false;
            break;

        case ATTR_Exceptions:
            n_exceptions_attr++;
            if(!_parse_exceptions(cp, attr_len, cfs))
                return false;
            break;

        case ATTR_RuntimeInvisibleParameterAnnotations:
            if (!cfs.skip(attr_len))
            {
                REPORT_FAILED_METHOD("error skipping RuntimeInvisibleParameterAnnotations");
                return false;
            }
            break;

        case ATTR_RuntimeVisibleParameterAnnotations:
            {
                if (_param_annotations) {
                    REPORT_FAILED_METHOD(
                        "more than one RuntimeVisibleParameterAnnotations attribute");
                    return false;
                }

                if (!cfs.parse_u1(&_num_param_annotations)) {
                    REPORT_FAILED_CLASS_FORMAT(clss, 
                        "cannot parse number of ParameterAnnotations");
                    return false;
                }
                uint32 read_len = 1;
                if (_num_param_annotations) {
                    _param_annotations = (AnnotationTable**)_class->get_class_loader()->Alloc(
                        _num_param_annotations * sizeof (AnnotationTable*));

                    for (unsigned i = 0; i < _num_param_annotations; i++)
                    {
                        uint32 next_len = parse_annotation_table(_param_annotations + i, cfs, _class);
                        if (next_len == 0) {
                            return false;
                        } 
                        read_len += next_len;
                    }
                }
                if (attr_len != read_len) {
                    REPORT_FAILED_METHOD( 
                        "error parsing ParameterAnnotations attribute"
                        << "; declared length " << attr_len
                        << " does not match actual " << read_len);
                    return false;
                }            
            }
            break;

        case ATTR_AnnotationDefault:
            {
                if (_default_value) {
                    REPORT_FAILED_METHOD("more than one AnnotationDefault attribute");
                    return false;
                }
                _default_value = (AnnotationValue *)_class->get_class_loader()->Alloc(
                    sizeof(AnnotationValue));

                uint32 read_len = parse_annotation_value(*_default_value, cfs, clss);
                if (read_len == 0) {
                    return false;
                } else if (read_len != attr_len) {
                    REPORT_FAILED_METHOD(
                        "declared length " << attr_len
                        << " of AnnotationDefault attribute "
                        << " does not match actual " << read_len);
                    return false;
                }
            }
            break;

        case ATTR_UNDEF:
            // unrecognized attribute; skipped
            break;

        default:
            switch(process_common_attribute(cur_attr, attr_len, cfs)) {
            case ATTR_ERROR: 
                // tried and failed
                return false;
            case ATTR_UNDEF:
                // unprocessed
                REPORT_FAILED_CLASS_CLASS(clss->get_class_loader(), clss, "java/lang/InternalError",
                    clss->get_name()->bytes
                    << " : error parsing attributes for method "
                    << _name->bytes << _descriptor->bytes
                    << "; unprocessed attribute " << cur_attr);
                return false;
            } // switch
        } // switch
    } // for

    //
    // there must be no more than 1 code attribute and no more than 1 exceptions
    // attribute per method
    //
    if (n_code_attr > 1) {
        REPORT_FAILED_CLASS_CLASS(_class->get_class_loader(), _class, "java/lang/ClassFormatError",
            _class->get_name()->bytes << ": there is more than one Code attribute for method "
            << _name->bytes << _descriptor->bytes);
        return false;
    }
    if(n_exceptions_attr > 1) {
        REPORT_FAILED_CLASS_CLASS(_class->get_class_loader(), _class, "java/lang/ClassFormatError",
            _class->get_name()->bytes << ": there is more than one Exceptions attribute for method "
            << _name->bytes << _descriptor->bytes);
        return false;
    }

    if((is_abstract() || is_native()) && n_code_attr > 0) {
        REPORT_FAILED_CLASS_CLASS(_class->get_class_loader(), _class, "java/lang/ClassFormatError",
            _class->get_name()->bytes << "." << _name->bytes << _descriptor->bytes
            << ": " << (is_abstract()?"abstract":(is_native()?"native":""))
            << " method should not have Code attribute present");
        return false;
    }

    if(!(is_abstract() || is_native()) && n_code_attr == 0) {
        REPORT_FAILED_CLASS_CLASS(_class->get_class_loader(), _class, "java/lang/ClassFormatError",
            _class->get_name()->bytes << "." << _name->bytes << _descriptor->bytes
            << ": Java method should have Code attribute present");
        return false;
    }
    if(is_native()) _access_flags |= ACC_NATIVE;
    return true;
} //Method::parse


bool Class::parse_fields(Global_Env* env, ByteReader& cfs)
{
    // Those fields are added by the loader even though they are nor defined
    // in their corresponding class files.
    static struct VMExtraFieldDescription {
        const String* classname;
        String* fieldname;
        String* descriptor;
        uint16 accessflags;
    } vm_extra_fields[] = {
        { env->string_pool.lookup("java/lang/Thread"),
                env->string_pool.lookup("vm_thread"),
                env->string_pool.lookup("J"), ACC_PRIVATE},
        { env->string_pool.lookup("java/lang/Throwable"),
                env->string_pool.lookup("vm_stacktrace"),
                env->string_pool.lookup("[J"), ACC_PRIVATE|ACC_TRANSIENT},
        { env->string_pool.lookup("java/lang/Class"),
                env->string_pool.lookup("vm_class"),
                env->string_pool.lookup("J"), ACC_PRIVATE},
    };
    if(!cfs.parse_u2_be(&m_num_fields)) {
        REPORT_FAILED_CLASS_CLASS(m_class_loader, this, "java/lang/ClassFormatError",
            get_name()->bytes << ": could not parse number of fields");
        return false;
    }

    int num_fields_in_class_file = m_num_fields;
    int i;
    for(i = 0; i < int(sizeof(vm_extra_fields)/sizeof(VMExtraFieldDescription)); i++) {
        if(get_name() == vm_extra_fields[i].classname) {
            m_num_fields++;
        }
    }

    m_fields = new Field[m_num_fields];
    // ppervov: FIXME: should throw OOME

    m_num_static_fields = 0;
    unsigned short last_nonstatic_field = (unsigned short)num_fields_in_class_file;
    for(i=0; i < num_fields_in_class_file; i++) {
        Field fd;
        if(!fd.parse(this, cfs))
            return false;
        if(fd.is_static()) {
            m_fields[m_num_static_fields] = fd;
            m_num_static_fields++;
        } else {
            last_nonstatic_field--;
            m_fields[last_nonstatic_field] = fd;
        }
    }
    assert(last_nonstatic_field == m_num_static_fields);

    for(i = 0; i < int(sizeof(vm_extra_fields)/sizeof(VMExtraFieldDescription)); i++) {
        if(get_name() == vm_extra_fields[i].classname) {
            Field& f = m_fields[num_fields_in_class_file];
            f.set(this, vm_extra_fields[i].fieldname,
                vm_extra_fields[i].descriptor, vm_extra_fields[i].accessflags);
            f.set_injected();
            TypeDesc* td = type_desc_create_from_java_descriptor(
                vm_extra_fields[i].descriptor->bytes, m_class_loader);
            if( td == NULL ) {
                // error occured
                // ppervov: FIXME: should throw OOME
                return false;
            }
            f.set_field_type_desc(td);
            num_fields_in_class_file++;
        }
    }

    return true; // success
} //class_parse_fields


long _total_method_bytes = 0;

bool Class::parse_methods(Global_Env* env, ByteReader &cfs)
{
    if(!cfs.parse_u2_be(&m_num_methods)) {
        REPORT_FAILED_CLASS_CLASS(m_class_loader, this, "java/lang/ClassFormatError",
            get_name()->bytes << ": could not parse number of methods");
        return false;
    }

    m_methods = new Method[m_num_methods];

    _total_method_bytes += sizeof(Method)*m_num_methods;
    for(unsigned i = 0;  i < m_num_methods; i++) {
        if(!m_methods[i].parse(*env, this, cfs)) {
            return false;
        }

        Method* m = &m_methods[i];
        if(m->is_clinit()) {
            // There can be at most one clinit per class.
            if(m_static_initializer) {
                REPORT_FAILED_CLASS_CLASS(m_class_loader, this, "java/lang/ClassFormatError",
                    get_name()->bytes << ": there is more than one class initialization method");
                return false;
            }
            m_static_initializer = &(m_methods[i]);
        }
        // to cache the default constructor 
        if (m->get_name() == VM_Global_State::loader_env->Init_String
            && m->get_descriptor() == VM_Global_State::loader_env->VoidVoidDescriptor_String)
        {
            m_default_constructor = &m_methods[i];
        }
    }
    return true; // success
} //class_parse_methods


static String* class_file_parse_utf8data(String_Pool& string_pool, ByteReader& cfs,
                                         uint16 len)
{
    // buffer ends before len
    if(!cfs.have(len))
        return false;

    // get utf8 bytes and move buffer pointer
    const char* utf8data = (const char*)cfs.get_and_skip(len);
    
    // FIXME: decode 6-byte Java 1.5 encoding

    // check utf8 correctness
    if(memchr(utf8data, 0, len) != NULL)
        return false;

    // then lookup on utf8 bytes and return string
    return string_pool.lookup(utf8data, len);
}


static String* class_file_parse_utf8(String_Pool& string_pool,
                                     ByteReader& cfs)
{
    uint16 len;
    if(!cfs.parse_u2_be(&len))
        return false;

    return class_file_parse_utf8data(string_pool, cfs, len);
}


bool ConstantPool::parse(Class* clss,
                         String_Pool& string_pool,
                         ByteReader& cfs)
{
    if(!cfs.parse_u2_be(&m_size)) {
        REPORT_FAILED_CLASS_CLASS(clss->get_class_loader(), clss, "java/lang/ClassFormatError",
            clss->get_name()->bytes << ": could not parse constant pool size");
        return false;
    }

    unsigned char* cp_tags = new unsigned char[m_size];
    // ppervov: FIXME: should throw OOME
    m_entries = new ConstPoolEntry[m_size];
    // ppervov: FIXME: should throw OOME

    //
    // 0'th constant pool entry is a pointer to the tags array
    //
    m_entries[0].tags = cp_tags;
    cp_tags[0] = CONSTANT_Tags;
    for(unsigned i = 1; i < m_size; i++) {
        // parse tag into tag array
        uint8 tag;
        if(!cfs.parse_u1(&tag)) {
            REPORT_FAILED_CLASS_CLASS(clss->get_class_loader(), clss, "java/lang/ClassFormatError",
                clss->get_name()->bytes << ": could not parse constant pool tag for index " << i);
            return false;
        }

        switch(cp_tags[i] = tag) {
            case CONSTANT_Class:
                if(!cfs.parse_u2_be(&m_entries[i].CONSTANT_Class.name_index)) {
                    REPORT_FAILED_CLASS_CLASS(clss->get_class_loader(), clss, "java/lang/ClassFormatError",
                        clss->get_name()->bytes << ": could not parse name index "
                        "for CONSTANT_Class entry");
                    return false;
                }
                break;

            case CONSTANT_Methodref:
            case CONSTANT_Fieldref:
            case CONSTANT_InterfaceMethodref:
                if(!cfs.parse_u2_be(&m_entries[i].CONSTANT_ref.class_index)) {
                    REPORT_FAILED_CLASS_CLASS(clss->get_class_loader(), clss, "java/lang/ClassFormatError",
                        clss->get_name()->bytes << ": could not parse class index for CONSTANT_*ref entry");
                    return false;
                }
                if(!cfs.parse_u2_be(&m_entries[i].CONSTANT_ref.name_and_type_index)) {
                    REPORT_FAILED_CLASS_CLASS(clss->get_class_loader(), clss, "java/lang/ClassFormatError",
                        clss->get_name()->bytes << ": could not parse name-and-type index for CONSTANT_*ref entry");
                    return false;
                }
                break;

            case CONSTANT_String:
                if(!cfs.parse_u2_be(&m_entries[i].CONSTANT_String.string_index)) {
                    REPORT_FAILED_CLASS_CLASS(clss->get_class_loader(), clss, "java/lang/ClassFormatError",
                        clss->get_name()->bytes << ": could not parse string index for CONSTANT_String entry");
                    return false;
                }
                break;

            case CONSTANT_Float:
            case CONSTANT_Integer:
                if(!cfs.parse_u4_be(&m_entries[i].int_value)) {
                    REPORT_FAILED_CLASS_CLASS(clss->get_class_loader(), clss, "java/lang/ClassFormatError",
                        clss->get_name()->bytes << ": could not parse value for "
                        << (tag==CONSTANT_Integer?"CONSTANT_Integer":"CONSTANT_Float") << " entry");
                    return false;
                }
                break;

            case CONSTANT_Double:
            case CONSTANT_Long:
                // longs and doubles take up two entries
                // on both IA32 & IPF, first constant pool element is used, second element - unused
                if(!cfs.parse_u4_be(&m_entries[i].CONSTANT_8byte.high_bytes)) {
                    REPORT_FAILED_CLASS_CLASS(clss->get_class_loader(), clss, "java/lang/ClassFormatError",
                        clss->get_name()->bytes << ": could not parse high four bytes for "
                        << (tag==CONSTANT_Long?"CONSTANT_Integer":"CONSTANT_Float") << " entry");
                    return false;
                }
                if(!cfs.parse_u4_be(&m_entries[i].CONSTANT_8byte.low_bytes)) {
                    REPORT_FAILED_CLASS_CLASS(clss->get_class_loader(), clss, "java/lang/ClassFormatError",
                        clss->get_name()->bytes << ": could not parse low four bytes for "
                        << (tag==CONSTANT_Long?"CONSTANT_Long":"CONSTANT_Double") << " entry");
                    return false;
                }
                // skip next constant pool entry as it is used by next 4 bytes of Long/Double
                cp_tags[i+1] = cp_tags[i];
                i++;
                break;

            case CONSTANT_NameAndType:
                if(!cfs.parse_u2_be(&m_entries[i].CONSTANT_NameAndType.name_index)) {
                    REPORT_FAILED_CLASS_CLASS(clss->get_class_loader(), clss, "java/lang/ClassFormatError",
                        clss->get_name()->bytes << ": could not parse name index "
                        "for CONSTANT_NameAndType entry");
                    return false;
                }
                if(!cfs.parse_u2_be(&m_entries[i].CONSTANT_NameAndType.descriptor_index)) {
                    REPORT_FAILED_CLASS_CLASS(clss->get_class_loader(), clss, "java/lang/ClassFormatError",
                        clss->get_name()->bytes << ": could not parse descriptor index "
                        "for CONSTANT_NameAndType entry");
                    return false;
                }
                break;

            case CONSTANT_Utf8:
                {
                    // parse and insert string into string table
                    String* str = class_file_parse_utf8(string_pool, cfs);
                    if(!str) {
                        REPORT_FAILED_CLASS_CLASS(clss->get_class_loader(), clss, "java/lang/ClassFormatError",
                            clss->get_name()->bytes << ": could not parse CONTANT_Utf8 entry");
                        return false;
                    }
                    m_entries[i].CONSTANT_Utf8.string = str;
                }
                break;
            default:
                REPORT_FAILED_CLASS_CLASS(clss->get_class_loader(), clss, "java/lang/ClassFormatError",
                    clss->get_name()->bytes << ": unknown constant pool tag " << cp_tags[i]);
                return false;
        }
    }
    return true;
} // ConstantPool::parse


bool ConstantPool::check(Class* clss)
{
    for(unsigned i = 1; i < m_size; i++) {
        switch(get_tag(i))
        {
        case CONSTANT_Class:
        {
            unsigned name_index = get_class_name_index(i);
            if (!is_valid_index(name_index) || !is_utf8(name_index)) {
                // illegal name index
                REPORT_FAILED_CLASS_CLASS(clss->get_class_loader(), clss, "java/lang/ClassFormatError",
                    clss->get_name()->bytes << ": wrong name index for CONSTANT_Class entry");
                return false;
            }
            break;
        }
        case CONSTANT_Methodref:
        case CONSTANT_Fieldref:
        case CONSTANT_InterfaceMethodref:
        {
            unsigned class_index = get_ref_class_index(i);
            unsigned name_type_index = get_ref_name_and_type_index(i);
            if (!is_valid_index(class_index) || !is_class(class_index)) {
                REPORT_FAILED_CLASS_CLASS(clss->get_class_loader(), clss, "java/lang/ClassFormatError",
                    clss->get_name()->bytes << ": wrong class index for CONSTANT_*ref entry");
                return false;
            }
            if (!is_valid_index(name_type_index) || !is_name_and_type(name_type_index)) {
                REPORT_FAILED_CLASS_CLASS(clss->get_class_loader(), clss, "java/lang/ClassFormatError",
                    clss->get_name()->bytes << ": wrong name-and-type index for CONSTANT_*ref entry");
                return false;
            }
            break;
        }
        case CONSTANT_String:
        {
            unsigned string_index = get_string_index(i);
            if (!is_valid_index(string_index) || !is_utf8(string_index)) {
                // illegal string index
                REPORT_FAILED_CLASS_CLASS(clss->get_class_loader(), clss, "java/lang/ClassFormatError",
                    clss->get_name()->bytes << ": wrong string index for CONSTANT_String entry");
                return false;
            }
            // set entry to the actual string
            resolve_entry(i, get_utf8_string(string_index));
            break;
        }
        case CONSTANT_Integer:
        case CONSTANT_Long:
        case CONSTANT_Float:
        case CONSTANT_Double:
            // not much to do here
            break;
        case CONSTANT_NameAndType:
        {
            unsigned name_index = get_name_and_type_name_index(i);
            unsigned descriptor_index = get_name_and_type_descriptor_index(i);
            if(!is_valid_index(name_index) || !is_utf8(name_index)) {
                REPORT_FAILED_CLASS_CLASS(clss->get_class_loader(), clss, "java/lang/ClassFormatError",
                    clss->get_name()->bytes << ": wrong name index for CONSTANT_NameAndType entry");
                return false;
            }
            if (!is_valid_index(descriptor_index) || !is_utf8(descriptor_index)) {
                REPORT_FAILED_CLASS_CLASS(clss->get_class_loader(), clss, "java/lang/ClassFormatError",
                    clss->get_name()->bytes << ": wrong descriptor index for CONSTANT_NameAndType entry");
                return false;
            }
            resolve_entry(i, get_utf8_string(name_index), get_utf8_string(descriptor_index));
            break;
        }
        case CONSTANT_Utf8:
            // nothing to do here
            break;
        default:
            REPORT_FAILED_CLASS_CLASS(clss->get_class_loader(), clss, "java/lang/ClassFormatError",
                clss->get_name()->bytes << ": wrong constant pool tag " << get_tag(i));
            return false;
        }
    }
    return true;
} // ConstantPool::check


bool Class::parse_interfaces(ByteReader &cfs)
{
    if(!cfs.parse_u2_be(&m_num_superinterfaces)) {
        REPORT_FAILED_CLASS_CLASS(m_class_loader, this, "java/lang/ClassFormatError",
            get_name()->bytes << ": could not parse number of superinterfaces");
        return false;
    }

    m_superinterfaces = (Class_Super*)m_class_loader->
        Alloc(sizeof(Class_Super)*m_num_superinterfaces);
    // ppervov: FIXME: should throw OOME
    for (unsigned i=0; i<m_num_superinterfaces; i++) {
        uint16 interface_index;
        if(!cfs.parse_u2_be(&interface_index)) {
            REPORT_FAILED_CLASS_CLASS(m_class_loader, this, "java/lang/ClassFormatError",
                get_name()->bytes << ": could not parse superinterface index");
            return false;
        }
        //
        // verify that entry in constant pool is of type CONSTANT_Class
        //
        m_superinterfaces[i].name = cp_check_class(m_const_pool, interface_index);
        if(m_superinterfaces[i].name == NULL) {
            REPORT_FAILED_CLASS_CLASS(m_class_loader, this, "java/lang/ClassFormatError",
                get_name()->bytes << ": constant pool index "
                << i << " is not CONSTANT_Class entry"
                " while parsing superinterfaces");
            return false;
        }
        m_superinterfaces[i].cp_index = interface_index;
    }
    return true;
} // Class::parse_interfaces


//
// magic number, and major/minor version numbers of class file
//
#define CLASSFILE_MAGIC 0xCAFEBABE
#define CLASSFILE_MAJOR 45
// Supported class files up to this version
#define CLASSFILE_MAJOR_MAX 49
#define CLASSFILE_MINOR 3

/*
 *  Parses and verifies the classfile. Format is (from JVM spec):
 * 
 *    ClassFile {
 *      u4 magic;
 *      u2 minor_version;
 *      u2 major_version;
 *      u2 constant_pool_count;
 *      cp_info constant_pool[constant_pool_count-1];
 *      u2 access_flags;
 *      u2 this_class;
 *      u2 super_class;
 *      u2 interfaces_count;
 *      u2 interfaces[interfaces_count];
 *      u2 fields_count;
 *      field_info fields[fields_count];
 *      u2 methods_count;
 *      method_info methods[methods_count];
 *      u2 attributes_count;
 *      attribute_info attributes[attributes_count];
 *   }
 */
bool Class::parse(Global_Env* env,
                  ByteReader& cfs)
{
    /*
     *  get and check magic number (Oxcafebabe)
     */
    uint32 magic;
    if (!cfs.parse_u4_be(&magic)) {
        REPORT_FAILED_CLASS_CLASS(m_class_loader, this, "java/lang/ClassFormatError",
            "class is not a valid Java class file");
        return false;
    }

    if (magic != CLASSFILE_MAGIC) {
        REPORT_FAILED_CLASS_CLASS(m_class_loader, this, "java/lang/ClassFormatError",
            "invalid magic");
        return false;
    }

    /*
     *  get and check major/minor version of classfile
     *  1.1 (45.0-3) 1.2 (46.???) 1.3 (47.???) 1.4 (48.?) 5 (49.0)
     */
    uint16 minor_version;
    if (!cfs.parse_u2_be(&minor_version)) {
        REPORT_FAILED_CLASS_CLASS(m_class_loader, this, "java/lang/ClassFormatError",
            "could not parse minor version");
        return false;
    }

    if (!cfs.parse_u2_be(&m_version)) {
        REPORT_FAILED_CLASS_CLASS(m_class_loader, this, "java/lang/ClassFormatError",
            "could not parse major version");
        return false;
    }

    if (!(m_version >= CLASSFILE_MAJOR
        && m_version <= CLASSFILE_MAJOR_MAX))
    {
        REPORT_FAILED_CLASS_CLASS(m_class_loader, this, "java/lang/UnsupportedClassVersionError",
            "class has version number " << m_version);
        return false;
    }

    /*
     *  allocate and parse constant pool
     */
    if(!m_const_pool.parse(this, env->string_pool, cfs))
        return false;

    /*
     * check and preprocess the constant pool
     */
    if(!m_const_pool.check(this))
        return false;

    if(!cfs.parse_u2_be(&m_access_flags)) {
        REPORT_FAILED_CLASS_CLASS(m_class_loader, this, "java/lang/ClassFormatError",
            m_name->bytes << ": could not parse access flags");
        return false;
    }

    if(is_interface()) {
        // NOTE: Fix for the statement that an interface should have
        // abstract flag set.
        // spec/harness/BenchmarkDone has interface flag, but it does not 
        // have abstract flag.
        m_access_flags |= ACC_ABSTRACT;
    }

    /*
     * parse this_class & super_class & verify their constant pool entries
     */
    uint16 this_class;
    if (!cfs.parse_u2_be(&this_class)) {
        REPORT_FAILED_CLASS_CLASS(m_class_loader, this, "java/lang/ClassFormatError",
            m_name->bytes << ": could not parse this class index");
        return false;
    }

    String * class_name = cp_check_class(m_const_pool, this_class);
    if (class_name == NULL) {
        REPORT_FAILED_CLASS_CLASS(m_class_loader, this, "java/lang/ClassFormatError",
            m_name->bytes << ": this_class constant pool entry "
            << this_class << " is an illegal CONSTANT_Class entry");
        return false;
    }

    /*
     * When defineClass from byte stream, there are cases that clss->name is null,
     * so we should add a check here
     */
    if(m_name != NULL && class_name != m_name) { 
        REPORT_FAILED_CLASS_CLASS(m_class_loader, this,
            VM_Global_State::loader_env->JavaLangNoClassDefFoundError_String->bytes,
            m_name->bytes << ": class name in class data does not match class name passed");
        return false;
    }

    if(m_name == NULL) {
        m_name = class_name;
    }

    /*
     *  Mark the current class as resolved.
     */
    m_const_pool.resolve_entry(this_class, this);

    /*
     * parse the super class name
     */
    uint16 super_class;
    if (!cfs.parse_u2_be(&super_class)) {
        REPORT_FAILED_CLASS_CLASS(m_class_loader, this, "java/lang/ClassFormatError",
            m_name->bytes << ": could not parse super class index");
        return false;
    }

    m_super_class.cp_index = super_class;
    if (super_class == 0) {
        //
        // this class must represent java.lang.Object
        //
        if(m_name != env->JavaLangObject_String) {
            REPORT_FAILED_CLASS_CLASS(m_class_loader, this, "java/lang/ClassFormatError",
                m_name->bytes << ": class does not contain super class "
                << "but is not java.lang.Object class");
            return false;
        }
        m_super_class.name = NULL;
    } else {
        m_super_class.name = cp_check_class(m_const_pool, super_class);
        if (m_super_class.name == NULL) {
            REPORT_FAILED_CLASS_CLASS(m_class_loader, this, "java/lang/ClassFormatError",
                m_name->bytes << ": super_class constant pool entry "
                << super_class << " is an illegal CONSTANT_Class entry");
            return false;
        }
    }

    /*
     * allocate and parse class' interfaces
     */
    if(!parse_interfaces(cfs))
        return false;

    /* 
     *  allocate and parse class' fields
     */
    if(!parse_fields(env, cfs))
        return false;

    /* 
     *  allocate and parse class' methods
     */
    if(!parse_methods(env, cfs))
        return false;

    /*
     *  only the FileName attribute is defined for Class
     */
    uint16 n_attrs;
    if (!cfs.parse_u2_be(&n_attrs)) {
        REPORT_FAILED_CLASS_CLASS(m_class_loader, this, "java/lang/ClassFormatError",
            m_name->bytes << ": could not parse number of attributes");
        return false;
    }

    unsigned n_source_file_attr = 0;
    unsigned numSourceDebugExtensions = 0;
    unsigned numEnclosingMethods = 0;
    uint32 attr_len = 0;

    for (unsigned i=0; i<n_attrs; i++) {
        Attributes cur_attr = parse_attribute(cfs, m_const_pool, class_attr_strings, class_attrs, &attr_len);
        switch(cur_attr){
        case ATTR_SourceFile:
        {
            // a class file can have at most one source file attribute
            if (++n_source_file_attr > 1) {
                REPORT_FAILED_CLASS_CLASS(m_class_loader, this, "java/lang/ClassFormatError",
                    m_name->bytes << ": there is more than one SourceFile attribute");
                return false;
            }

            // attribute length must be two (vm spec 4.7.2)
            if (attr_len != 2) {
                REPORT_FAILED_CLASS_CLASS(m_class_loader, this, "java/lang/ClassFormatError",
                    m_name->bytes << ": SourceFile attribute has incorrect length ("
                    << attr_len << " bytes, should be 2 bytes)");
                return false;
            }

            // constant value attribute
            uint16 filename_index;
            if(!cfs.parse_u2_be(&filename_index)) {
                REPORT_FAILED_CLASS_CLASS(m_class_loader, this, "java/lang/ClassFormatError",
                    m_name->bytes << ": could not parse filename index"
                    << " while parsing SourceFile attribute");
                return false;
            }

            m_src_file_name = cp_check_utf8(m_const_pool, filename_index);
            if(m_src_file_name == NULL) {
                REPORT_FAILED_CLASS_CLASS(m_class_loader, this, "java/lang/ClassFormatError",
                    m_name->bytes << ": filename index points to incorrect constant pool entry"
                    << " while parsing SourceFile attribute");
                return false;
            }
            break;
        }

        case ATTR_InnerClasses:
        {
            if (m_declaring_class_index || m_innerclasses) {
                REPORT_FAILED_CLASS_FORMAT(this, "more than one InnerClasses attribute");
            }
            bool isinner = false;
            // found_myself == 2: myself is not inner class or has passed myself when iterating inner class attribute arrays
            // found_myself == 1: myself is inner class, current index of inner class attribute arrays is just myself
            // found_myself == 0: myself is inner class, hasn't met myself in inner class attribute arrays
            int found_myself = 2;
            if(strchr(m_name->bytes, '$')){
                isinner = true;
                found_myself = 0;
            }
            //Only handle inner class
            uint16 num_of_classes;
            if(!cfs.parse_u2_be(&num_of_classes)) {
                REPORT_FAILED_CLASS_CLASS(m_class_loader, this, "java/lang/ClassFormatError",
                    m_name->bytes << ": could not parse number of classes"
                    << " while parsing InnerClasses attribute");
                return false;
            }

            if(isinner)
                m_num_innerclasses = (uint16)(num_of_classes - 1); //exclude itself
            else
                m_num_innerclasses = num_of_classes;
            if(num_of_classes)
                m_innerclasses = (InnerClass*) m_class_loader->
                    Alloc(2*sizeof(InnerClass)*m_num_innerclasses);
                // ppervov: FIXME: should throw OOME
            int index = 0;
            for(int i = 0; i < num_of_classes; i++){
                uint16 inner_clss_info_idx;
                if(!cfs.parse_u2_be(&inner_clss_info_idx)) {
                    REPORT_FAILED_CLASS_CLASS(m_class_loader, this, "java/lang/ClassFormatError",
                        m_name->bytes << ": could not parse inner class info index"
                        << " while parsing InnerClasses attribute");
                    return false;
                }
                if(inner_clss_info_idx
                    && !valid_cpi(this, inner_clss_info_idx, CONSTANT_Class))
                {
                    REPORT_FAILED_CLASS_CLASS(m_class_loader, this, "java/lang/ClassFormatError",
                        m_name->bytes << ": inner class info index points to incorrect constant pool entry"
                        << " while parsing InnerClasses attribute");
                    return false;
                }

                if(!found_myself){
                    String* clssname = cp_check_class(m_const_pool, inner_clss_info_idx);
                    // Only handle this class
                    if(m_name == clssname)
                        found_myself = 1;
                }
                if(found_myself != 1)
                    m_innerclasses[index].index = inner_clss_info_idx;

                uint16 outer_clss_info_idx;
                if(!cfs.parse_u2_be(&outer_clss_info_idx)) {
                    REPORT_FAILED_CLASS_CLASS(m_class_loader, this, "java/lang/ClassFormatError",
                        m_name->bytes << ": could not parse outer class info index"
                        << " while parsing InnerClasses attribute");
                    return false;
                }
                if(outer_clss_info_idx
                    && !valid_cpi(this, outer_clss_info_idx, CONSTANT_Class))
                {
                    REPORT_FAILED_CLASS_CLASS(m_class_loader, this, "java/lang/ClassFormatError",
                        m_name->bytes << ": outer class info index points to incorrect constant pool entry"
                        << outer_clss_info_idx << " while parsing InnerClasses attribute");
                    return false;
                }
                if(found_myself == 1 && outer_clss_info_idx){
                    m_declaring_class_index = outer_clss_info_idx;
                }

                uint16 inner_name_idx;
                if(!cfs.parse_u2_be(&inner_name_idx)) {
                    REPORT_FAILED_CLASS_CLASS(m_class_loader, this, "java/lang/ClassFormatError",
                        m_name->bytes << ": could not parse inner name index"
                        << " while parsing InnerClasses attribute");
                    return false;
                }
                if(inner_name_idx && !valid_cpi(this, inner_name_idx, CONSTANT_Utf8))
                {
                    REPORT_FAILED_CLASS_FORMAT(this,
                        "inner name index points to incorrect constant pool entry");
                    return false;
                }
                if(found_myself == 1){
                    if (inner_name_idx) {
                        m_simple_name = m_const_pool.get_utf8_string(inner_name_idx);
                    } else {
                        //anonymous class
                        m_simple_name = env->string_pool.lookup("");
                    }
                }

                uint16 inner_clss_access_flag;
                if(!cfs.parse_u2_be(&inner_clss_access_flag)) {
                    REPORT_FAILED_CLASS_CLASS(m_class_loader, this, "java/lang/ClassFormatError",
                        m_name->bytes << ": could not parse inner class access flags"
                        << " while parsing InnerClasses attribute");
                    return false;
                }
                if(found_myself == 1) {
                    found_myself = 2;
                    m_access_flags = inner_clss_access_flag;
                } else
                    m_innerclasses[index++].access_flags = inner_clss_access_flag;
            } // for num_of_classes
        }break; //case ATTR_InnerClasses

        case ATTR_SourceDebugExtension:
            {
                // attribute length is already recorded in attr_len
                // now reading debug extension information
                if( ++numSourceDebugExtensions > 1 ) {
                    REPORT_FAILED_CLASS_CLASS(m_class_loader, this, "java/lang/ClassFormatError",
                        m_name->bytes << ": there is more than one SourceDebugExtension attribute");
                    return false;
                }

                // cfs is at debug_extension[] which is:
                //      The debug_extension array holds a string, which must be in UTF-8 format.
                //      There is no terminating zero byte.
                m_sourceDebugExtension = class_file_parse_utf8data(env->string_pool, cfs, attr_len);
                if(!m_sourceDebugExtension) {
                    REPORT_FAILED_CLASS_FORMAT(this, "invalid SourceDebugExtension attribute");
                    return false;
                }
            }
            break;

        case ATTR_EnclosingMethod:
            {
                if ( ++numEnclosingMethods > 1 ) {
                    REPORT_FAILED_CLASS_FORMAT(this, "more than one EnclosingMethod attribute");
                    return false;
                }
                if (attr_len != 4) {
                    REPORT_FAILED_CLASS_FORMAT(this,
                        "unexpected length of EnclosingMethod attribute: " << attr_len);
                    return false;
                }
                uint16 class_idx;
                if(!cfs.parse_u2_be(&class_idx)) {
                    REPORT_FAILED_CLASS_FORMAT(this, 
                        "could not parse class index of EnclosingMethod attribute");
                    return false;
                }
                if(!valid_cpi(this, class_idx, CONSTANT_Class))
                {
                    REPORT_FAILED_CLASS_FORMAT(this, 
                        "incorrect class index of EnclosingMethod attribute");
                    return false;
                }
                m_enclosing_class_index = class_idx;

                uint16 method_idx;
                if(!cfs.parse_u2_be(&method_idx)) {
                    REPORT_FAILED_CLASS_FORMAT(this, 
                        "could not parse method index of EnclosingMethod attribute");
                    return false;
                }
                if(method_idx && !valid_cpi(this, method_idx, CONSTANT_NameAndType))
                {
                    REPORT_FAILED_CLASS_FORMAT(this, 
                        "incorrect method index of EnclosingMethod attribute");
                    return false;
                }
                m_enclosing_method_index = method_idx;
            }
            break;

        case ATTR_Synthetic:
            {
                if(attr_len != 0) {
                    REPORT_FAILED_CLASS_FORMAT(this,
                        "attribute Synthetic has non-zero length");
                    return false;
                }
                m_access_flags |= ACC_SYNTHETIC;
            }
            break;

        case ATTR_Deprecated:
            {
                if(attr_len != 0) {
                    REPORT_FAILED_CLASS_FORMAT(this,
                        "attribute Deprecated has non-zero length");
                    return false;
                }
                m_deprecated = true;
            }
            break;

        case ATTR_Signature:
            {
                if(m_signature != NULL) {
                    REPORT_FAILED_CLASS_FORMAT(this,
                        "more than one Signature attribute for the class");
                    return false;
                }
                if (!(m_signature = parse_signature_attr(cfs, attr_len, this))) {
                    return false;
                }
            }
            break;

        case ATTR_RuntimeVisibleAnnotations:
            {
                uint32 read_len = parse_annotation_table(&m_annotations, cfs, this);
                if (attr_len != read_len) {
                    REPORT_FAILED_CLASS_FORMAT(this, 
                        "error parsing Annotations attribute"
                        << "; declared length " << attr_len
                        << " does not match actual " << read_len);
                    return false;
                }
            }
            break;

        case ATTR_RuntimeInvisibleAnnotations:
            {
                if(!cfs.skip(attr_len)) {
                    REPORT_FAILED_CLASS_FORMAT(this,
                        "failed to skip RuntimeInvisibleAnnotations attribute");
                    return false;
                }
            }
            break;

        case ATTR_UNDEF:
            // unrecognized attribute; skipped
            break;
        default:
            // error occured
            REPORT_FAILED_CLASS_CLASS(m_class_loader, this, "java/lang/InternalError",
                m_name->bytes << ": unknown error occured"
                " while parsing attributes for class"  
                << "; unprocessed attribute " << cur_attr);
            return false;
        } // switch
    } // for

    if (cfs.have(1)) {
        REPORT_FAILED_CLASS_FORMAT(this, "Extra bytes at the end of class file");
        return false;
    }

    if (m_enclosing_class_index && m_simple_name == NULL) {
        WARN("Attention: EnclosingMethod attribute does not imply "
            "InnerClasses presence for class " << m_name->bytes);
    }

    /*
     *   can't be both final and interface, or both final and abstract
     */
    if(is_final() && is_interface())
    {
        REPORT_FAILED_CLASS_FORMAT(this, "interface cannot be final");
        return false;
    }
    
    if(is_final() && is_abstract()) {
        REPORT_FAILED_CLASS_FORMAT(this, "abstract class cannot be final");
        return false;
    }

    // This requirement seems to be ignored by some compilers
    // if(class_is_interface(clss) && !class_is_abstract(clss))
    //{
    //    REPORT_FAILED_CLASS_FORMAT(clss, "interface must have ACC_ABSTRACT flag set");
    //    return false;
    //}

    if(is_annotation() && !is_interface())
    {
        REPORT_FAILED_CLASS_FORMAT(this, "annotation type must be interface");
        return false;
    }

    return true;
} // Class::parse


static bool const_pool_find_entry(ByteReader& cp, uint16 cp_count, uint16 index)
{
    uint8 tag;
    // cp must be at the beginning of constant pool
    for(uint16 cp_index = 1; cp_index < cp_count; cp_index++) {
        if(cp_index == index) return true;
        if(!cp.parse_u1(&tag))
            return false;
        switch(tag) {
            case CONSTANT_Class:
                if(!cp.skip(2))
                    return false;
                break;
            case CONSTANT_Fieldref:
            case CONSTANT_Methodref:
            case CONSTANT_InterfaceMethodref:
                if(!cp.skip(4))
                    return false;
                break;
            case CONSTANT_String:
                if(!cp.skip(2))
                    return false;
                break;
            case CONSTANT_Integer:
            case CONSTANT_Float:
                if(!cp.skip(4))
                    return false;
                break;
            case CONSTANT_Long:
            case CONSTANT_Double:
                if(!cp.skip(8))
                    return false;
                cp_index++;
                break;
            case CONSTANT_NameAndType:
                if(!cp.skip(4))
                    return false;
                break;
            case CONSTANT_Utf8:
                {
                    uint16 dummy16;
                    if(!cp.parse_u2_be(&dummy16))
                        return false;
                    if(!cp.skip(dummy16))
                        return false;
                }
                break;
        }
    }

    return false; // not found
}


const String* class_extract_name(Global_Env* env,
                                 uint8* buffer, unsigned offset, unsigned length)
{
    ByteReader cfs(buffer, offset, length);

    uint32 magic;
    // check magic
    if(!cfs.parse_u4_be(&magic) || magic != CLASSFILE_MAGIC)
        return NULL;

    // skip minor_version and major_version
    if(!cfs.skip(4))
        return NULL;

    uint16 cp_count;
    // get constant pool entry number
    if(!cfs.parse_u2_be(&cp_count))
        return NULL;

    // skip constant pool
    uint8 tag;
    uint16 utf8_len;
    offset = cfs.get_offset(); // offset now contains the start of constant pool
    uint16 cp_index;
    for(cp_index = 1; cp_index < cp_count; cp_index++) {
        if(!cfs.parse_u1(&tag))
            return NULL;
        switch(tag) {
            case CONSTANT_Class:
                if(!cfs.skip(2))
                    return NULL;
                break;
            case CONSTANT_Fieldref:
            case CONSTANT_Methodref:
            case CONSTANT_InterfaceMethodref:
                if(!cfs.skip(4))
                    return NULL;
                break;
            case CONSTANT_String:
                if(!cfs.skip(2))
                    return NULL;
                break;
            case CONSTANT_Integer:
            case CONSTANT_Float:
                if(!cfs.skip(4))
                    return NULL;
                break;
            case CONSTANT_Long:
            case CONSTANT_Double:
                if(!cfs.skip(8))
                    return NULL;
                cp_index++;
                break;
            case CONSTANT_NameAndType:
                if(!cfs.skip(4))
                    return NULL;
                break;
            case CONSTANT_Utf8:
                if(!cfs.parse_u2_be(&utf8_len))
                    return NULL;
                if(!cfs.skip(utf8_len))
                    return NULL;
                break;
        }
    }

    // skip access_flags
    if(!cfs.skip(2))
        return NULL;

    // get this_index in constant pool
    uint16 this_class_idx;
    if(!cfs.parse_u2_be(&this_class_idx))
        return NULL;

    // find needed entry
    if(!cfs.go_to_offset(offset))
        return NULL;
    if(!const_pool_find_entry(cfs, cp_count, this_class_idx))
        return NULL;

    // now cfs is at CONSTANT_Class entry
    if(!cfs.parse_u1(&tag) && tag != CONSTANT_Class)
        return NULL;
    // set this_class_idx to class_name index in constant pool
    if(!cfs.parse_u2_be(&this_class_idx))
        return NULL;

    // find entry class_name
    if(!cfs.go_to_offset(offset))
        return NULL;
    if(!const_pool_find_entry(cfs, cp_count, this_class_idx))
        return NULL;

    // now cfs is at CONSTANT_Utf8 entry
    if(!cfs.parse_u1(&tag) && tag != CONSTANT_Utf8)
        return NULL;
    // parse class name
    const String* class_name = class_file_parse_utf8(env->string_pool, cfs);
    return class_name;
}

Class *class_load_verify_prepare_by_loader_jni(Global_Env* env,
                                               const String* classname,
                                               ClassLoader* cl)
{
    assert(hythread_is_suspend_enabled());
    // if no class loader passed, re-route to bootstrap
    if(!cl) cl = env->bootstrap_class_loader;
    Class* clss = cl->LoadVerifyAndPrepareClass(env, classname);
     return clss;
}


Class *class_load_verify_prepare_from_jni(Global_Env *env, const String *classname)
{
    assert(hythread_is_suspend_enabled());
    Class *clss = env->bootstrap_class_loader->LoadVerifyAndPrepareClass(env, classname);
    return clss;
}
