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
 * @version $Revision: 1.1.2.6.4.5 $
 */  



//
// exceptions that can be thrown during class resolution:
//
//  (0) LinkageError exceptions during loading and linking
//  (1) ExceptionInInitializerError: class initializer completes
//      abruptly by throwing an exception
//  (2) IllegalAccessError: current class or interface does not have
//      permission to access the class or interface being resolved.
//
//  class resolution can occur indirectly via resolution of other
//  constant pool entries (Fieldref, Methodref, InterfaceMethodref),
//  or directly by the following byte codes:
//
//  (0) anewarray       (pg 162)
//  (1) checkcast       (pg 174)
//  (2) instanceof      (pg 256)
//  (3) multianewarray  (pg 316) 
//      - also throws the linking exception, IllegalAccessError, if 
//      the current class does have permission to access the *base* 
//      class of the resolved array class.
//  (4) new             (pg 318) 
//      - also throws the linking exception, InstantiationError, if 
//      the resolved class is an abstract class or an interface.
//
//  resolution of constants occurs directly by the following byte codes:
//  (0) ldc     (pg 291)
//  (1) ldc_w   (pg 292)
//  (2) ldc2_w  (pg 294)
//
//  of these ldc byte codes, only the ldc and ldc_w can cause exceptions
//  and only when they refer to CONSTANT_String entries.  The only exception
//  possible seems to be the VirtualMachineError exception that happens
//  when the VM runs out of internal resources.
//
//  exceptions that can be thrown during field and method resolution:
//
//  (0) any of the exceptions for resolving classes
//  (1) NoSuchFieldError: referenced field does not exist in specified 
//      class or interface.
//  (2) IllegalAccessError: the current class does not have permission
//      to access the referenced field.
//
//  During resolution of methods that are declared as native, if the code
//  for the native method cannot be found, then the VM throws an 
//  UnsatisfiedLinkError.  Note, that this causes a problem because the
//  code for native methods are usually loaded in by the static initializer
//  code of a class.  Therefore, this condition can only be checked at
//  run-time, when the native method is first called.
//
//  In addition, the byte codes that refer to the constant pool entries
//  can throw the following exceptions:
//
//  (0) IncompatibleClassChangeError: thrown by getfield and putfield 
//      if the field reference is resolved to a static field.  
//  (1) IncompatibleClassChangeError: thrown by getstatic and putstatic
//      if the field reference is resolved to a non-static field.
//  (2) IncompatibleClassChangeError: thrown by invokespecial, 
//      invokeinterface and invokevirtual if the method reference is 
//      resolved to a static method.
//  (3) AbstractMethodError: thrown by invokespecial, invokeinterface
//      and invokevirtual if the method reference is resolved to an 
//      abstract method.
//  (4) IncompatibleClassChangeError: thrown by invokestatic if the
//      method reference is resolved to a non-static method.
//
//  Invokeinterface throws an IncompatibleClassChangeError if no method
//  matching the resolved name and description can be found in the class
//  of the object that is being invoked, or if the method being invoked is
//  a class (static) method.
//

#define LOG_DOMAIN util::CLASS_LOGGER
#include "cxxlog.h"

#include "Class.h"
#include "classloader.h"
#include "environment.h"
#include "jit_intf.h"
#include "compile.h"
#include "exceptions.h"
#include "interpreter.h"

#include "open/bytecodes.h"
#include "open/vm_util.h"


#define CLASS_REPORT_FAILURE(target, cp_index, exnclass, exnmsg)    \
{                                                               \
    std::stringstream ss;                                       \
    ss << exnmsg;                                               \
    class_report_failure(target, cp_index, exnclass, ss);       \
}

static void class_report_failure(Class* target, uint16 cp_index, 
                                 const char* exnname, std::stringstream& exnmsg)
{
    TRACE2("resolve.testing", "class_report_failure: " << exnmsg.str().c_str());
    jthrowable exn = exn_create(exnname, exnmsg.str().c_str());
    // ppervov: FIXME: should throw OOME
    class_report_failure(target, cp_index, exn);
}

// check is class "first" in the same runtime package with class "second"
static bool is_class_in_same_runtime_package( Class *first, Class *second)
{
    return first->package == second->package;
}

