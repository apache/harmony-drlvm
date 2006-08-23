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
 * @version $Revision: 1.1.2.6.4.6 $
 */  

#define LOG_DOMAIN util::CLASS_LOGGER
#include "cxxlog.h"


#include "port_filepath.h"
#include <assert.h>

#include "environment.h"
#include "classloader.h"
#include "Class.h"
#include "vm_strings.h"
#include "open/vm_util.h"
#include "bytereader.h"
#include "compile.h"
#include "jit_intf_cpp.h"
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



String *cp_check_utf8(Const_Pool *cp,
                     unsigned cp_size,
                     unsigned utf8_index)
{
    if(utf8_index >= cp_size || !cp_is_utf8(cp, utf8_index)) {
        return NULL;
    }
    return cp[utf8_index].CONSTANT_Utf8.string;
} //cp_check_utf8



String *cp_check_class(Const_Pool *cp,
                      unsigned cp_size,
                      unsigned class_index)
{
    if (class_index >= cp_size || !cp_is_class(cp,class_index)) {
#ifdef _DEBUG
        WARN("cp_check_class: illegal const pool class index" << class_index);
#endif
        return NULL;
    }    
    return cp[cp[class_index].CONSTANT_Class.name_index].CONSTANT_Utf8.string;
} //cp_check_class



static String *field_attr_strings[N_FIELD_ATTR+1];
static Attributes field_attrs[N_FIELD_ATTR];

static String *method_attr_strings[N_METHOD_ATTR+1];
static Attributes method_attrs[N_METHOD_ATTR];

static String *class_attr_strings[N_CLASS_ATTR+1];
static Attributes class_attrs[N_CLASS_ATTR];

static String *code_attr_strings[N_CODE_ATTR+1];
static Attributes code_attrs[N_CODE_ATTR];

static String *must_recognize_attr_strings[4];
static Attributes must_recognize_attrs[3];

//
// initialize string pool by preloading it with commonly used strings
//
static bool preload_attrs(String_Pool& string_pool)
{
    method_attr_strings[0] = string_pool.lookup("Code");
    method_attrs[0] = ATTR_Code;

    method_attr_strings[1] = string_pool.lookup("Exceptions");
    method_attrs[1] = ATTR_Exceptions;

    method_attr_strings[2] = string_pool.lookup("Synthetic");
    method_attrs[2] = ATTR_Synthetic;

    method_attr_strings[3] = string_pool.lookup("Deprecated");
    method_attrs[3] = ATTR_Deprecated;

    method_attr_strings[4] = NULL;

    field_attr_strings[0] = string_pool.lookup("ConstantValue");
    field_attrs[0] = ATTR_ConstantValue;

    field_attr_strings[1] = string_pool.lookup("Synthetic");
    field_attrs[1] = ATTR_Synthetic;

    field_attr_strings[2] = string_pool.lookup("Deprecated");
    field_attrs[2] = ATTR_Deprecated;

    field_attr_strings[3] = NULL;

    class_attr_strings[0] = string_pool.lookup("SourceFile");
    class_attrs[0] = ATTR_SourceFile;

    class_attr_strings[1] = string_pool.lookup("InnerClasses");
    class_attrs[1] = ATTR_InnerClasses;

    class_attr_strings[2] = string_pool.lookup("SourceDebugExtension");
    class_attrs[2] = ATTR_SourceDebugExtension;

    class_attr_strings[3] = NULL;

    code_attr_strings[0] = string_pool.lookup("LineNumberTable");
    code_attrs[0] = ATTR_LineNumberTable;

    code_attr_strings[1] = string_pool.lookup("LocalVariableTable");
    code_attrs[1] = ATTR_LocalVariableTable;

    code_attr_strings[2] = NULL;

    must_recognize_attr_strings[0] = string_pool.lookup("Code");
    must_recognize_attrs[0] = ATTR_Code;

    must_recognize_attr_strings[1] = string_pool.lookup("ConstantValue");
    must_recognize_attrs[1] = ATTR_ConstantValue;

    must_recognize_attr_strings[2] = string_pool.lookup("Exceptions");
    must_recognize_attrs[2] = ATTR_Exceptions;

    must_recognize_attr_strings[3] = NULL;

    return true;
} //init_loader



Attributes parse_attribute(ByteReader &cfs,
                           Const_Pool *cp,
                           unsigned cp_size,
                           String *attr_strings[],
                           Attributes attrs[],
                           uint32 *attr_len)
{
    static bool UNUSED init = preload_attrs(VM_Global_State::loader_env->string_pool);

    uint16 attr_name_index;
    bool result = cfs.parse_u2_be(&attr_name_index);
    if (!result)
        return ATTR_ERROR;

    result = cfs.parse_u4_be(attr_len);
    if (!result)
        return ATTR_ERROR;

    String *attr_name = cp_check_utf8(cp,cp_size,attr_name_index);
    if (attr_name == NULL) {
#ifdef _DEBUG
        WARN("parse_attribute: illegal const pool attr_name_index");
#endif
        return ATTR_ERROR;
    }
    unsigned i;
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


void* Class_Member::Alloc(size_t size) {
    ClassLoader* cl = get_class()->class_loader;
    assert(cl);
    return cl->Alloc(size);
}


bool Class_Member::parse(Class *clss, Const_Pool *cp, unsigned cp_size, ByteReader &cfs)
{
    if (!cfs.parse_u2_be(&_access_flags)) {
        REPORT_FAILED_CLASS_CLASS(clss->class_loader, clss, "java/lang/ClassFormatError",
            clss->name->bytes << ": error parsing field/method access flags");
        return false;
    }

    _class = clss;
    uint16 name_index;
    if (!cfs.parse_u2_be(&name_index)) {
        REPORT_FAILED_CLASS_CLASS(clss->class_loader, clss, "java/lang/ClassFormatError",
            clss->name->bytes << ": error parsing field/method name");
        return false;
    }

    uint16 descriptor_index;
    if (!cfs.parse_u2_be(&descriptor_index)) {
        REPORT_FAILED_CLASS_CLASS(clss->class_loader, clss, "java/lang/ClassFormatError",
            clss->name->bytes << ": error parsing field/method descriptor");
        return false;
    }

    //
    // look up the name_index and descriptor_index 
    // utf8 string const pool entries
    //
    String *name = cp_check_utf8(cp,cp_size,name_index);
    String *descriptor = cp_check_utf8(cp,cp_size,descriptor_index);
    if (name == NULL || descriptor == NULL) {
        REPORT_FAILED_CLASS_CLASS(clss->class_loader, clss, "java/lang/ClassFormatError",
            clss->name->bytes << ": error parsing field/method: constant pool index " << name_index << " or " << descriptor_index
            << " is not CONSTANT_Utf8 entry");
        return false;
    } 
    _name = name;
    _descriptor = descriptor;
    return true;
} //Class_Member::parse


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
        // break; // exclude remark #111: statement is unreachable
    case '[':
        {
            // descriptor is checked in recursion
            if(!check_field_descriptor(descriptor + 1, next, is_void_legal ))
                return false;
            return true;
        }
    default:
        // bad Java descriptor
        return false;
    }
    // DIE( "unreachable code!" ); // exclude remark #111: statement is unreachable
}

