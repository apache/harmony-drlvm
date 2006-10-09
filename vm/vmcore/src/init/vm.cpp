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
 * @author Intel, Alexei Fedotov
 * @version $Revision: 1.1.2.3.4.4 $
 */  


#define LOG_DOMAIN "vm.core"
#include "cxxlog.h"

#include "platform_lowlevel.h"

#include <assert.h>

#include "vm_process.h"

#include "open/types.h"
#include "Class.h"
#include "classloader.h"
#include "environment.h"
#include "method_lookup.h"
#include "exceptions.h"
#include "compile.h"
#include "object_layout.h"
#include "open/vm_util.h"
#include "jit_intf.h"
#include "object_handles.h"
#include "vm_synch.h"
#include "vm_threads.h"
#include "vm_stats.h"
#include "vm_arrays.h"
#include "nogc.h"

#include "object_generic.h"

#include "open/hythread_ext.h"
#include "open/jthread.h"
#include "component_manager.h"
#include "lock_manager.h"
#include "root_set_enum_internal.h"

#include "natives_support.h"
#include "properties.h"


Global_Env *VM_Global_State::loader_env = 0;


// Set the default for compressed vtable pointers depending on the architecture.
#ifdef _IPF_
bool ManagedObject::_compressed = true;
#else // !_IPF_
bool ManagedObject::_compressed = false;
#endif // !_IPF_



/////////////////////////////////////////////////////////////////
// begin Class
/////////////////////////////////////////////////////////////////

Field* class_resolve_field(Class* clss, unsigned cp_index)
{
    Compilation_Handle ch;
    ch.env = VM_Global_State::loader_env;
    ch.jit = NULL;
    Field_Handle fh = resolve_field(&ch, (Class_Handle)clss, cp_index);
    return (Field *)fh;
}


Method *class_resolve_method(Class *clss, unsigned cp_index)
{
    Compilation_Handle ch;
    ch.env = VM_Global_State::loader_env;
    ch.jit = NULL;
    Method_Handle fh = resolve_method(&ch, (Class_Handle)clss, cp_index);
    return (Method *)fh;
} //class_resolve_static_field


Field *class_resolve_nonstatic_field(Class *clss, unsigned cp_index)
{
    Compilation_Handle ch;
    ch.env = VM_Global_State::loader_env;
    ch.jit = NULL;
    Field_Handle fh = resolve_field(&ch, (Class_Handle)clss, cp_index);
    if(!fh || field_is_static(fh))
        return NULL;
    return fh;
} //class_resolve_static_field


Class *class_resolve_class(Class *clss, unsigned cp_index)
{
    Compilation_Handle ch;
    ch.env = VM_Global_State::loader_env;
    ch.jit = NULL;
    return resolve_class(&ch, clss, cp_index);
} //class_resolve_class




// A run-time error occurs if called for an index which does not represent
// a constant of type Class, String, Integer, Float, Long or Double.
Java_Type class_get_cp_const_type(Class *clss, unsigned cp_index)
{
    Const_Pool *cp = clss->const_pool;
    switch(cp_tag(cp, cp_index)) {
    case CONSTANT_String:
        return JAVA_TYPE_STRING;
    case CONSTANT_Integer:
        return JAVA_TYPE_INT;
    case CONSTANT_Float:
        return JAVA_TYPE_FLOAT;
    case CONSTANT_Long:
        return JAVA_TYPE_LONG;
    case CONSTANT_Double:
        return JAVA_TYPE_DOUBLE;
    case CONSTANT_Class:
        return JAVA_TYPE_CLASS;
    default:
        DIE("non-constant type is requested from constant pool : " << cp_tag(cp, cp_index));
    }
    return JAVA_TYPE_INVALID;
} //class_get_cp_const_type



// Returns an address of an int, float, etc.
// For a string it returns a pointer to the utf8 representation of the string.
const void *class_get_addr_of_constant(Class *clss, unsigned cp_index)
{
    Const_Pool *cp = clss->const_pool;
    assert(cp_tag(cp, cp_index) != CONSTANT_String);

    return (void *)(cp + cp_index);
} //class_get_addr_of_constant

/////////////////////////////////////////////////////////////////
// end Class
/////////////////////////////////////////////////////////////////