static Class* _resolve_class(Global_Env *env,
                             Class *clss,
                             unsigned cp_index)
{
    assert(hythread_is_suspend_enabled());
    Const_Pool *cp = clss->const_pool;

    clss->m_lock->_lock();
    if(cp_in_error(cp, cp_index)) {
        TRACE2("resolve:testing", "Constant pool entry " << cp_index << " already contains error.");
        clss->m_lock->_unlock();
        return NULL;
    }

    if(cp_is_resolved(cp, cp_index)) {
        clss->m_lock->_unlock();
        return cp[cp_index].CONSTANT_Class.klass;
    }

    String *classname = cp[cp[cp_index].CONSTANT_Class.name_index].CONSTANT_Utf8.string;
    clss->m_lock->_unlock();

    // load the class in
    Class *other_clss;

    other_clss = clss->class_loader->LoadVerifyAndPrepareClass(env, classname);
    if(other_clss == NULL)
    {
        jthrowable exn = class_get_error(clss->class_loader, classname->bytes);
        if (exn) {
            class_report_failure(clss, cp_index, exn);
        } else {
            assert(exn_raised());
        }
        return NULL;
    }

    // Check access control:
    //   referenced class should be public,
    //   or referenced class & declaring class are the same,
    //   or referenced class & declaring class are in the same runtime package,
    //   or declaring class not verified
    //   (the last case is needed for certain magic classes,
    //   eg, reflection implementation)
    if(class_is_public(other_clss)
        || other_clss == clss
        || is_class_in_same_runtime_package( clss, other_clss )
        || clss->is_not_verified) 
    {
        clss->m_lock->_lock();
        cp_resolve_to_class(cp, cp_index, other_clss);
        clss->m_lock->_unlock();
        return other_clss;
    }

    // Check access control for inner classes:
    //   access control checks is the same as for members
    if(strrchr((char*)other_clss->name->bytes, '$') != NULL
        && check_inner_class_access(env, other_clss, clss))
    {
        clss->m_lock->_lock();
        cp_resolve_to_class(cp, cp_index, other_clss);
        clss->m_lock->_unlock();
        return other_clss;
    }

    CLASS_REPORT_FAILURE(clss, cp_index, "java/lang/IllegalAccessError",
        "from " << clss->name->bytes << " to " << other_clss->name->bytes);
    // IllegalAccessError
    return NULL;
} //_resolve_class


static bool class_can_instantiate(Class* clss, bool _throw)
{
    ASSERT_RAISE_AREA;
    bool fail = class_is_abstract(clss);
    if(fail && _throw) {
        exn_raise_by_name("java/lang/InstantiationError", clss->name->bytes);
    }
    return !fail;
}


Class* _resolve_class_new(Global_Env *env, Class *clss,
                          unsigned cp_index)
{
    ASSERT_RAISE_AREA;

    Class *new_clss = _resolve_class(env,clss,cp_index);
    if (!new_clss) return NULL;
    bool can_instantiate = class_can_instantiate(new_clss, false);

    if (new_clss && !can_instantiate) {
        return NULL;
    }
    return new_clss;
} //_resolve_class_new

// Can "other_clss" access the field or method "member"?
Boolean check_member_access(Class_Member *member, Class *other_clss)
{
    Class *member_clss = member->get_class();
    const char *reflect = "java/lang/reflect/";

    // check if reflection class it has full access to all fields
    if( !strncmp( other_clss->name->bytes, reflect, strlen(reflect) ) ) {
        return 1;
    }
    // check access permissions
    if (member->is_public() || (other_clss == member_clss)) {
        // no problemo
        return 1;
    } else if (member->is_private()) {
        // IllegalAccessError
        return 0;
    } else if (member->is_protected()) {
        // When a member is protected, it can be accessed by classes 
        // in the same runtime package. 
        if( is_class_in_same_runtime_package( other_clss, member_clss ) )
            return 1;
        // Otherwise, when other_clss is not in the same package, 
        // the class containing the member (member_clss) must be
        // a superclass of other_clss.
        Class *c;
        for (c = other_clss->super_class; c != NULL; c = c->super_class) {
            if (c == member_clss)
                break;
        }
        if (c == NULL) {
            // IllegalAccessError
            return 0;
        }
        return 1;
    } else {
        // When a member has default (or package private) access, it can only be accessed 
        // by classes in the same package.
        if( is_class_in_same_runtime_package( other_clss, member_clss ) )
            return 1;
        return 0;
    }
} //check_member_access