bool Field::parse(Class *clss, Const_Pool *cp, unsigned cp_size, ByteReader &cfs)
{
    if(!Class_Member::parse(clss, cp, cp_size, cfs))
        return false;

    // check field descriptor
    const char *next;
    if(!check_field_descriptor(get_descriptor()->bytes, &next, false) || *next != '\0') {
        REPORT_FAILED_CLASS_CLASS(clss->class_loader, clss, "java/lang/ClassFormatError",
            clss->name->bytes << ": bad field descriptor");
        return false;
    }

    if((is_public() && is_protected() || is_protected() && is_private() || is_public() && is_private())
        || (is_final() && is_volatile())) {
        REPORT_FAILED_CLASS_CLASS(clss->class_loader, clss, "java/lang/ClassFormatError",
            clss->name->bytes << ": field " << get_name()->bytes << " has invalid combination of access flags");
        return false;
    }
    // check interface fields access flags
    if( class_is_interface(clss) ) {
        if(!(is_public() && is_static() && is_final()) ||
            is_private() || is_protected() || is_volatile() || is_transient())
        {
            REPORT_FAILED_CLASS_CLASS(clss->class_loader, clss, "java/lang/ClassFormatError",
                clss->name->bytes << ": interface field "
                << get_descriptor()->bytes << " "
                << clss->name->bytes << "." << get_name()->bytes
                << " does not have one of ACC_PUBLIC, ACC_STATIC, or ACC_FINAL access flags set");
            return false;
        }
    }

    uint16 attr_count;
    if(!cfs.parse_u2_be(&attr_count)) {
        REPORT_FAILED_CLASS_CLASS(clss->class_loader, clss, "java/lang/ClassFormatError",
            clss->name->bytes << ": could not parse attribute count for field " << get_name());
        return false;
    }

    _offset_computed = 0;

    unsigned n_constval_attr = 0;

    uint32 attr_len = 0;

    for (unsigned j=0; j<attr_count; j++) {
        switch (parse_attribute(cfs, cp, cp_size, field_attr_strings, field_attrs, &attr_len)){
        case ATTR_ConstantValue:
        {    // constant value attribute
            // JVM spec (4.7.2) says that we should silently ignore the
            // ConstantValue attribute for non-static fields.

            // a field can have at most 1 ConstantValue attribute
            if (++n_constval_attr > 1) {
                REPORT_FAILED_CLASS_CLASS(clss->class_loader, clss, "java/lang/ClassFormatError",
                    clss->name->bytes << ": field " << get_name() << " has more then one ConstantValue attribute");
                return false;
            }
            // attribute length must be two (vm spec reference 4.7.3)
            if (attr_len != 2) {
                REPORT_FAILED_CLASS_CLASS(clss->class_loader, clss, "java/lang/ClassFormatError",
                    clss->name->bytes << ": ConstantValue attribute has invalid length for field " << get_name());
                return false;
            }

            if(!cfs.parse_u2_be(&_const_value_index)) {
                REPORT_FAILED_CLASS_CLASS(clss->class_loader, clss, "java/lang/ClassFormatError",
                    clss->name->bytes << ": could not parse ConstantValue index for field " << get_name());
                return false;
            }

            if(_const_value_index == 0 || _const_value_index >= cp_size) {
                REPORT_FAILED_CLASS_CLASS(clss->class_loader, clss, "java/lang/ClassFormatError",
                    clss->name->bytes << ": invalid ConstantValue index for field " << get_name());
                return false;
            }
            // type of constant must match field's type
            Const_Pool_Tags tag = (Const_Pool_Tags)cp_tag(cp, _const_value_index);

            Java_Type java_type = get_java_type();

            switch(tag) {
            case CONSTANT_Long:
                {
                    if (java_type != JAVA_TYPE_LONG) {
                        REPORT_FAILED_CLASS_CLASS(clss->class_loader, clss, "java/lang/ClassFormatError",
                            clss->name->bytes
                            << ": data type CONSTANT_Long of ConstantValue does not correspond to the type of field "
                            << get_name());
                        return false;
                    }
                const_value.l.lo_bytes = cp[_const_value_index].CONSTANT_8byte.low_bytes;
                const_value.l.hi_bytes = cp[_const_value_index].CONSTANT_8byte.high_bytes;
                break;
                }
            case CONSTANT_Float:
                {
                    if (java_type != JAVA_TYPE_FLOAT) {
                        REPORT_FAILED_CLASS_CLASS(clss->class_loader, clss, "java/lang/ClassFormatError",
                            clss->name->bytes
                            << ": data type CONSTANT_Float of ConstantValue does not correspond to the type of field "
                            << get_name());
                        return false;
                    }
                const_value.f = cp[_const_value_index].float_value;
                break;
                }
            case CONSTANT_Double:
                {
                    if (java_type != JAVA_TYPE_DOUBLE) {
                        REPORT_FAILED_CLASS_CLASS(clss->class_loader, clss, "java/lang/ClassFormatError",
                            clss->name->bytes
                            << ": data type CONSTANT_Double of ConstantValue does not correspond to the type of field "
                            << get_name());
                        return false;
                    }
                const_value.l.lo_bytes = cp[_const_value_index].CONSTANT_8byte.low_bytes;
                const_value.l.hi_bytes = cp[_const_value_index].CONSTANT_8byte.high_bytes;
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
                        REPORT_FAILED_CLASS_CLASS(clss->class_loader, clss, "java/lang/ClassFormatError",
                            clss->name->bytes
                            << ": data type CONSTANT_Integer of ConstantValue does not correspond to the type of field "
                            << get_name());
                        return false;
                    }
                const_value.i = cp[_const_value_index].int_value;
                break;
                }
            case CONSTANT_String:
                {
                    if (java_type != JAVA_TYPE_CLASS) {
                        REPORT_FAILED_CLASS_CLASS(clss->class_loader, clss, "java/lang/ClassFormatError",
                            clss->name->bytes
                            << ": data type CONSTANT_String of ConstantValue does not correspond to the type of field "
                            << get_name());
                        return false;
                    }
                const_value.string = cp[_const_value_index].CONSTANT_String.string;
                break;
                }
            default:
                {
                    REPORT_FAILED_CLASS_CLASS(clss->class_loader, clss, "java/lang/ClassFormatError",
                        clss->name->bytes
                        << ": invalid data type tag of ConstantValue for field " << get_name());
                    return false;
                }
            }
            break;
        }
        case ATTR_Synthetic:
            if(attr_len != 0) {
                REPORT_FAILED_CLASS_CLASS(clss->class_loader, clss, "java/lang/ClassFormatError",
                    clss->name->bytes
                    << ": invalid Synthetic attribute length for field "
                    << get_name());
                return false;
            }
            _synthetic = true;
            break;
        case ATTR_Deprecated:
            if(attr_len != 0) {
                REPORT_FAILED_CLASS_CLASS(clss->class_loader, clss, "java/lang/ClassFormatError",
                    clss->name->bytes
                    << ": invalid Deprecated attribute length for field "
                    << get_name());
                return false;
            }
            _deprecated = true;
            break;
        case ATTR_UNDEF:
            // unrecognized attribute; skipped
            break;
        default:
            // error occured
            REPORT_FAILED_CLASS_CLASS(clss->class_loader, clss, "java/lang/InternalError",
                clss->name->bytes
                << ": error parsing attributes for field "
                << get_name());
            return false;
        } // switch
    } // for

    TypeDesc* td = type_desc_create_from_java_descriptor(get_descriptor()->bytes, clss->class_loader);
    if( td == NULL ) {
        // ppervov: the fact we don't have td indicates we could not allocate one
        //std::stringstream ss;
        //ss << clss->name->bytes << ": could not create type descriptor for field " << get_name();
        //jthrowable exn = exn_create("java/lang/OutOfMemoryError", ss.str().c_str());
        exn_raise_only(VM_Global_State::loader_env->java_lang_OutOfMemoryError);
        return false;
    }
    set_field_type_desc(td);

    return true;
} //Field::parse


bool Handler::parse(Const_Pool* cp, unsigned cp_size, unsigned code_length,
                    ByteReader &cfs)
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
        _catch_type = cp_check_class(cp,cp_size,catch_index);
        if (_catch_type == NULL)
            return false;
    }
    return true;
} //Handler::parse

int Method::get_line_number(uint16 bc) {
    int line = -1;
    if (_line_number_table) {
        uint16 prev = 0;
        for (int i = _line_number_table->length - 1; i >= 0; --i){
            uint16 start = _line_number_table->table[i].start_pc;
            if (bc >= start && start >= prev) {
                prev = start;
                line = _line_number_table->table[i].line_number;
            }
        }
    }

    return line;
}

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
                         jint* length, jint* slot, String** name, String** type) {

    if (_line_number_table && index < _local_vars_table->length) {
        *pc = _local_vars_table->table[index].start_pc;
        *length = _local_vars_table->table[index].length;
        *slot = _local_vars_table->table[index].index;
        *name = _local_vars_table->table[index].name;
        *type = _local_vars_table->table[index].type;
        return true;
    } else {
        return false;
    }
}

#define REPORT_FAILED_METHOD(msg) REPORT_FAILED_CLASS_CLASS(_class->class_loader, \
    _class, "java/lang/ClassFormatError", \
    _class->name->bytes << " : " << msg << " for method "\
    << _name->bytes << _descriptor->bytes);