static struct VmStandardProperty {
    const char *name;       // Full name of the property
    const char *docString;  // Documentation of the property
    bool        isBoolean;  // Whether it's a boolean-valued property
    Boolean     value;      // Value of a boolean-valued property (initialized to default value)
} standardProperties[] = {
    // Sorted only for convenience.
    {"gc.verbose",                  "Controls GC verbosity", 
                true, FALSE},
    {"vm.assert_dialog",            "If false, prevent assertion failures from popping up a dialog box.", 
                true, TRUE},
#ifdef PLATFORM_POSIX
    {"vm.crash_handler",            "Invoke gdb on crashes",
                true, FALSE},
#endif // PLATFORM_POSIX
    {"vm.finalize",                 "Run finalizers",
                true, TRUE},
    {"vm.jit_may_inline_sync",      "The JIT is allowed to inline part of the synchronization sequence.", 
                true, TRUE},
    {"vm.multiple_dumpjit_files",   "Split the dump file into multiple files with a maximum of approximated 10 million bytes", 
                true, FALSE},
    {"vm.use_interpreter",          "Use interpreter not jit.", 
                true, FALSE},
    {"vm.use_verifier",             "Use verifier.",
                true, TRUE},
    {"vm.jvmti.enabled",            "Whether JVMTI mode is enabled.",
                true, FALSE},
    {"vm.cleanupOnExit",             "Excplicitly free VM resources before exit.",
                true, FALSE},
    {"vm.bootclasspath.appendclasspath", "Append classpath to the bootclasspath",
                true, FALSE},

    // Non-boolean properties below.  (sorted for convenience)
    {"vm.boot.library.path",             "List of directories which contain additional dynamic libraries to load into VM"},
    {"vm.boot.class.path",               "Virtual machine bootclasspath"},
    {"vm.bootclasspath.initmethod",      "Set to \"java-home\" to use a JDK style bootclasspath based on java home."},
    {"vm.bootclasspath.prepend",         "Prepended to the JDK style booclasspath."},
    {"vm.dlls",                          "A ';'-delimited list of modular dlls (GC/etc.) to load at startup."},
    {"vm.ee_dlls",                       "A ';'-delimited list of modular dlls (JIT/Interpreter/etc.) to load at startup."},
    {"vm.em_dll",                        "A ';'-execution manager (EM) dll to load at startup."},
    {"vm.other_natives_dlls",            "A " EXPAND(PORT_PATH_SEPARATOR) "-delimited list of dlls contained native methods implementations to load at startup."},
};

static const int numStandardProperties = sizeof(standardProperties) / sizeof(standardProperties[0]);
static bool areStandardPropertiesSet = false;


void check_vm_standard_property(const char *propertyName, const char *propertyValue)
{
    for (int i=0; i<numStandardProperties; i++)
    {
        if (!strcmp(propertyName, standardProperties[i].name))
        {
            if (standardProperties[i].isBoolean)
            {
                areStandardPropertiesSet = true;
                if (!strcmp(propertyValue, "on") ||
                    !strcmp(propertyValue, "true") ||
                    !strcmp(propertyValue, "1"))
                    standardProperties[i].value = TRUE;
                else if (!strcmp(propertyValue, "off") ||
                    !strcmp(propertyValue, "false") ||
                    !strcmp(propertyValue, "0"))
                    standardProperties[i].value = FALSE;
                else
                {
                    fprintf(stderr,"check_vm_standard_property: invalid boolean value '%s' for property '%s'\n",
                        propertyValue, propertyName);
                    fflush(stdout);
                }
            }
            return;
        }
    }
    fprintf(stderr,"check_vm_standard_property: unknown standard property used: '%s'\n", propertyName);
    fflush(stderr);
}


// This must be called while properties still hold their default values,
// i.e. before any properties are loaded.
void print_vm_standard_properties()
{
    if (areStandardPropertiesSet)
    {
        printf("Warning: print_vm_standard_properties() called after properties are already set.\n");
    }
    printf("Boolean-valued properties (set to one of {on,true,1,off,false,0}):\n");
    int i;
    for (i=0; i<numStandardProperties; i++)
    {
        if (standardProperties[i].isBoolean)
        {
            printf("%s (default %s):\n  %s\n",
                standardProperties[i].name,
                (standardProperties[i].value ? "TRUE" : "FALSE"),
                standardProperties[i].docString);
        }
    }
    printf("\nOther properties:\n");
    for (i=0; i<numStandardProperties; i++)
    {
        if (!standardProperties[i].isBoolean)
        {
            printf("%s:\n  %s\n",
                standardProperties[i].name,
                standardProperties[i].docString);
        }
    }
    fflush(stdout);
}



Boolean vm_get_boolean_property_value_with_default(const char *property_name)
{
    bool found = false;
    for (int i=0; !found && i<numStandardProperties; i++)
    {
        if (!strcmp(standardProperties[i].name, property_name))
        {
            found = true;
            if (!standardProperties[i].isBoolean)
            {
                {
                    printf("vm_get_boolean_property_value_with_default: non-boolean property '%s'\n", property_name);
                    fflush(stdout);
                }
            }
            return standardProperties[i].value;
        }
    }
    // XXX- print error
    printf("vm_get_boolean_property_value_with_default: standard property '%s' not found\n", property_name);
    fflush(stdout);
    return FALSE;
} //vm_get_boolean_property_value_with_default