inline static bool
is_class_extended_class( Class *super_clss, 
                         Class *check_clss)
{
    for(; super_clss != NULL; super_clss = super_clss->super_class)
    {
        if( super_clss->class_loader == check_clss->class_loader
            && super_clss->name == check_clss->name )
        {
            return true;
        }
    }
    return false;
} // is_class_extended_class

inline static Class*
get_enclosing_class( Global_Env *env,
                     Class *klass )
{
    Class *encl_clss = NULL;

    if( strrchr( (char*)klass->name->bytes, '$') != NULL ) 
    {   // it is anonymous class
        // search "this$..." in fields and look for enclosing class
        unsigned index;
        Field *field;
        for( index = 0, field = &klass->fields[index];
             index < klass->n_fields;
             index++, field = &klass->fields[index] )
        {
            if( strncmp( field->get_name()->bytes, "this$", 5 ) 
                || !(field->get_access_flags() & ACC_FINAL)
                || !field->is_synthetic() ) 
            {
                continue;
            }
            // found self, get signature of enclosing class
            const String* desc = field->get_descriptor();
            // get name of enclosing class
            String* name = env->string_pool.lookup(&desc->bytes[1], desc->len - 2);
            // loading enclosing class
            encl_clss = klass->class_loader->LoadVerifyAndPrepareClass(env, name);
            break;
        }
    }
    return encl_clss;
} // get_enclosing_class

// Can "other_clss" access the "inner_clss"
Boolean
check_inner_class_access(Global_Env *env,
                         Class *inner_clss,
                         Class *other_clss)
{
    // check access permissions
    if ((inner_clss->access_flags & ACC_PUBLIC) || (other_clss == inner_clss)) {
        // no problemo
        return 1;
    } else if (inner_clss->access_flags & ACC_PRIVATE) {
        // IllegalAccessError
        return 0;
    } else if (inner_clss->access_flags & ACC_PROTECTED) {
        // When inner class is protected, it can be accessed by classes 
        // in the same runtime package. 
        if( is_class_in_same_runtime_package( other_clss, inner_clss ) )
            return 1;
        // Otherwise, when other_clss is not in the same package, 
        // inner_clss must be a superclass of other_clss.
        for( Class *decl_other_clss = other_clss; decl_other_clss != NULL; )
        {
            for( Class *decl_inner_clss = inner_clss; decl_inner_clss != NULL; )
            {
                if(is_class_extended_class( decl_other_clss, decl_inner_clss ) ) {
                    return 1;
                }
                if( !decl_inner_clss->declaringclass_index ) {
                    // class "decl_inner_clss" isn't inner class
                    break;
                } else {
                    // loading declaring class
                    if(Class* decl_inner_clss_res = _resolve_class(env, decl_inner_clss,
                            decl_inner_clss->declaringclass_index)) {
                        decl_inner_clss = decl_inner_clss_res;
                    } else {
                        break;
                    }
                }
            }
            if( !decl_other_clss->declaringclass_index )
            {
                // class "decl_other_clss" isn't inner class
                decl_other_clss = get_enclosing_class(env, decl_other_clss);
                continue;
            } else {
                // loading declaring class
                if(Class* decl_other_clss_res =
                    _resolve_class(env, decl_other_clss, decl_other_clss->declaringclass_index))
                {
                    decl_other_clss = decl_other_clss_res ;
                    continue;
                }
            }
            break;
        }
        // IllegalAccessError
        return 0;
    } else {
        // When a member has default (or package private) access,
        // it can only be accessed by classes in the same runtime package.
        if(is_class_in_same_runtime_package(other_clss, inner_clss))
            return 1;
        return 0;
    }
} // check_inner_class_access

/**
 *
 */