bool Method::_parse_exceptions(Const_Pool *cp, unsigned cp_size, unsigned attr_len,
                               ByteReader &cfs)
{
    if(!cfs.parse_u2_be(&_n_exceptions)) {
        REPORT_FAILED_CLASS_CLASS(_class->class_loader, _class, "java/lang/ClassFormatError",
            _class->name->bytes << ": could not parse number of exceptions for method "
            << _name->bytes << _descriptor->bytes);
        return false;
    }

    _exceptions = new String*[_n_exceptions];
    for (unsigned i=0; i<_n_exceptions; i++) {
        uint16 index;
        if(!cfs.parse_u2_be(&index)) {
            REPORT_FAILED_CLASS_CLASS(_class->class_loader, _class, "java/lang/ClassFormatError",
                _class->name->bytes << ": could not parse exception class index "
                << "while parsing excpetions for method "
                << _name->bytes << _descriptor->bytes);
            return false;
        }

        _exceptions[i] = cp_check_class(cp,cp_size,index);
        if (_exceptions[i] == NULL) {
            REPORT_FAILED_CLASS_CLASS(_class->class_loader, _class, "java/lang/ClassFormatError",
                _class->name->bytes << ": exception class index "
                << index << "is not a valid CONSTANT_class entry "
                << "while parsing excpetions for method "
                << _name->bytes << _descriptor->bytes);
            return false;
        }
    }
    if (attr_len != _n_exceptions * sizeof(uint16) + sizeof(_n_exceptions) ) {
        REPORT_FAILED_CLASS_CLASS(_class->class_loader, _class, "java/lang/ClassFormatError",
            _class->name->bytes << ": invalid Exceptions attribute length "
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

bool Method::_parse_local_vars(Const_Pool *cp, unsigned cp_size, 
                               unsigned attr_len, ByteReader &cfs) {

    uint16 n_local_vars;
    if(!cfs.parse_u2_be(&n_local_vars)) {
        REPORT_FAILED_METHOD("could not parse local variables number "
            "while parsing LocalVariableTable attribute");
        return false;
    }
    unsigned real_lnt_attr_len = 2 + n_local_vars * 10; 
    if(real_lnt_attr_len != attr_len) {
        REPORT_FAILED_METHOD("real LocalVariableTable length differ "
            "from attribute_length ("
            << attr_len << " vs. " << real_lnt_attr_len << ")" );
        return false;
    }

    _local_vars_table =
        (Local_Var_Table *)STD_MALLOC(sizeof(Local_Var_Table) +
        sizeof(Local_Var_Entry) * (n_local_vars - 1));
    // ppervov: FIXME: should throw OOME
    _local_vars_table->length = n_local_vars;

    for (unsigned j = 0; j < n_local_vars; j++) {
        uint16 start_pc;    
        if(!cfs.parse_u2_be(&start_pc)) {
            REPORT_FAILED_METHOD("could not parse start pc "
                "while parsing LocalVariableTable attribute");
            return false;
        }
        uint16 length;      
        if(!cfs.parse_u2_be(&length)) {
            REPORT_FAILED_METHOD("could not parse length "
                << _name->bytes << _descriptor->bytes);
            return false;
        }

        if( (start_pc >= _byte_code_length)
            || (start_pc + (unsigned)length) > _byte_code_length ) {
            REPORT_FAILED_METHOD("local var code range "
                "[start_pc, start_pc + length] points outside bytecode "
                "while parsing LocalVariableTable attribute");
            return false;
        }

        uint16 name_index;
        if(!cfs.parse_u2_be(&name_index)) {
            REPORT_FAILED_METHOD("could not parse name index "
                "while parsing LocalVariableTable attribute");
            return false;
        }

        uint16 descriptor_index;
        if(!cfs.parse_u2_be(&descriptor_index)) {
            REPORT_FAILED_METHOD("could not parse descriptor index "
                "while parsing LocalVariableTable attribute");
            return false;
        }

        String* name = cp_check_utf8(cp,cp_size,name_index);
        if(name == NULL) {
            REPORT_FAILED_METHOD("name index is not valid CONSTANT_Utf8 entry "
                "while parsing LocalVariableTable attribute");
            return false;
        }

        String* descriptor = cp_check_utf8(cp,cp_size,descriptor_index);
        if(descriptor == NULL) {
            REPORT_FAILED_METHOD("descriptor index is not valid CONSTANT_Utf8 entry "
                "while parsing LocalVariableTable attribute");
            return false;
        }

        uint16 index;
        if(!cfs.parse_u2_be(&index)) {
            REPORT_FAILED_METHOD("could not parse index "
                "while parsing LocalVariableTable attribute");
            return false;
        }

        // FIXME Don't work with long and double
        if (index >= _max_locals) {
            REPORT_FAILED_METHOD("invalid local index "
                "while parsing LocalVariableTable attribute");
            return false;
        }

        _local_vars_table->table[j].start_pc = start_pc;
        _local_vars_table->table[j].length = length;
        _local_vars_table->table[j].index = index;
        _local_vars_table->table[j].name = name;
        _local_vars_table->table[j].type = descriptor;
    }

    return true;
} //Method::_parse_local_vars

bool Method::_parse_code( Const_Pool *cp, unsigned cp_size, unsigned code_attr_len, ByteReader &cfs)
{
    unsigned real_code_attr_len = 0;
    if(!cfs.parse_u2_be(&_max_stack)) {
        REPORT_FAILED_CLASS_CLASS(_class->class_loader, _class, "java/lang/ClassFormatError",
            _class->name->bytes << ": could not parse max_stack "
            << "while parsing Code attribute for method "
            << _name->bytes << _descriptor->bytes);
        return false;
    }

    if(!cfs.parse_u2_be(&_max_locals)) {
        REPORT_FAILED_CLASS_CLASS(_class->class_loader, _class, "java/lang/ClassFormatError",
            _class->name->bytes << ": could not parse max_locals "
            << "while parsing Code attribute for method "
            << _name->bytes << _descriptor->bytes);
        return false;
    }

    if(_max_locals < (get_num_arg_bytes() / 4) ) {
        REPORT_FAILED_CLASS_CLASS(_class->class_loader, _class, "java/lang/ClassFormatError",
            _class->name->bytes << ": wrong max_locals count "
            << "while parsing Code attribute for method "
            << _name->bytes << _descriptor->bytes);
        return false;
    }

    if(!cfs.parse_u4_be(& _byte_code_length)) {
        REPORT_FAILED_CLASS_CLASS(_class->class_loader, _class, "java/lang/ClassFormatError",
            _class->name->bytes << ": could not parse bytecode length "
            << "while parsing Code attribute for method "
            << _name->bytes << _descriptor->bytes);
        return false;
    }

    // code length for non-abstract java methods must not be 0
    if(_byte_code_length == 0
        || (_byte_code_length >= (1<<16) && !is_native() && !is_abstract()))
    {
        REPORT_FAILED_CLASS_CLASS(_class->class_loader, _class, "java/lang/ClassFormatError",
            _class->name->bytes << ": bytecode length for method "
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
            REPORT_FAILED_CLASS_CLASS(_class->class_loader, _class, "java/lang/ClassFormatError",
                _class->name->bytes << ": could not parse bytecode for method "
                << _name->bytes << _descriptor->bytes);
            return false;
        }
    }
    real_code_attr_len += _byte_code_length;

    if(!cfs.parse_u2_be(&_n_handlers)) {
            REPORT_FAILED_CLASS_CLASS(_class->class_loader, _class, "java/lang/ClassFormatError",
                _class->name->bytes << ": could not parse number of exception handlers for method "
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
        if(!_handlers[i].parse(cp, cp_size, _byte_code_length, cfs)) {
            REPORT_FAILED_CLASS_CLASS(_class->class_loader, _class, "java/lang/ClassFormatError",
                _class->name->bytes << ": could not parse exceptions for method "
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
        REPORT_FAILED_CLASS_CLASS(_class->class_loader, _class, "java/lang/ClassFormatError",
            _class->name->bytes << ": could not parse number of attributes for method "
            << _name->bytes << _descriptor->bytes);
        return false;
    }
    real_code_attr_len += 2;

    uint32 attr_len = 0;
    for (i=0; i<n_attrs; i++) {
        switch (parse_attribute(cfs, cp, cp_size, code_attr_strings, code_attrs, &attr_len)){
        case ATTR_LineNumberTable:
            {
                if  (!_parse_line_numbers(attr_len, cfs)) {
                    return false;
                }
                break;
            }
        case ATTR_LocalVariableTable:
            {
                static bool TI_enabled = VM_Global_State::loader_env->TI->isEnabled();
                if (TI_enabled)
                {
                    if (!_parse_local_vars(cp, cp_size, attr_len, cfs))
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
        case ATTR_UNDEF:
            // unrecognized attribute; skipped
            break;
        default:
            // error occured
            REPORT_FAILED_CLASS_CLASS(_class->class_loader, _class, "java/lang/InternalError",
                _class->name->bytes << ": unknown error occured "
                "while parsing attributes for code of method "
                << _name->bytes << _descriptor->bytes);
            return false;
        } // switch
        real_code_attr_len += 6 + attr_len; // u2 - attribute_name_index, u4 - attribute_length
    } // for

    if(code_attr_len != real_code_attr_len) {
        REPORT_FAILED_CLASS_CLASS(_class->class_loader, _class, "java/lang/ClassFormatError",
            _class->name->bytes << ": Code attribute length does not match real length "
            "in class file (" << code_attr_len << " vs. " << real_code_attr_len
            << ") while parsing attributes for code of method "
            << _name->bytes << _descriptor->bytes);
        return false;
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


bool Method::parse(Global_Env& env, Class *clss, Const_Pool *cp, unsigned cp_size,
                   ByteReader &cfs)
{
    if (!Class_Member::parse(clss, cp, cp_size, cfs))
        return false;

    // check method descriptor
    if(!check_method_descriptor(_descriptor->bytes)) {
        REPORT_FAILED_CLASS_CLASS(_class->class_loader, _class, "java/lang/ClassFormatError",
            _class->name->bytes << ": invalid descriptor "
            "while parsing method "
            << _name->bytes << _descriptor->bytes);
        return false;
    }

    uint16 attr_count;
    if (!cfs.parse_u2_be(&attr_count)) {
        REPORT_FAILED_CLASS_CLASS(_class->class_loader, _class, "java/lang/ClassFormatError",
            _class->name->bytes << ": could not parse attributes count for method "
            << _name->bytes << _descriptor->bytes);
        return false;
    }
    _intf_method_for_fake_method = NULL;
    //
    // set the has_finalizer, is_clinit and is_init flags
    //
    if (_name == env.FinalizeName_String && _descriptor == env.VoidVoidDescriptor_String) {
        _flags.is_finalize = 1;
        if(clss->name != env.JavaLangObject_String) {
            clss->has_finalizer = 1;
        }
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
            REPORT_FAILED_CLASS_CLASS(_class->class_loader, _class, "java/lang/ClassFormatError",
                _class->name->bytes << ": invalid combination of access flags ("
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
            REPORT_FAILED_CLASS_CLASS(_class->class_loader, _class, "java/lang/ClassFormatError",
                _class->name->bytes << ": invalid combination of access flags (ACC_ABSTRACT|"
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
        if(class_is_interface(_class) && !(is_abstract() && is_public())) {
            REPORT_FAILED_CLASS_CLASS(_class->class_loader, _class, "java/lang/ClassFormatError",
                _class->name->bytes << "." << _name->bytes << _descriptor->bytes
                << ": interface method cannot have access flags other then "
                "ACC_ABSTRACT and ACC_PUBLIC set"
                );
            return false;
        }
        if(class_is_interface(_class) &&
            (is_private() || is_protected() || is_static() || is_final()
            || is_synchronized() || is_native() || is_strict()))
        {
            REPORT_FAILED_CLASS_CLASS(_class->class_loader, _class, "java/lang/ClassFormatError",
                _class->name->bytes << "." << _name->bytes << _descriptor->bytes
                << ": interface method cannot have access flags other then "
                "ACC_ABSTRACT and ACC_PUBLIC set");
            return false;
        }
    }    
    if(is_init() && (is_static() || is_final() || is_synchronized() || is_native() || is_abstract())) {
        REPORT_FAILED_CLASS_CLASS(_class->class_loader, _class, "java/lang/ClassFormatError",
            _class->name->bytes << "." << _name->bytes << _descriptor->bytes
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

    for (unsigned j=0; j<attr_count; j++) {
        //
        // only code and exception attributes are defined for Method
        //
        switch (parse_attribute(cfs, cp, cp_size, method_attr_strings, method_attrs, &attr_len)){
        case ATTR_Code:
            n_code_attr++;
            if(!_parse_code(cp, cp_size, attr_len, cfs))
                return false;
            break;
        case ATTR_Exceptions:
            n_exceptions_attr++;
            if(!_parse_exceptions(cp, cp_size, attr_len, cfs))
                return false;
            break;
        case ATTR_Synthetic:
            if(attr_len != 0) {
                REPORT_FAILED_CLASS_CLASS(_class->class_loader, _class, "java/lang/ClassFormatError",
                    _class->name->bytes << ": attribute Synthetic has non-zero length for method "
                    << _name->bytes << _descriptor->bytes);
                return false;
            }
            _synthetic = true;
            break;
        case ATTR_Deprecated:
            if(attr_len != 0) {
                REPORT_FAILED_CLASS_CLASS(_class->class_loader, _class, "java/lang/ClassFormatError",
                    _class->name->bytes << ": attribute Deprecated has non-zero length for method "
                    << _name->bytes << _descriptor->bytes);
                return false;
            }
            _deprecated = true;
            break;
        case ATTR_UNDEF:
            // unrecognized attribute; skipped
            break;
        default:
            // error occured
            REPORT_FAILED_CLASS_CLASS(_class->class_loader, _class, "java/lang/InternalError",
                _class->name->bytes << ": unknown error occured "
                "while parsing attributes for method "
                << _name->bytes << _descriptor->bytes);
            return false;
        } // switch
    } // for

    //
    // there must be no more than 1 code attribute and no more than 1 exceptions
    // attribute per method
    //
    if (n_code_attr > 1) {
        REPORT_FAILED_CLASS_CLASS(_class->class_loader, _class, "java/lang/ClassFormatError",
            _class->name->bytes << ": there is more than one Code attribute for method "
            << _name->bytes << _descriptor->bytes);
        return false;
    }
    if(n_exceptions_attr > 1) {
        REPORT_FAILED_CLASS_CLASS(_class->class_loader, _class, "java/lang/ClassFormatError",
            _class->name->bytes << ": there is more than one Exceptions attribute for method "
            << _name->bytes << _descriptor->bytes);
        return false;
    }

    if((is_abstract() || is_native()) && n_code_attr > 0) {
        REPORT_FAILED_CLASS_CLASS(_class->class_loader, _class, "java/lang/ClassFormatError",
            _class->name->bytes << "." << _name->bytes << _descriptor->bytes
            << ": " << (is_abstract()?"abstract":(is_native()?"native":""))
            << " method should not have Code attribute present");
        return false;
    }

    if(!(is_abstract() || is_native()) && n_code_attr == 0) {
        REPORT_FAILED_CLASS_CLASS(_class->class_loader, _class, "java/lang/ClassFormatError",
            _class->name->bytes << "." << _name->bytes << _descriptor->bytes
            << ": Java method should have Code attribute present");
        return false;
    }
    if(is_native()) _access_flags |= ACC_NATIVE;
    return true;
} //Method::parse






static bool class_parse_fields(Global_Env* env,
                               Class *clss,
                               ByteReader &cfs)
{
    // Those fields are added by the loader even though they are nor defined
    // in their corresponding class files.
    static     struct VMExtraFieldDescription {
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

        { env->string_pool.lookup("java/lang/reflect/AccessibleObject"), // Constructor, Method, Field
                env->string_pool.lookup("vm_member"),
                env->string_pool.lookup("J"), ACC_PRIVATE},
        { env->string_pool.lookup("java/lang/reflect/AccessibleObject"), // Constructor, Method
                env->string_pool.lookup("parameterTypes"),
                env->string_pool.lookup("[Ljava/lang/Class;"), ACC_PRIVATE},
        { env->string_pool.lookup("java/lang/reflect/AccessibleObject"), // Constructor, Method
                env->string_pool.lookup("exceptionTypes"),
                env->string_pool.lookup("[Ljava/lang/Class;"), ACC_PRIVATE},
        { env->string_pool.lookup("java/lang/reflect/AccessibleObject"), // Constructor, Method, Field
                env->string_pool.lookup("declaringClass"),
                env->string_pool.lookup("Ljava/lang/Class;"), ACC_PRIVATE},
        { env->string_pool.lookup("java/lang/reflect/AccessibleObject"), // Method, Field
                env->string_pool.lookup("name"),
                env->string_pool.lookup("Ljava/lang/String;"), ACC_PRIVATE},
        { env->string_pool.lookup("java/lang/reflect/AccessibleObject"), // Method, Field
                env->string_pool.lookup("type"),
                env->string_pool.lookup("Ljava/lang/Class;"), ACC_PRIVATE}
    };
    if(!cfs.parse_u2_be(&clss->n_fields)) {
        REPORT_FAILED_CLASS_CLASS(clss->class_loader, clss, "java/lang/ClassFormatError",
            clss->name->bytes << ": could not parse number of fields");
        return false;
    }

    int num_fields_in_class_file = clss->n_fields;
    int i;
    for(i = 0; i < int(sizeof(vm_extra_fields)/sizeof(VMExtraFieldDescription)); i++) {
        if(clss->name == vm_extra_fields[i].classname) {
            clss->n_fields++;
        }
    }

    clss->fields = new Field[clss->n_fields];
    // ppervov: FIXME: should throw OOME

    clss->n_static_fields = 0;
    unsigned short last_nonstatic_field = (unsigned short)num_fields_in_class_file;
    for (i=0; i < num_fields_in_class_file; i++) {
        Field fd;
        if(!fd.parse(clss, clss->const_pool, clss->cp_size, cfs))
            return false;
        if(fd.is_static()) {
            clss->fields[clss->n_static_fields] = fd;
            clss->n_static_fields++;
        } else {
            last_nonstatic_field--;
            clss->fields[last_nonstatic_field] = fd;
        }
    }
    assert(last_nonstatic_field == clss->n_static_fields);

    for(i = 0; i < int(sizeof(vm_extra_fields)/sizeof(VMExtraFieldDescription)); i++) {
        if(clss->name == vm_extra_fields[i].classname) {
            Field* f = clss->fields+num_fields_in_class_file;
            f->set(clss, vm_extra_fields[i].fieldname,
                vm_extra_fields[i].descriptor, vm_extra_fields[i].accessflags);
            f->set_injected();
            TypeDesc* td = type_desc_create_from_java_descriptor(
                vm_extra_fields[i].descriptor->bytes, clss->class_loader);
            if( td == NULL ) {
                // error occured
                // ppervov: FIXME: should throw OOME
                return false;
            }
            f->set_field_type_desc(td);
            num_fields_in_class_file++;
        }
    }

    return true; // success
} //class_parse_fields


long _total_method_bytes = 0;

static bool class_parse_methods(Class *clss,
                                         ByteReader &cfs,
                                         Global_Env* env)
{
    if(!cfs.parse_u2_be(&clss->n_methods)) {
        REPORT_FAILED_CLASS_CLASS(clss->class_loader, clss, "java/lang/ClassFormatError",
            clss->name->bytes << ": could not parse number of methods");
        return false;
    }

    clss->methods = new Method[clss->n_methods];

    _total_method_bytes += sizeof(Method)*clss->n_methods;
    for (unsigned i=0;  i < clss->n_methods;  i++) {
        if (!clss->methods[i].parse(*env, clss, clss->const_pool, clss->cp_size, cfs))
            return false;
        Method *m = &clss->methods[i];

        if(m->is_clinit()) {
            // There can be at most one clinit per class.
            if(clss->static_initializer) {
                REPORT_FAILED_CLASS_CLASS(clss->class_loader, clss, "java/lang/ClassFormatError",
                    clss->name->bytes << ": there is more than one class initialization method");
                return false;
            }
            clss->static_initializer = &(clss->methods[i]);
        }
        // to cache the default constructor 
        if (m->get_name() == VM_Global_State::loader_env->Init_String && m->get_descriptor() == VM_Global_State::loader_env->VoidVoidDescriptor_String) {
            clss->default_constructor = &(clss->methods[i]);
        }        
    }
    return true; // success
} //class_parse_methods


static String* const_pool_parse_utf8data(String_Pool& string_pool, ByteReader& cfs,
                                         uint16 len)
{
    // buffer ends before len
    if(!cfs.have(len))
        return false;

    // get utf8 bytes and move buffer pointer
    const char* utf8data = (const char*)cfs.get_and_skip(len);

    // check utf8 correctness
    if(memchr(utf8data, 0, len) != NULL)
        return false;

    // then lookup on utf8 bytes and return string
    return string_pool.lookup(utf8data, len);
}


static String* const_pool_parse_utf8(String_Pool& string_pool,
                                     ByteReader& cfs)
{
    uint16 len;
    if(!cfs.parse_u2_be(&len))
        return false;

    return const_pool_parse_utf8data(string_pool, cfs, len);
}


static bool class_parse_const_pool(Class *clss,
                                   String_Pool& string_pool,
                                   ByteReader &cfs)
{
    if(!cfs.parse_u2_be(&clss->cp_size)) {
        REPORT_FAILED_CLASS_CLASS(clss->class_loader, clss, "java/lang/ClassFormatError",
            clss->name->bytes << ": could not parse constant pool size");
        return false;
    }

    unsigned char *cp_tags = new unsigned char[clss->cp_size];
    // ppervov: FIXME: should throw OOME
    clss->const_pool = new Const_Pool[clss->cp_size];
    // ppervov: FIXME: should throw OOME

    //
    // 0'th constant pool entry is a pointer to the tags array
    //
    clss->const_pool[0].tags = cp_tags;
    cp_tags[0] = CONSTANT_Tags;
    for (unsigned i=1; i<clss->cp_size; i++) {
        // parse tag into tag array
        uint8 tag;
        if(!cfs.parse_u1(&tag)) {
            REPORT_FAILED_CLASS_CLASS(clss->class_loader, clss, "java/lang/ClassFormatError",
                clss->name->bytes << ": could not parse constant pool tag for index " << i);
            return false;
        }

        switch (cp_tags[i] = tag) {
            case CONSTANT_Class:
                if(!cfs.parse_u2_be(&clss->const_pool[i].CONSTANT_Class.name_index)) {
                    REPORT_FAILED_CLASS_CLASS(clss->class_loader, clss, "java/lang/ClassFormatError",
                        clss->name->bytes << ": could not parse name index "
                        "for CONSTANT_Class entry");
                    return false;
                }
                break;

            case CONSTANT_Methodref:
            case CONSTANT_Fieldref:
            case CONSTANT_InterfaceMethodref:
                if(!cfs.parse_u2_be(&clss->const_pool[i].CONSTANT_ref.class_index)) {
                    REPORT_FAILED_CLASS_CLASS(clss->class_loader, clss, "java/lang/ClassFormatError",
                        clss->name->bytes << ": could not parse class index for CONSTANT_*ref entry");
                    return false;
                }
                if(!cfs.parse_u2_be(&clss->const_pool[i].CONSTANT_ref.name_and_type_index)) {
                    REPORT_FAILED_CLASS_CLASS(clss->class_loader, clss, "java/lang/ClassFormatError",
                        clss->name->bytes << ": could not parse name-and-type index for CONSTANT_*ref entry");
                    return false;
                }
                break;

            case CONSTANT_String:
                if(!cfs.parse_u2_be(&clss->const_pool[i].CONSTANT_String.string_index)) {
                    REPORT_FAILED_CLASS_CLASS(clss->class_loader, clss, "java/lang/ClassFormatError",
                        clss->name->bytes << ": could not parse string index for CONSTANT_String entry");
                    return false;
                }
                break;

            case CONSTANT_Float:
            case CONSTANT_Integer:
                if(!cfs.parse_u4_be(&clss->const_pool[i].int_value)) {
                    REPORT_FAILED_CLASS_CLASS(clss->class_loader, clss, "java/lang/ClassFormatError",
                        clss->name->bytes << ": could not parse value for "
                        << (tag==CONSTANT_Integer?"CONSTANT_Integer":"CONSTANT_Float") << " entry");
                    return false;
                }
                break;

            case CONSTANT_Double:
            case CONSTANT_Long:
                // longs and doubles take up two entries
                // on both IA32 & IPF, first constant pool element is used, second element - unused
                if(!cfs.parse_u4_be(&clss->const_pool[i].CONSTANT_8byte.high_bytes)) {
                    REPORT_FAILED_CLASS_CLASS(clss->class_loader, clss, "java/lang/ClassFormatError",
                        clss->name->bytes << ": could not parse high four bytes for "
                        << (tag==CONSTANT_Long?"CONSTANT_Integer":"CONSTANT_Float") << " entry");
                    return false;
                }
                if(!cfs.parse_u4_be(&clss->const_pool[i].CONSTANT_8byte.low_bytes)) {
                    REPORT_FAILED_CLASS_CLASS(clss->class_loader, clss, "java/lang/ClassFormatError",
                        clss->name->bytes << ": could not parse low four bytes for "
                        << (tag==CONSTANT_Long?"CONSTANT_Long":"CONSTANT_Double") << " entry");
                    return false;
                }
                // skip next constant pool entry as it is used by next 4 bytes of Long/Double
                cp_tags[i+1] = cp_tags[i];
                i++;
                break;

            case CONSTANT_NameAndType:
                if(!cfs.parse_u2_be(&clss->const_pool[i].CONSTANT_NameAndType.name_index)) {
                    REPORT_FAILED_CLASS_CLASS(clss->class_loader, clss, "java/lang/ClassFormatError",
                        clss->name->bytes << ": could not parse name index "
                        "for CONSTANT_NameAndType entry");
                    return false;
                }
                if(!cfs.parse_u2_be(&clss->const_pool[i].CONSTANT_NameAndType.descriptor_index)) {
                    REPORT_FAILED_CLASS_CLASS(clss->class_loader, clss, "java/lang/ClassFormatError",
                        clss->name->bytes << ": could not parse descriptor index "
                        "for CONSTANT_NameAndType entry");
                    return false;
                }
                break;

            case CONSTANT_Utf8:
                {
                    // parse and insert string into string table
                    String* str = const_pool_parse_utf8(string_pool, cfs);
                    if(!str) {
                        REPORT_FAILED_CLASS_CLASS(clss->class_loader, clss, "java/lang/ClassFormatError",
                            clss->name->bytes << ": could not parse CONTANT_Utf8 entry");
                        return false;
                    }
                    clss->const_pool[i].CONSTANT_Utf8.string = str;
                }
                break;
            default:
                REPORT_FAILED_CLASS_CLASS(clss->class_loader, clss, "java/lang/ClassFormatError",
                    clss->name->bytes << ": unknown constant pool tag " << cp_tags[i]);
                return false;
        }
    }
    return true;
} //class_parse_const_pool


//
// check consistency of constant pool
//
// make sure all indices to other constant pool entries are in range
// make sure contents of constant pool entries are of the right type
//
// Set CONSTANT_Class entries to point directly to String representing
// internal form of fully qualified name of Class.
//
// Set CONSTANT_String entries to point directly to String representation
// of String.
//
// Peresolve CONSTANT_NameAndType entries to signature
//
static bool check_const_pool(Class* clss,
                             Const_Pool *cp,
                                unsigned cp_size)
{
    unsigned char *cp_tags = cp[0].tags;
    for (unsigned i=1; i<cp_size; i++) {
        switch (cp_tags[i]) 
        {
        case CONSTANT_Class:
        {
            unsigned name_index = cp[i].CONSTANT_Class.name_index;
            if (name_index >= cp_size ||
                cp_tag(cp,name_index) != CONSTANT_Utf8) {
                // illegal name index
                REPORT_FAILED_CLASS_CLASS(clss->class_loader, clss, "java/lang/ClassFormatError",
                    clss->name->bytes << ": wrong name index for CONSTANT_Class entry");
                return false;
            }
            break;
        }
        case CONSTANT_Methodref:
        case CONSTANT_Fieldref:
        case CONSTANT_InterfaceMethodref:
        {
            unsigned class_index = cp[i].CONSTANT_ref.class_index;
            unsigned name_type_index = cp[i].CONSTANT_ref.name_and_type_index;
            if (class_index >= cp_size ||
                cp_tag(cp,class_index) != CONSTANT_Class) {
                REPORT_FAILED_CLASS_CLASS(clss->class_loader, clss, "java/lang/ClassFormatError",
                    clss->name->bytes << ": wrong class index for CONSTANT_*ref entry");
                return false;
            }
            if (name_type_index >= cp_size ||
                cp_tag(cp,name_type_index) != CONSTANT_NameAndType) {
                REPORT_FAILED_CLASS_CLASS(clss->class_loader, clss, "java/lang/ClassFormatError",
                    clss->name->bytes << ": wrong name-and-type index for CONSTANT_*ref entry");
                return false;
            }
            break;
        }
        case CONSTANT_String:
        {
            unsigned string_index = cp[i].CONSTANT_String.string_index;
            if (string_index >= cp_size ||
                cp_tag(cp,string_index) != CONSTANT_Utf8) {
                // illegal string index
                REPORT_FAILED_CLASS_CLASS(clss->class_loader, clss, "java/lang/ClassFormatError",
                    clss->name->bytes << ": wrong string index for CONSTANT_String entry");
                return false;
            }
            // set entry to the actual string
            cp[i].CONSTANT_String.string = cp[string_index].CONSTANT_Utf8.string;
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
            unsigned name_index = cp[i].CONSTANT_NameAndType.name_index;
            unsigned descriptor_index = cp[i].CONSTANT_NameAndType.descriptor_index;
            if (name_index >= cp_size ||
                cp_tag(cp,name_index) != CONSTANT_Utf8) {
                REPORT_FAILED_CLASS_CLASS(clss->class_loader, clss, "java/lang/ClassFormatError",
                    clss->name->bytes << ": wrong name index for CONSTANT_NameAndType entry");
                return false;
            }
            if (descriptor_index >= cp_size ||
                cp_tag(cp,descriptor_index) != CONSTANT_Utf8) {
                REPORT_FAILED_CLASS_CLASS(clss->class_loader, clss, "java/lang/ClassFormatError",
                    clss->name->bytes << ": wrong descriptor index for CONSTANT_NameAndType entry");
                return false;
            }
            cp[i].CONSTANT_NameAndType.name = cp[name_index].CONSTANT_Utf8.string;
            cp[i].CONSTANT_NameAndType.descriptor = cp[descriptor_index].CONSTANT_Utf8.string;
            cp_set_resolved(cp,i);
            break;
        }
        case CONSTANT_Utf8:
            // nothing to do here
            break;
        default:
            REPORT_FAILED_CLASS_CLASS(clss->class_loader, clss, "java/lang/ClassFormatError",
                clss->name->bytes << ": wrong constant pool tag");
            return false;
        }
    }
    return true;
} //check_const_pool


static bool class_parse_interfaces(Class *clss, ByteReader &cfs)
{
    if(!cfs.parse_u2_be(&clss->n_superinterfaces)) {
        REPORT_FAILED_CLASS_CLASS(clss->class_loader, clss, "java/lang/ClassFormatError",
            clss->name->bytes << ": could not parse number of superinterfaces");
        return false;
    }

    clss->superinterfaces = (Class_Superinterface *) clss->class_loader->
        Alloc(sizeof(Class_Superinterface)*clss->n_superinterfaces);
    // ppervov: FIXME: should throw OOME
    for (unsigned i=0; i<clss->n_superinterfaces; i++) {
        uint16 interface_index;
        if(!cfs.parse_u2_be(&interface_index)) {
            REPORT_FAILED_CLASS_CLASS(clss->class_loader, clss, "java/lang/ClassFormatError",
                clss->name->bytes << ": could not parse superinterface index");
            return false;
        }
        //
        // verify that entry in constant pool is of type CONSTANT_Class
        //
        clss->superinterfaces[i].name = cp_check_class(clss->const_pool,clss->cp_size,interface_index);
        if (clss->superinterfaces[i].name == NULL) {
            REPORT_FAILED_CLASS_CLASS(clss->class_loader, clss, "java/lang/ClassFormatError",
                clss->name->bytes << ": constant pool index " << i << " is not CONSTANT_Class entry"
                " while parsing superinterfaces");
            return false;
        }
    }
    return true;
} //class_parse_interfaces



/*
 *  Parses and verifies the classfile.  Format is (from JVM spec) :
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
bool class_parse(Global_Env* env,
                 Class* clss,
                 unsigned* super_class_cp_index,
                 ByteReader& cfs)
{
    /*
     *  get and check magic number (Oxcafebabe)
     */
    uint32 magic;
    if (!cfs.parse_u4_be(&magic)) {
        REPORT_FAILED_CLASS_CLASS(clss->class_loader, clss, "java/lang/ClassFormatError",
            clss->name->bytes << ".class is not a valid Java class file");
        return false;
    }

    if (magic != CLASSFILE_MAGIC) {
        REPORT_FAILED_CLASS_CLASS(clss->class_loader, clss, "java/lang/ClassFormatError",
            clss->name->bytes << ": invalid magic");
        return false;
    }

    /*
     *  get and check major/minor version of classfile
     *  1.1 (45.0-3) 1.2 (46.???) 1.3 (47.???) 1.4 (48.?) 5 (49.0)
     */
    uint16 minor_version;
    if (!cfs.parse_u2_be(&minor_version)) {
        REPORT_FAILED_CLASS_CLASS(clss->class_loader, clss, "java/lang/ClassFormatError",
            clss->name->bytes << ": could not parse minor version");
        return false;
    }

    uint16 major_version;
    if (!cfs.parse_u2_be(&major_version)) {
        REPORT_FAILED_CLASS_CLASS(clss->class_loader, clss, "java/lang/ClassFormatError",
            clss->name->bytes << ": could not parse major version");
        return false;
    }

    if (!(major_version >= CLASSFILE_MAJOR
        && major_version <= CLASSFILE_MAJOR_MAX))
    {
        REPORT_FAILED_CLASS_CLASS(clss->class_loader, clss, "java/lang/UnsupportedClassVersionError",
            clss->name->bytes << " has version number " << major_version);
        return false;
    }

    /*
     *  allocate and parse constant pool
     */
    if (!class_parse_const_pool(clss, env->string_pool, cfs)) {
        return false;
    }

    /*
     * check and preprocess the constant pool
     */
    if (!check_const_pool(clss, clss->const_pool, clss->cp_size))
        return false;

    if(!cfs.parse_u2_be(&clss->access_flags)) {
        REPORT_FAILED_CLASS_CLASS(clss->class_loader, clss, "java/lang/ClassFormatError",
            clss->name->bytes << ": could not parse access flags");
        return false;
    }

    if (class_is_interface(clss)) {
        // NOTE: Fix for the bug that an interface should have
        // abstract flag set.
        // spec/harness/BenchmarkDone has interface flag, but it does not 
        // have abstract flag.
        clss->access_flags |= ACC_ABSTRACT;
    }

    /*
     * parse this_class & super_class & verify their constant pool entries
     */
    uint16 this_class;
    if (!cfs.parse_u2_be(&this_class)) {
        REPORT_FAILED_CLASS_CLASS(clss->class_loader, clss, "java/lang/ClassFormatError",
            clss->name->bytes << ": could not parse this class index");
        return false;
    }

    String *name = cp_check_class(clss->const_pool, clss->cp_size, this_class);
    if (name == NULL) {
        REPORT_FAILED_CLASS_CLASS(clss->class_loader, clss, "java/lang/ClassFormatError",
            clss->name->bytes << ": this_class constant pool entry "
            << this_class << " is an illegal CONSTANT_Class entry");
        return false;
    }

    /*
     * When defineClass from byte stream, there are cases that clss->name is null,
     * so we should add a check here
     */
    if (clss->name && name != clss->name) { 
        REPORT_FAILED_CLASS_CLASS(clss->class_loader, clss, "java/lang/NoClassDefFoundError",
            clss->name->bytes << ": class name in class data does not match class name passed");
        return false;
    }

    if (!clss->name) {
        clss->name = name;
    }

    /*
     *  Mark the current class as resolved.
     */
    cp_resolve_to_class(clss->const_pool, this_class, clss);

    /*
     * parse the super class name
     */
    uint16 super_class;
    if (!cfs.parse_u2_be(&super_class)) {
        REPORT_FAILED_CLASS_CLASS(clss->class_loader, clss, "java/lang/ClassFormatError",
            clss->name->bytes << ": could not parse super class index");
        return false;
    }

    *super_class_cp_index = super_class;

    if (super_class == 0) {
        //
        // this class must represent java.lang.Object
        //
        if(clss->name != env->JavaLangObject_String) {
            REPORT_FAILED_CLASS_CLASS(clss->class_loader, clss, "java/lang/ClassFormatError",
                clss->name->bytes << ": class does not contain super class "
                << "but is not java.lang.Object class");
            return false;
        }
        clss->super_name = NULL;
    } else {
        clss->super_name = cp_check_class(clss->const_pool, clss->cp_size, super_class);
        if (clss->super_name == NULL) {
            REPORT_FAILED_CLASS_CLASS(clss->class_loader, clss, "java/lang/ClassFormatError",
                clss->name->bytes << ": super_class constant pool entry "
                << super_class << " is an illegal CONSTANT_Class entry");
            return false;
        }
    }

    /*
     * allocate and parse class' interfaces
     */
    if (!class_parse_interfaces(clss, cfs))
        return false;

    /* 
     *  allocate and parse class' fields
     */
    if (!class_parse_fields(env, clss, cfs))
        return false;


    /* 
     *  allocate and parse class' methods
     */
    if (!class_parse_methods(clss, cfs, env))
        return false;

    /*
     *  only the FileName attribute is defined for Class
     */
    uint16 n_attrs;
    if (!cfs.parse_u2_be(&n_attrs)) {
        REPORT_FAILED_CLASS_CLASS(clss->class_loader, clss, "java/lang/ClassFormatError",
            clss->name->bytes << ": could not parse number of attributes");
        return false;
    }

    unsigned n_source_file_attr = 0;
    unsigned numSourceDebugExtensions = 0;
    uint32 attr_len = 0;

    for (unsigned i=0; i<n_attrs; i++) {
        switch(parse_attribute(cfs, clss->const_pool, clss->cp_size, class_attr_strings, class_attrs, &attr_len)){
        case ATTR_SourceFile:
        {
            // a class file can have at most one source file attribute
            if (++n_source_file_attr > 1) {
                REPORT_FAILED_CLASS_CLASS(clss->class_loader, clss, "java/lang/ClassFormatError",
                    clss->name->bytes << ": there is more than one SourceFile attribute");
                return false;
            }

            // attribute length must be two (vm spec 4.7.2)
            if (attr_len != 2) {
                REPORT_FAILED_CLASS_CLASS(clss->class_loader, clss, "java/lang/ClassFormatError",
                    clss->name->bytes << ": SourceFile attribute has incorrect length ("
                    << attr_len << " bytes, should be 2 bytes)");
                return false;
            }

            // constant value attribute
            uint16 filename_index;
            if(!cfs.parse_u2_be(&filename_index)) {
                REPORT_FAILED_CLASS_CLASS(clss->class_loader, clss, "java/lang/ClassFormatError",
                    clss->name->bytes << ": could not parse filename index"
                    << " while parsing SourceFile attribute");
                return false;
            }

            clss->src_file_name = cp_check_utf8(clss->const_pool,clss->cp_size,filename_index);
            if (clss->src_file_name == NULL) {
                REPORT_FAILED_CLASS_CLASS(clss->class_loader, clss, "java/lang/ClassFormatError",
                    clss->name->bytes << ": filename index points to incorrect constant pool entry"
                    << " while parsing SourceFile attribute");
                return false;
            }
            break;
        }

        case ATTR_InnerClasses:
        {
            bool isinner = false;
            // found_myself == 2: myself is not inner class or has passed myself when iterating inner class attribute arrays
            // found_myself == 1: myself is inner class, current index of inner class attribute arrays is just myself
            // found_myself == 0: myself is inner class, hasn't met myself in inner class attribute arrays
            int found_myself = 2;
            if(strchr(clss->name->bytes, '$')){
                isinner = true;
                found_myself = 0;
            }
            //Only handle inner class
            uint16 num_of_classes;
            if(!cfs.parse_u2_be(&num_of_classes)) {
                REPORT_FAILED_CLASS_CLASS(clss->class_loader, clss, "java/lang/ClassFormatError",
                    clss->name->bytes << ": could not parse number of classes"
                    << " while parsing InnerClasses attribute");
                return false;
            }

            clss->declaringclass_index = 0; // would it be a valid index
            clss->innerclass_indexes = NULL;
            if(isinner)
                clss->n_innerclasses = (uint16)(num_of_classes - 1); //exclude itself
            else
                clss->n_innerclasses = num_of_classes;
            if(num_of_classes)
                clss->innerclass_indexes = (uint16*) clss->class_loader->
                    Alloc(2*sizeof(uint16)*clss->n_innerclasses);
                // ppervov: FIXME: should throw OOME
            int index = 0;
            for(int i = 0; i < num_of_classes; i++){
                uint16 inner_clss_info_idx;
                if(!cfs.parse_u2_be(&inner_clss_info_idx)) {
                    REPORT_FAILED_CLASS_CLASS(clss->class_loader, clss, "java/lang/ClassFormatError",
                        clss->name->bytes << ": could not parse inner class info index"
                        << " while parsing InnerClasses attribute");
                    return false;
                }
                if(inner_clss_info_idx
                   && cp_tag(clss->const_pool,inner_clss_info_idx) != CONSTANT_Class)
                {
                    REPORT_FAILED_CLASS_CLASS(clss->class_loader, clss, "java/lang/ClassFormatError",
                        clss->name->bytes << ": inner class info index points to incorrect constant pool entry"
                        << " while parsing InnerClasses attribute");
                    return false;
                }

                if(!found_myself){
                    String *clssname = cp_check_class(clss->const_pool, clss->cp_size, inner_clss_info_idx);
                    // Only handle this class
                    if(clss->name == clssname)
                        found_myself = 1;
                }
                if(found_myself != 1)
                    clss->innerclass_indexes[index++] = inner_clss_info_idx;

                uint16 outer_clss_info_idx;
                if(!cfs.parse_u2_be(&outer_clss_info_idx)) {
                    REPORT_FAILED_CLASS_CLASS(clss->class_loader, clss, "java/lang/ClassFormatError",
                        clss->name->bytes << ": could not parse outer class info index"
                        << " while parsing InnerClasses attribute");
                    return false;
                }
                if(outer_clss_info_idx
                    && cp_tag(clss->const_pool,outer_clss_info_idx) != CONSTANT_Class)
                {
                    REPORT_FAILED_CLASS_CLASS(clss->class_loader, clss, "java/lang/ClassFormatError",
                        clss->name->bytes << ": outer class info index points to incorrect constant pool entry"
                        << " while parsing InnerClasses attribute");
                    return false;
                }
                if(found_myself == 1 && outer_clss_info_idx){
                    clss->declaringclass_index = outer_clss_info_idx;
                }

                uint16 inner_name_idx;
                if(!cfs.parse_u2_be(&inner_name_idx)) {
                    REPORT_FAILED_CLASS_CLASS(clss->class_loader, clss, "java/lang/ClassFormatError",
                        clss->name->bytes << ": could not parse inner name index"
                        << " while parsing InnerClasses attribute");
                    return false;
                }
                if(inner_name_idx
                    && cp_tag(clss->const_pool,inner_name_idx) != CONSTANT_Utf8)
                {
                    REPORT_FAILED_CLASS_CLASS(clss->class_loader, clss, "java/lang/ClassFormatError",
                        clss->name->bytes << ": inner name index points to incorrect constant pool entry"
                        << " while parsing InnerClasses attribute");
                    return false;
                }
                uint16 inner_clss_access_flag;
                if(!cfs.parse_u2_be(&inner_clss_access_flag)) {
                    REPORT_FAILED_CLASS_CLASS(clss->class_loader, clss, "java/lang/ClassFormatError",
                        clss->name->bytes << ": could not parse inner class access flags"
                        << " while parsing InnerClasses attribute");
                    return false;
                }
                if(found_myself == 1) {
                    found_myself = 2;
                    clss->access_flags = inner_clss_access_flag;
                } else
                    clss->innerclass_indexes[index++] = inner_clss_access_flag;
            } // for num_of_classes
        }break; //case ATTR_InnerClasses

        case ATTR_SourceDebugExtension:
            {
                // attribute length is already recorded in attr_len
                // now reading debug extension information
                if( ++numSourceDebugExtensions > 1 ) {
                    REPORT_FAILED_CLASS_CLASS(clss->class_loader, clss, "java/lang/ClassFormatError",
                        clss->name->bytes << ": there is more than one SourceDebugExtension attribute");
                    return false;
                }

                // cfs is at debug_extension[] which is:
                //      The debug_extension array holds a string, which must be in UTF-8 format.
                //      There is no terminating zero byte.
                clss->sourceDebugExtension = const_pool_parse_utf8data(env->string_pool, cfs, attr_len);
            }
            break;

        case ATTR_UNDEF:
            // unrecognized attribute; skipped
            break;
        default:
            // error occured
            REPORT_FAILED_CLASS_CLASS(clss->class_loader, clss, "java/lang/InternalError",
                clss->name->bytes << ": unknown error occured"
                " while parsing attributes for class");
            return false;
        } // switch
    } // for

    /*
     *   can't be both final and interface, or both final and abstract
     */
    if (class_is_final(clss) && class_is_interface(clss))
    {
        REPORT_FAILED_CLASS_CLASS(clss->class_loader, clss, "java/lang/ClassFormatError",
            clss->name->bytes << ": interface cannot have ACC_FINAL flag set");
        return false;
    }
    
    if (class_is_final(clss) && class_is_abstract(clss)) {
        REPORT_FAILED_CLASS_CLASS(clss->class_loader, clss, "java/lang/ClassFormatError",
            clss->name->bytes << ": class cannot have both ACC_FINAL and ACC_ABSTRACT flags set");
        return false;
    }

    return true;
} //class_parse


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
    const String* class_name = const_pool_parse_utf8(env->string_pool, cfs);
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


////////////////////////////////////////////////////////////////////
// begin support for JIT notification when classes are extended

struct Class_Extended_Notification_Record {
    Class *class_of_interest;
    JIT   *jit;
    void  *callback_data;
    Class_Extended_Notification_Record *next;

    bool equals(Class *class_of_interest_, JIT *jit_, void *callback_data_) {
        if ((class_of_interest == class_of_interest_) &&
            (jit == jit_) &&
            (callback_data == callback_data_)) {
            return true;
        }
        return false;
    }
};


// Notify the given JIT whenever the class "clss" is extended.
// The callback_data pointer will be passed back to the JIT during the callback.  
// The JIT's callback function is JIT_extended_class_callback.
void class_register_jit_extended_class_callback(Class *clss, JIT *jit_to_be_notified, void *callback_data)
{
    // Don't insert the same entry repeatedly on the notify_extended_records list.
    Class_Extended_Notification_Record *nr = clss->notify_extended_records;
    while (nr != NULL) {
        if (nr->equals(clss, jit_to_be_notified, callback_data)) {
            return;
        }
        nr = nr->next;
    }

    // Insert a new notification record.
    Class_Extended_Notification_Record *new_nr = 
        (Class_Extended_Notification_Record *)STD_MALLOC(sizeof(Method_Change_Notification_Record));
    new_nr->class_of_interest  = clss;
    new_nr->jit                = jit_to_be_notified;
    new_nr->callback_data      = callback_data;
    new_nr->next               = clss->notify_extended_records;
    clss->notify_extended_records = new_nr;
} //class_register_jit_extended_class_callback


void do_jit_extended_class_callbacks(Class *clss, Class *new_subclass)
{
    Class_Extended_Notification_Record *nr;
    for (nr = clss->notify_extended_records;  nr != NULL;  nr = nr->next) {
        JIT *jit_to_be_notified = nr->jit;
        Boolean code_was_modified = 
            jit_to_be_notified->extended_class_callback(/*extended_class*/ clss,
                                                        /*new_class*/ new_subclass,
                                                        nr->callback_data);
        if (code_was_modified) {
#ifdef _IPF_
            // 20030128 I don't think we have to do a flush_hw_cache() here since that should 
            // be done by the recompiled_method callbacks.
            sync_i_cache();            
            do_mf();
#endif //_IPF_
        }
    }
} //do_jit_extended_class_callbacks

// end support for JIT notification when classes are extended
////////////////////////////////////////////////////////////////////


unsigned class_calculate_size(const Class* klass)
{
    unsigned size = 0;
    size += sizeof(Class);
    size += klass->n_innerclasses*sizeof(uint16);
    size += klass->cp_size*sizeof(Const_Pool);
    for(unsigned i = 0; i < klass->n_fields; i++) {
        size += klass->fields[i].calculate_size();
    }
    for(unsigned i = 0; i < klass->n_methods; i++) {
        size += klass->methods[i].calculate_size();
    }
    size += klass->n_superinterfaces*sizeof(Class_Superinterface);
    size += klass->static_data_size;
    size += klass->static_method_size;
    if(!class_is_interface(klass))
        size += sizeof(VTable);
    size += klass->n_intfc_table_entries*sizeof(Class*);
    for(Class_Extended_Notification_Record* mcnr = klass->notify_extended_records;
        mcnr != NULL; mcnr = mcnr->next)
    {
        size += sizeof(Class_Extended_Notification_Record);
    }
    size += sizeof(Lock_Manager);

    return size;
}
