/*
 *  Copyright 2006 The Apache Software Foundation or its licensors, as applicabl
e.
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
 * @author Intel, Pavel Pervov
 * @version $Revision: 1.6 $
 */  
#ifndef _VM_CLASS_LOADING_H
#define _VM_CLASS_LOADING_H

/**
 * @file
 * Class loading functionality of the class support interface. 
 * These functions are responsible for loading classes
 * from the virtual machine and interested components.
 */


/** 
 * @defgroup Extended VM Class Loading Extended Interface
 * The extended functionality is implemented on the basis of basic interfaces
 * and enables greater efficiency on the corresponding component side. 
 */

/**
 * Looks up the class only among the classes loaded by the given class loader. 
 * 
 * This class loader does not delegate the lookup operation to 
 * the parent loader or try to load any class.
 *
 * @param classloader    - the handle of the C++ class loader
 * @param name           - the name of the class to look up
 *
 * @return The handle for C++ class representation, if found. Otherwise, <code>NULL</code>.
 *
 * @note Replaces cl_get_class.
 */
Class_Handle
class_loader_lookup_class(Open_Class_Loader_Handle classloader, const char* name);


/**
 * Tries to load the class given its name and using the specified class loader.
 *
 * @param classloader    - the handle of the C++ class loader representation
 * @param name           - the name of the class to load
 *
 * @return The handle for the C++ class representation, if loaded successfully; otherwise, <code>NULL</code>.
 *
 * @note Replaces cl_load_class. 
 */
Class_Handle
class_loader_load_class(Open_Class_Loader_Handle classloader, const char* name);

/** @ingroup Extended 
 *
 * Tries to load the class given its name and using the bootstrap class loader.
 *
 * @param name  - the name of the class to load
 * @param exc   - the exception code for a class loading failure
 *
 * @result The handle for the C++ class representation, if loaded successfully; otherwise, <code>NULL</code>.
 *
 * @note Replaces class_load_class_by_name_using_system_class_loader. 
 */
Class_Handle
vm_load_class_with_bootstrap(const char* name);

/**
 * Returns the C++ class structure representing the system 
 * <code>java.lang.Object</code> class. 
 *
 * This function is the fast equivalent of the <code>vm_load_class_with_bootstrap("java/lang/Object")</code> function.
 *
 * @return the handle for the <code>java.lang.Object</code> C++ class representation. 
 *
 * @note Replaces get_system_object_class.
 */
Class_Handle
vm_get_java_lang_object_class();

/**
 * Returns the C++ class structure representing the system class
 * <code>java.lang.string</code>. 
 * 
 * This function is the fast equivalent of the <code>vm_load_class_with_bootstrap("java/lang/String")</code> function.
 *
 * @return The handle of <code>java.lang.String</code> C++ class representation
 *
 * @note Replaces get_system_string_class.
 */
Class_Handle
vm_get_java_lang_string_class();

/**
 * Stores the pointer to verifier-specific data into the class loader C++ structure. 
 *
 * @param classloader      - the handle to the class loader to set the verifier data in
 * @param data             - the pointer to the verifier data
 *
 * @note Replaces cl_set_verify_data_ptr.
 */
void
class_loader_set_verifier_data_ptr(Open_Class_Loader_Handle classloader, const void* data);

/**
 * Returns the pointer to verifier-specific data associated with the given class loader.
 *
 * @param classloader - the handle to class loader to retrieve verifier pointer from
 * 
 * @return The pointer to the verifier data
 *
 * @note Replaces cl_get_verify_data_ptr.
 */
void*
class_loader_get_verifier_data_ptr(Open_Class_Loader_Handle classloader);

/**
 * Acquires the lock on a given class loader. 
 *
 * @param classloader - the handle to the C++ class loader structure to acquire lock on.
 * 
 * @note Replaces cl_acquire_lock.
 */
void
class_loader_lock(Open_Ñlass_Loader_Handle classloader);

/**
 * Releases the lock on a given class loader. 
 *
 * @param classloader - the handle to the C++ class loader structure to release lock on. 
 * 
 * @note Replaces cl_acquire_lock.
 */
void
class_loader_unlock(Open_Class_Loader_Handle classloader);

#endif // _VM_CLASS_LOADING_H