static Field* _resolve_field(Global_Env *env, Class *clss, unsigned cp_index)
{
    Const_Pool *cp = clss->const_pool;
    clss->m_lock->_lock();
    if(cp_in_error(cp, cp_index)) {
        TRACE2("resolve.testing", "Constant pool entry " << cp_index << " already contains error.");
        clss->m_lock->_unlock();
        return NULL;
    }

    if (cp_is_resolved(cp, cp_index)) {
        clss->m_lock->_unlock();
        return cp[cp_index].CONSTANT_ref.field;
    }

    //
    // constant pool entry hasn't been resolved yet
    //
    unsigned other_index = cp[cp_index].CONSTANT_ref.class_index;
    clss->m_lock->_unlock();

    //
    // check error condition from resolve class
    //
    Class *other_clss = _resolve_class(env, clss, other_index);
    if(!other_clss) {
        if(cp_in_error(clss->const_pool, other_index)) {
            class_report_failure(clss, cp_index, 
                (jthrowable)(&(clss->const_pool[other_index].error.cause)));
        } else {
            assert(exn_raised());
        }
        return NULL;
    }

    String* name = cp[cp[cp_index].CONSTANT_ref.name_and_type_index]
            .CONSTANT_NameAndType.name;
    String* desc = cp[cp[cp_index].CONSTANT_ref.name_and_type_index]
            .CONSTANT_NameAndType.descriptor;

    Field* field = class_lookup_field_recursive(other_clss, name, desc);
    if (field == NULL)
    {
        //
        // NoSuchFieldError
        //
        CLASS_REPORT_FAILURE(clss, cp_index, "java/lang/NoSuchFieldError",
            other_clss->name->bytes << "." << name->bytes
            << " of type " << desc->bytes
            << " while resolving constant pool entry at index "
            << cp_index << " in class " << clss->name->bytes);
        return NULL;
    }

    //
    // check access permissions
    //
    if (check_member_access(field, clss) == 0)
    {
        //
        // IllegalAccessError
        //
        CLASS_REPORT_FAILURE(clss, cp_index, "java/lang/IllegalAccessError",
            other_clss->name->bytes << "." << name->bytes
            << " of type " << desc->bytes
            << " while resolving constant pool entry at index "
            << cp_index << " in class " << clss->name->bytes);
        return NULL;
    }
    clss->m_lock->_lock();
    cp_resolve_to_field(cp, cp_index, field);
    clss->m_lock->_unlock();

    return field;
} //_resolve_field


bool field_can_link(Class* clss, Field* field, bool _static, bool putfield, bool _throw)
{
    ASSERT_RAISE_AREA;
    if(_static?(!field->is_static()):(field->is_static())) {
        if(_throw) {
            exn_raise_by_name("java/lang/IncompatibleClassChangeError",
                field->get_class()->name->bytes);
        }
        return false;
    }
    if(putfield && field->is_final()) {
        for(int fn = 0; fn < clss->n_fields; fn++) {
            if(&(clss->fields[fn]) == field) {
                return true;
            }
        }
        if(_throw) {
            unsigned buf_size = clss->name->len + field->get_class()->name->len + field->get_name()->len + 15;
            char* buf = (char*)STD_ALLOCA(buf_size);
            memset(buf, 0, buf_size);
            sprintf(buf, " from %s to %s.%s", clss->name->bytes, field->get_class()->name->bytes, field->get_name()->bytes);
            jthrowable exc_object = exn_create("java/lang/IllegalAccessError", buf);
            exn_raise_object(exc_object);
        }
        return false;
    }
    return true;
}

static bool CAN_LINK_FROM_STATIC = true; // can link from putstatic/getstatic
static bool CAN_LINK_FROM_FIELD = false; // can link from putfield/getfield
static bool LINK_WRITE_ACCESS   = true;  // link from putfield/putstatic
static bool LINK_READ_ACCESS = false;    // link from getfield/getstatic
static bool LINK_THROW_ERRORS = true;    // should throw linking exception on error
static bool LINK_NO_THROW = false;       // must not throw linking exception on error

static Field* _resolve_static_field(Global_Env *env,
                                  Class *clss,
                                  unsigned cp_index,
                                  bool putfield)
{
    ASSERT_RAISE_AREA;

    Field *field = _resolve_field(env,clss,cp_index);
    if(field && !field_can_link(clss, field, CAN_LINK_FROM_STATIC, putfield, LINK_NO_THROW)) {
        return NULL;
    }
    return field;
} //_resolve_static_field



static Field* _resolve_nonstatic_field(Global_Env *env,
                                       Class *clss,
                                       unsigned cp_index,
                                       unsigned putfield)
{
    ASSERT_RAISE_AREA;

    Field *field = _resolve_field(env, clss, cp_index);
    if(field && !field_can_link(clss, field, CAN_LINK_FROM_FIELD, putfield, LINK_NO_THROW)) {
        return NULL;
    }
    return field;
} //_resolve_nonstatic_field

/**
 *
 */ 
static Method* _resolve_method(Global_Env *env, Class *clss, unsigned cp_index)
{
    Const_Pool *cp = clss->const_pool;
    clss->m_lock->_lock();
    if(cp_in_error(cp, cp_index)) {
        TRACE2("resolve:testing", "Constant pool entry " << cp_index << " already contains error.");
        clss->m_lock->_unlock();
        return NULL;
    }

    if (cp_is_resolved(cp,cp_index)) {
        clss->m_lock->_unlock();
        return cp[cp_index].CONSTANT_ref.method;
    }

    //
    // constant pool entry hasn't been resolved yet
    //
    unsigned other_index;
    other_index = cp[cp_index].CONSTANT_ref.class_index;
    clss->m_lock->_unlock();

    //
    // check error condition from resolve class
    //
    Class *other_clss = _resolve_class(env, clss, other_index);
    if(!other_clss) {
        if(cp_in_error(clss->const_pool, other_index)) {
            class_report_failure(clss, cp_index, 
                (jthrowable)(&(clss->const_pool[other_index].error.cause)));
        } else {
            assert(exn_raised());
        }
        return NULL;
    }

    String* name = cp[cp[cp_index].CONSTANT_ref.name_and_type_index].
        CONSTANT_NameAndType.name;

    String* desc = cp[cp[cp_index].CONSTANT_ref.name_and_type_index].
        CONSTANT_NameAndType.descriptor;

    // CONSTANT_Methodref must refer to a class, not an interface, and
    // CONSTANT_InterfaceMethodref must refer to an interface (vm spec 4.4.2)
    if (cp_is_methodref(cp, cp_index) && class_is_interface(other_clss)) {
        CLASS_REPORT_FAILURE(clss, cp_index, "java/lang/IncompatibleClassChangeError",
            other_clss->name->bytes
            << " while resolving constant pool entry " << cp_index
            << " in class " << clss->name->bytes);
        return NULL;
    }

    if(cp_is_interfacemethodref(cp, cp_index) && !class_is_interface(other_clss)) {
        CLASS_REPORT_FAILURE(clss, cp_index, "java/lang/IncompatibleClassChangeError",
            other_clss->name->bytes
            << " while resolving constant pool entry " << cp_index
            << " in class " << clss->name->bytes);
        return NULL;
    }

    Method* method = class_lookup_method_recursive(other_clss, name, desc);
    if (method == NULL) {
        // NoSuchMethodError
        CLASS_REPORT_FAILURE(clss, cp_index, "java/lang/NoSuchMethodError",
            other_clss->name->bytes << "." << name->bytes << desc->bytes
            << " while resolving constant pool entry at index " << cp_index
            << " in class " << clss->name->bytes);
        return NULL;
    }

    if(method_is_abstract(method) && !class_is_abstract(other_clss)) {
        // AbstractMethodError
        CLASS_REPORT_FAILURE(clss, cp_index, "java/lang/AbstractMethodError",
            other_clss->name->bytes << "." << name->bytes << desc->bytes
            << " while resolving constant pool entry at index " << cp_index
            << " in class " << clss->name->bytes);
        return NULL;
    }

    //
    // check access permissions
    //
    if (check_member_access(method,clss) == 0) {
        // IllegalAccessError
        CLASS_REPORT_FAILURE(clss, cp_index, "java/lang/IllegalAccessError",
            other_clss->name->bytes << "." << name->bytes << desc->bytes
            << " while resolving constant pool entry at index " << cp_index
            << " in class " << clss->name->bytes);
        return NULL; 
    }

    clss->m_lock->_lock();
    cp_resolve_to_method(cp,cp_index,method);
    clss->m_lock->_unlock();

    return method;
} //_resolve_method


static bool method_can_link_static(Class* clss, unsigned index, Method* method, bool _throw) {
    ASSERT_RAISE_AREA;

    if (!method->is_static()) {
        if(_throw) {
            exn_raise_by_name("java/lang/IncompatibleClassChangeError",
                method->get_class()->name->bytes);
        }
        return false;
    }
    return true;
}

static Method* _resolve_static_method(Global_Env *env,
                                      Class *clss,
                                      unsigned cp_index)
{
    ASSERT_RAISE_AREA;

    Method* method = _resolve_method(env, clss, cp_index);
    if(method && !method_can_link_static(clss, cp_index, method, LINK_NO_THROW))
        return NULL;
    return method;
} //_resolve_static_method


static bool method_can_link_virtual(Class* clss, unsigned cp_index, Method* method, bool _throw)
{
    ASSERT_RAISE_AREA;

    if(method->is_static()) {
        if(_throw) {
            exn_raise_by_name("java/lang/IncompatibleClassChangeError",
                method->get_class()->name->bytes);
        }
        return false;
    }
    if(class_is_interface(method->get_class())) {
        if(_throw) {
            char* buf = (char*)STD_ALLOCA(clss->name->len
                + method->get_name()->len + method->get_descriptor()->len + 2);
            sprintf(buf, "%s.%s%s", clss->name->bytes,
                method->get_name()->bytes, method->get_descriptor()->bytes);
            jthrowable exc_object = exn_create("java/lang/AbstractMethodError", buf);
            exn_raise_object(exc_object);
        }
        return false;
    }
    return true;
}


static Method* _resolve_virtual_method(Global_Env *env,
                                       Class *clss,
                                       unsigned cp_index)
{
    Method* method = _resolve_method(env, clss, cp_index);
    if(method && !method_can_link_virtual(clss, cp_index, method, LINK_NO_THROW))
        return NULL;
    return method;
} //_resolve_virtual_method


static bool method_can_link_interface(Class* clss, unsigned cp_index, Method* method, bool _throw) {
    return true;
}


static Method* _resolve_interface_method(Global_Env *env,
                                         Class *clss,
                                         unsigned cp_index)
{
    Method* method = _resolve_method(env, clss, cp_index);
    if(method && !method_can_link_interface(clss, cp_index, method, LINK_NO_THROW)) {
        return NULL;
    }
    return method;
} //_resolve_interface_method


Field_Handle resolve_field(Compile_Handle h,
                           Class_Handle c,
                           unsigned index)
{
    return _resolve_field(compile_handle_to_environment(h), c, index);
} // resolve_field


//
// resolve constant pool reference to a non-static field
// used for getfield and putfield
//
Field_Handle resolve_nonstatic_field(Compile_Handle h,
                                     Class_Handle c,
                                     unsigned index,
                                     unsigned putfield)
{
    return _resolve_nonstatic_field(compile_handle_to_environment(h), c, index, putfield);
} //resolve_nonstatic_field


//
// resolve constant pool reference to a static field
// used for getstatic and putstatic
//
Field_Handle resolve_static_field(Compile_Handle h,
                                  Class_Handle c,
                                  unsigned index,
                                  unsigned putfield)
{
    return _resolve_static_field(compile_handle_to_environment(h), c, index, putfield);
} //resolve_static_field


Method_Handle resolve_method(Compile_Handle h, Class_Handle ch, unsigned idx)
{
    return _resolve_method(compile_handle_to_environment(h), ch, idx);
}


//
// resolve constant pool reference to a virtual method
// used for invokevirtual
//
Method_Handle resolve_virtual_method(Compile_Handle h,
                                     Class_Handle c,
                                     unsigned index)
{
    return _resolve_virtual_method(compile_handle_to_environment(h), c, index);
} //resolve_virtual_method


static bool method_can_link_special(Class* clss, unsigned index, Method* method, bool _throw)
{
    ASSERT_RAISE_AREA;

    unsigned class_idx = clss->const_pool[index].CONSTANT_ref.class_index;
    unsigned class_name_idx = clss->const_pool[class_idx].CONSTANT_Class.name_index;
    String* ref_class_name = clss->const_pool[class_name_idx].CONSTANT_String.string;

    if(method->get_name() == VM_Global_State::loader_env->Init_String
        && method->get_class()->name != ref_class_name)
    {
        if(_throw) {
            exn_raise_by_name("java/lang/NoSuchMethodError",
                method->get_name()->bytes);
        }
        return false;
    }
    if(method->is_static())
    {
        if(_throw) {
            exn_raise_by_name("java/lang/IncompatibleClassChangeError",
                method->get_class()->name->bytes);
        }
        return false;
    }
    if(method->is_abstract())
    {
        if(_throw) {
            tmn_suspend_enable();
            unsigned buf_size = clss->name->len + method->get_name()->len + method->get_descriptor()->len + 5;
            char* buf = (char*)STD_ALLOCA(buf_size);
            memset(buf, 0, buf_size);
            sprintf(buf, "%s.%s%s", clss->name->bytes, method->get_name()->bytes, method->get_descriptor()->bytes);
            jthrowable exc_object = exn_create("java/lang/AbstractMethodError", buf);
            exn_raise_object(exc_object);
            tmn_suspend_disable();
        }
        return false;
    }
    return true;
}

//
// resolve constant pool reference to a method
// used for invokespecial
//
Method_Handle resolve_special_method_env(Global_Env *env,
                                         Class_Handle curr_clss,
                                         unsigned index)
{
    ASSERT_RAISE_AREA;

    Method* method = _resolve_method(env, curr_clss, index);
    if(!method) {
        return NULL;
    }
    if(class_is_super(curr_clss)
        && is_class_extended_class(curr_clss->super_class, method->get_class())
        && method->get_name() != env->Init_String)
    {
        Method* result_meth;
        for(Class* clss = curr_clss->super_class; clss; clss = clss->super_class)
        {
            result_meth = class_lookup_method(clss, method->get_name(), method->get_descriptor());
            if(result_meth) {
                method = result_meth;
                break;
            }
        }
    }
    if(method && !method_can_link_special(curr_clss, index, method, false))
        return NULL;
    return method;
} //resolve_special_method_env


//
// resolve constant pool reference to a method
// used for invokespecial
//
Method_Handle resolve_special_method(Compile_Handle h,
                                     Class_Handle c,
                                     unsigned index)
{
    return resolve_special_method_env(compile_handle_to_environment(h), c, index);
} //resolve_special_method



//
// resolve constant pool reference to a static method
// used for invokestatic
//
Method_Handle resolve_static_method(Compile_Handle h,
                                    Class_Handle c,
                                    unsigned index) 
{
    return _resolve_static_method(compile_handle_to_environment(h), c, index);
} //resolve_static_method



//
// resolve constant pool reference to a method
// used for invokeinterface
//
Method_Handle resolve_interface_method(Compile_Handle h,
                                       Class_Handle c,
                                       unsigned index) 
{
    return _resolve_interface_method(compile_handle_to_environment(h), c, index);
} //resolve_interface_method


//
// resolve constant pool reference to a class
// used for
//      (1) new 
//              - InstantiationError exception if resolved class is abstract
//      (2) anewarray
//      (3) checkcast
//      (4) instanceof
//      (5) multianewarray
//
// resolve_class_new is used for resolving references to class entries by the
// the new byte code.
//
Class_Handle resolve_class_new(Compile_Handle h,
                               Class_Handle c,
                               unsigned index) 
{
    return _resolve_class_new(compile_handle_to_environment(h), c, index);
} //resolve_class_new



//
// resolve_class is used by all the other byte codes that reference classes.
//
Class_Handle resolve_class(Compile_Handle h,
                           Class_Handle c,
                           unsigned index) 
{
    return _resolve_class(compile_handle_to_environment(h), c, index);
} //resolve_class


void class_throw_linking_error(Class_Handle ch, unsigned index, unsigned opcode)
{
    ASSERT_RAISE_AREA;

    Const_Pool* cp = ch->const_pool;
    if(cp_in_error(cp, index)) {
        exn_raise_object((jthrowable)(&(cp[index].error.cause)));
        return; // will return in interpreter mode
    }

    switch(opcode) {
        case OPCODE_NEW:
            class_can_instantiate(cp[index].CONSTANT_Class.klass, LINK_THROW_ERRORS);
            break;
        case OPCODE_PUTFIELD:
            field_can_link(ch, cp[index].CONSTANT_ref.field,
                CAN_LINK_FROM_FIELD, LINK_WRITE_ACCESS, LINK_THROW_ERRORS);
            break;
        case OPCODE_GETFIELD:
            field_can_link(ch, cp[index].CONSTANT_ref.field,
                CAN_LINK_FROM_FIELD, LINK_READ_ACCESS, LINK_THROW_ERRORS);
            break;
        case OPCODE_PUTSTATIC:
            field_can_link(ch, cp[index].CONSTANT_ref.field,
                CAN_LINK_FROM_STATIC, LINK_WRITE_ACCESS, LINK_THROW_ERRORS);
            break;
        case OPCODE_GETSTATIC:
            field_can_link(ch, cp[index].CONSTANT_ref.field,
                CAN_LINK_FROM_STATIC, LINK_READ_ACCESS, LINK_THROW_ERRORS);
            break;
        case OPCODE_INVOKEINTERFACE:
            method_can_link_interface(ch, index, cp[index].CONSTANT_ref.method,
                LINK_THROW_ERRORS);
            break;
        case OPCODE_INVOKESPECIAL:
            method_can_link_special(ch, index, cp[index].CONSTANT_ref.method,
                LINK_THROW_ERRORS);
            break;
        case OPCODE_INVOKESTATIC:
            method_can_link_static(ch, index, cp[index].CONSTANT_ref.method,
                LINK_THROW_ERRORS);
            break;
        case OPCODE_INVOKEVIRTUAL:
            method_can_link_virtual(ch, index, cp[index].CONSTANT_ref.method,
                LINK_THROW_ERRORS);
            break;
        default:
            // FIXME Potentially this can be any RuntimeException or Error
            // The most probable case is OutOfMemoryError.
            WARN("**Java exception occured during resolution under compilation");
            exn_raise_object(VM_Global_State::loader_env->java_lang_OutOfMemoryError);
            //ASSERT(0, "Unexpected opcode: " << opcode);
            break;
    }
}

Class *resolve_class_array_of_class1(Global_Env *env,
                                     Class *cc)
{
    // If the element type is primitive, return one of the preloaded
    // classes of arrays of primitive types.
    if (cc->is_primitive) {
        if (cc == env->Boolean_Class) {
            return env->ArrayOfBoolean_Class;
        } else if (cc == env->Byte_Class) {
            return env->ArrayOfByte_Class;
        } else if (cc == env->Char_Class) {
            return env->ArrayOfChar_Class;
        } else if (cc == env->Short_Class) {
            return env->ArrayOfShort_Class;
        } else if (cc == env->Int_Class) {
            return env->ArrayOfInt_Class;
        } else if (cc == env->Long_Class) {
            return env->ArrayOfLong_Class;
        } else if (cc == env->Float_Class) {
            return env->ArrayOfFloat_Class;
        } else if (cc == env->Double_Class) {
            return env->ArrayOfDouble_Class;
        }
    }

    char *array_name = (char *)STD_MALLOC(cc->name->len + 5);
    if(cc->name->bytes[0] == '[') {
        sprintf(array_name, "[%s", cc->name->bytes);
    } else {
        sprintf(array_name, "[L%s;", cc->name->bytes);
    }
    String *arr_str = env->string_pool.lookup(array_name);
    STD_FREE(array_name);

    Class* arr_clss = cc->class_loader->LoadVerifyAndPrepareClass(env, arr_str);

    return arr_clss;
} //resolve_class_array_of_class1

//
// Given a class handle cl construct a class handle of the type
// representing array of cl.
//
Class_Handle class_get_array_of_class(Class_Handle cl)
{
    Global_Env *env = VM_Global_State::loader_env;
    Class *arr_clss = resolve_class_array_of_class1(env, cl);
    assert(arr_clss || exn_raised());

    return arr_clss;
} //class_get_array_of_class


static bool resolve_const_pool_item(Global_Env *env, Class *clss, unsigned cp_index)
{
    Const_Pool *cp = clss->const_pool;
    unsigned char *cp_tags = cp[0].tags;

    if (cp_is_resolved(cp, cp_index)) {
        return true;
    }

    switch (cp_tags[cp_index]) {
        case CONSTANT_Class:
            return _resolve_class(env, clss, cp_index);
        case CONSTANT_Fieldref:
            return _resolve_field(env, clss, cp_index);
        case CONSTANT_Methodref:
            return _resolve_method(env, clss, cp_index);
        case CONSTANT_InterfaceMethodref:
            return _resolve_method(env, clss, cp_index);
        case CONSTANT_NameAndType: // fall through
        case CONSTANT_Utf8:
            return true;
        case CONSTANT_String: // fall through
        case CONSTANT_Float: // fall through
        case CONSTANT_Integer:
            return true;
        case CONSTANT_Double: // fall through
        case CONSTANT_Long:
            return true;
    }
    return false;
}


/**
 * Resolve whole constant pool
 */
unsigned resolve_const_pool(Global_Env& env, Class *clss) {
    Const_Pool *cp = clss->const_pool;

    // It's possible that cp is null when defining class on the fly
    if (!cp) return true;
    unsigned cp_size = clss->cp_size;

    for (unsigned i = 1; i < cp_size; i++) {
        if(!resolve_const_pool_item(&env, clss, i)) {
            return i;
        }
    }
    return 0xFFFFFFFF;
} //resolve_const_pool
