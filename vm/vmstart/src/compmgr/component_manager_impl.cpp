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
 * @author Alexei Fedotov
 * @version $Revision: 1.1.2.2.4.5 $
 */  

#include <apr_atomic.h>
#include <apr_lib.h>
#include <apr_strings.h>

#include "open/compmgr.h"
#include "component_manager_impl.h"

#define LOG_DOMAIN "compmgr"
#include "cxxlog.h"

/*
 * Private variables and functions. See public fuctions
 * at the end of file.
 */

#ifndef NDEBUG
/**
 * Instance sanity check.
 */
static int
is_instance_valid(OpenInstanceHandle instance) {
    return apr_isdigit(*(instance->intf->GetVersion()));
}
#endif

#define ASSERT_IS_INSTANCE_VALID(instance) ASSERT(is_instance_valid(instance), \
    "Instance is not properly initialized, " \
    "the first strucutre element should point to a default interface table");

/**
 * Lock which synchronizes all component manager operations.
 *
 * There is only one component manager instance when several
 * virtual machines coexist in a single process. We synchronize these
 * instances by means of a global lock which is never destroyed.
 *
 * Since component manager does not expect many calls, one lock for
 * all operations is ok from performance perspective.
 */
static apr_thread_rwlock_t* global_lock = NULL;

/**
 * A pool <code>global_lock</code> is allocated from.
 */
static apr_pool_t* global_pool = NULL;

/**
 * A global component manager.
 */
static _ComponentManagerImpl* component_manager_impl = NULL;

/**
 * The length of a preallocated buffer for an error message.
 */
#define MAX_ERROR_BUFFER_SIZE 1024

/**
 * Get a component information structure for a component
 * with a given name.
 * @param[out] p_component_info on return, points to a handle of
 * a component information structure
 * @param name component name
 */
static int
GetComponentInfo(ComponentInfoHandle* p_component_info,
                 const char* name) {
    ComponentInfoHandle component_info = component_manager_impl->components;
    while (component_info) {
        if (!strcmp(name, component_info->component->GetName())) {
            /* Found a component information structure */
            *p_component_info = component_info;
            return APR_SUCCESS;
        }
        component_info = component_info->next;
    }
    return APR_NOTFOUND;
}

/**
 * Checks if DLL is still used by registered components.
 * @param lib DLL handle
 * @return APR_SUCCESS if DLL is used by some registered component,
 * APR_NOTFOUND otherwise
 */
static int
IsDllInUse(const DllHandle lib) {
    ComponentInfoHandle component_info = component_manager_impl->components;
    while (component_info) {
        if (component_info->declaring_library == lib) {
            /* Found a component information structure,
               which refers to the library */
            return APR_SUCCESS;
        }
        component_info = component_info->next;
    }
    return APR_NOTFOUND;
}

/**
 * Allocate an instance info structure and add it
 * to a component manager list.
 */
static int
AddInstance(OpenInstanceHandle instance,
            ComponentInfoHandle component_info,
            apr_pool_t* pool) {
    TRACE("Cm.AddInstance()"); 
    ASSERT_IS_INSTANCE_VALID(instance);

    _InstanceInfo* instance_info =
        (_InstanceInfo*) apr_palloc(pool, sizeof(_InstanceInfo));

    if (NULL == instance_info) {
        /* Out of memory */
        return APR_ENOMEM;
    }
    instance_info->pool = pool;
    instance_info->instance = instance;
    instance_info->component_info = component_info;

    instance_info->next = component_info->instances;
    ((struct _ComponentInfo*) component_info)->instances = instance_info;
    return APR_SUCCESS;
}

/**
 * Unregister an instance and return a pointer to a correspondent
 * instance information structure.
 * @param[out] p_instance_info on return, points to <code>InstanceInfoHandle</code>
 * handle which corresponds to the unregisterd instance
 * @param instance a handle of an instance to be deleted
 * @return APR_SUCCESS if successful, or APR_NOTFOUND if the instance
 * cannot be found
 */
static int
RemoveInstanceInfo(InstanceInfoHandle* p_instance_info,
                   OpenInstanceHandle instance) {

    ComponentInfoHandle component_info;
    GetComponentInfo(&component_info, instance->intf->GetName());

    _InstanceInfo** p_instance_info_next =
        &(((struct _ComponentInfo*) component_info)->instances);
    _InstanceInfo* instance_info = component_info->instances;
    while (instance_info) {
        if (instance_info->instance == instance) {
            *p_instance_info_next = instance_info->next;
            *p_instance_info = instance_info;
            return APR_SUCCESS;
        }
        p_instance_info_next = &(instance_info->next);
        instance_info = instance_info->next;
    }
    return APR_NOTFOUND;
}

/**
 * Unregister a component and return a pointer to a correspondent
 * component information structure.
 * @param[out] p_component_info on return, points to
 * <code>ComponentInfoHandle</code> handle which correspond to the
 * unregisterd component
 * @param name a component name to be unregistered
 * @return APR_SUCCESS if successful, or APR_NOTFOUND if the component
 * cannot be found
 */
static int
RemoveComponentInfo(ComponentInfoHandle* p_component_info,
                     const char* name) {
    _ComponentInfo** p_component_info_next =
        &component_manager_impl->components;
    _ComponentInfo* component_info = component_manager_impl->components;
    while (component_info) {
        if (!strcmp(component_info->component->GetName(),
            name)) {
            *p_component_info_next = component_info->next;
            *p_component_info = component_info;
            return APR_SUCCESS;
        }
        p_component_info_next = &(component_info->next);
        component_info = component_info->next;
    }
    return APR_NOTFOUND;
}

/**
 * Deallocates an instance and the correspondent instance
 * information structure.
 */
static int
FreeInstanceInfo(InstanceInfoHandle instance_info) {
    int ret = instance_info->component_info->
        instance_allocator->FreeInstance(instance_info->instance);
    apr_pool_destroy(instance_info->pool);
    return ret;
}

/**
 * Instance deallocation sequence.
 * <ol>
 * <li>Find a corresponding <code>InstanceInfoHandle</code> structure, and a
 * previous structure from the list</li>
 * <li>Remove the structure from the list</li>
 * <li>Get the <code>ComponentInfoHandle</code> strucutre</li>
 * <li>Use <code>instance_allocator</code> to deallocate an instance</li>
 * <li>Deallocate <code>InstanceInfoHandle</code> by freeing a corresponding pool</li>
 * </ol>
 */
static int
RemoveAndFreeInstanceInfo(OpenInstanceHandle instance) {
    InstanceInfoHandle instance_info;
    int ret = RemoveInstanceInfo(&instance_info, instance);
    if (APR_SUCCESS != ret) {
        return ret;
    }

    return FreeInstanceInfo(instance_info);
}

/**
 * Tries to remove and free all component instances.
 * @return APR_SUCCESS if instances are freed successfully, or
 * the first error code.
 */
static int
FreeComponentInstances(ComponentInfoHandle component_info) {
    int ret = APR_SUCCESS;

    _InstanceInfo* instance_info = component_info->instances;
    while (instance_info) {
        _InstanceInfo* instance_info_next = instance_info->next;
        int ret_new = FreeInstanceInfo(instance_info);
        if (APR_SUCCESS == ret) { /* Store the first error code */
            ret = ret_new;
        }
        instance_info = instance_info_next;
    }
    return ret;
}

/**
 * Removes a given library from a component manager library list.
 *
 * @param lib library handle
 * @return APR_SUCCESS if successful, or a non-zero error code
 */
static int
RemoveLib(DllHandle lib) {
    _Dll** p_library_next =
        &(component_manager_impl->libraries);
    _Dll* library = component_manager_impl->libraries;
    while (library) {
        if (library == lib) {
            *p_library_next = lib->next;
            return APR_SUCCESS;
        }
        p_library_next = &(library->next);
        library = library->next;
    }
    return APR_NOTFOUND;
}

/**
 * Removes the library from a component manager library list and
 * unloads it.
 * @param lib library handle
 * @return APR_SUCCESS if successful, or a non-zero error code
 */
static int
UnloadLib(DllHandle lib) {
    int ret = RemoveLib(lib);
    if (APR_SUCCESS != ret) {
        return ret;
    }
    ret = apr_dso_unload(lib->descriptor);
    apr_pool_destroy(lib->pool);
    return ret;
}

static int
ReleaseLib(DllHandle lib) {
    int ret = IsDllInUse(lib);
    if (APR_NOTFOUND == ret) {
        /* DLL is not used anymore, unload it */
        return UnloadLib(lib);
    }
    return ret;
}

static int
FreeComponentInfo(ComponentInfoHandle component_info) {
    int ret = FreeComponentInstances(component_info);
    DllHandle declaring_library = component_info->declaring_library;
    apr_pool_destroy(component_info->pool);
    if (NULL != declaring_library) {
        ReleaseLib(declaring_library);
    }
    return ret;
}

static int
RemoveAndFreeComponentInfo(const char* component_name) {
    ComponentInfoHandle component_info;
    int ret = RemoveComponentInfo(&component_info, component_name);
    if (APR_SUCCESS != ret) {
        return ret;
    }

    return FreeComponentInfo(component_info);
}

static int
InitializeGlobalLock() {
   TRACE("Cm.InitializeGlobalLock()");
    apr_pool_t* pool;
    int ret = apr_pool_create(&pool, NULL);
    if (APR_SUCCESS != ret) {
        return ret;
    }

    apr_thread_rwlock_t* lock;
    ret = apr_thread_rwlock_create(&lock, pool);
    if (APR_SUCCESS != ret) {
        apr_pool_destroy(pool); /* Ignore errors */
        return ret;
    }

    /* Atomic replacement of a global component manager lock */
    void* old_value = apr_atomic_casptr((volatile void **) &global_lock,
        (void*) lock, NULL);

    if (NULL == old_value) {
        /* Successfully placed a lock to a static storage */
        global_pool = pool;
        return APR_SUCCESS;
    } else {
        /* The global lock already exists */

        /*
         * FIXME 
         * Currently apr_thread_rwlock_destroy is called automatically
         * from apr_pool_destroy.
         *
         * ret = apr_thread_rwlock_destroy(lock);
         * apr_pool_destroy(pool);
         * return ret;
         */
        apr_pool_destroy(pool);
        return APR_SUCCESS; 
    }
}

/*
 * The function deallocates a given component manager.
 * The caller should ensure that the component manager is not still used.
 */
static int
Destroy() {
    TRACE("Cm.Destroy()");
    int ret = APR_SUCCESS, ret_new;

    /* Deallocate all components */
    ComponentInfoHandle component_info = component_manager_impl->components;
    while (NULL != component_info) {
        ComponentInfoHandle component_info_next = component_info->next;
        ret_new = FreeComponentInfo(component_info_next);
        if (APR_SUCCESS == ret) {
            ret = ret_new;
        }
        component_info = component_info_next;
    }

    apr_pool_destroy(component_manager_impl->pool);
    component_manager_impl = NULL;
    return ret;
}

/**
 * Since <code>_Dll</code> is an internal component
 * manager structure we cannot expose this function
 * as a public interface.
 */
static int
AddComponent(OpenComponentInitializer init_func,
             DllHandle lib,
             apr_pool_t* parent_pool) {
    TRACE("Cm.AddComponent()");

    apr_pool_t* pool;
    int ret = apr_pool_create(&pool, parent_pool);
    if (APR_SUCCESS != ret) {
        return ret;
    }

    _ComponentInfo* component_info = (_ComponentInfo*)
        apr_pcalloc(pool, sizeof(_ComponentInfo));

    if (NULL == component_info) {
        /* Out of memory */
        apr_pool_destroy(pool);
        return APR_ENOMEM;
    }

    OpenComponentHandle component;
    ret = init_func(&component,
        &(component_info->instance_allocator),
        pool);
    if (APR_SUCCESS != ret) {
        apr_pool_destroy(pool);
        return ret;
    }
    component_info->component = component;
    component_info->pool = pool;
    component_info->declaring_library = lib;

    TRACE("Cm.AddComponent():\t" << component->GetName()
        << "-" << component->GetVersion()
        << " (Vendor: "
        << component->GetVendor()
        << ")\n"
        << "Description:\t"
        << component->GetDescription() << "\n"
        << "Interfaces:\t");

    const char** p_name = component->ListInterfaceNames();
    /*
     * String array is NULL terminated. We increase a string pointer until
     * it becomes NULL.
     */
    for(; *p_name; p_name++) {
        TRACE(" " << *p_name);
    }

    /* Add to the global component manager list */
    component_info->next = component_manager_impl->components;
    component_manager_impl->components = component_info;
    return APR_SUCCESS;
}

static int
GetOpenComponentInitializer(OpenComponentInitializer* p_init_func,
                            DllHandle lib,
                            const char* init_func_name) {
    return apr_dso_sym((apr_dso_handle_sym_t*) p_init_func,
        lib->descriptor, init_func_name);
}

static int
FindLibrary(DllHandle* p_lib, const char* path) {
    DllHandle lib = component_manager_impl->libraries;

    while (lib) {
        if (!strcmp(lib->path, path)) {
            *p_lib = lib;
            return APR_SUCCESS;
        }
        lib = lib->next;
    }
    return APR_NOTFOUND;
}

/**
 * Loads a library if it is not loaded and registers it in
 * a component manager.
 * @param[out] p_lib on return, points to a library handle
 * @param path platform independent library name
 * @return APR_SUCCESS if successful, or a non-zero error code
 */
static int
LoadLib(DllHandle* p_lib, const char* path) {
    TRACE("Cm.LoadLibrary(\"" << path << "\")");
    int ret = FindLibrary(p_lib, path);
    if (APR_SUCCESS == ret) {
        return APR_SUCCESS;
    }

    ASSERT(APR_NOTFOUND == ret, \
        "Unexpected return code from FindLibrary()");

    apr_pool_t* pool;
    ret = apr_pool_create(&pool, component_manager_impl->pool);
    if (APR_SUCCESS != ret) {
        return ret;
    }

    _Dll* lib = (_Dll*) apr_palloc(pool, sizeof(_Dll));
    if (NULL == lib) {
        apr_pool_destroy(pool);
        return APR_ENOMEM;
    }
    lib->pool = pool;

    /* strdup(lib->path, path); */
    apr_size_t len = strlen(path) + 1;
    lib->path = (char*) apr_palloc(pool, len);
    if (lib->path == NULL) {
        apr_pool_destroy(pool);
        return APR_ENOMEM;
    }
    memcpy(lib->path, path, len);

    ret = apr_dso_load(&lib->descriptor, path, pool);
    if (APR_SUCCESS != ret) {
        char buffer[MAX_ERROR_BUFFER_SIZE];
        apr_dso_error(lib->descriptor,
                buffer, MAX_ERROR_BUFFER_SIZE);
        TRACE("Error loading " << path << ": " \
            << buffer);
        apr_pool_destroy(pool);
        return ret;
    }

    lib->next = component_manager_impl->libraries;
    component_manager_impl->libraries = lib;
    *p_lib = lib;
    return APR_SUCCESS;
}

/*
 * The following functions can be accessed
 * by means of a component manager virtual table.
 */
static int
CmGetComponent(OpenComponentHandle* p_component,
               const char* name) {
    int ret = apr_thread_rwlock_rdlock(global_lock);
    if (APR_SUCCESS != ret) {
        return ret;
    }

    ComponentInfoHandle component_info;
    ret = GetComponentInfo(&component_info, name);
    if (APR_SUCCESS != ret) {
        apr_thread_rwlock_unlock(global_lock);
        return ret;
    }
    *p_component = component_info->component;
    return apr_thread_rwlock_unlock(global_lock);
}


static int
CmCreateInstance(OpenInstanceHandle* p_instance,
                    const char* name) {
    int ret = apr_thread_rwlock_wrlock(global_lock);
    if (APR_SUCCESS != ret) {
        return ret;
    }

    ComponentInfoHandle component_info;
    ret = GetComponentInfo(&component_info, name);
    if (APR_SUCCESS != ret) {
        apr_thread_rwlock_unlock(global_lock);
        return ret;
    }

    apr_pool_t* pool;
    ret = apr_pool_create(&pool, component_info->pool);
    if (APR_SUCCESS != ret) {
        apr_thread_rwlock_unlock(global_lock);
        return ret;
    }
    ret = component_info->instance_allocator->CreateInstance(p_instance, pool);
    if (APR_SUCCESS != ret) {
        apr_pool_destroy(pool);
        apr_thread_rwlock_unlock(global_lock);
        return ret;
    }

    ret = AddInstance(*p_instance, component_info, pool);
    if (APR_SUCCESS != ret) {
        component_info->instance_allocator->FreeInstance(*p_instance); /* Ignore errors */
        apr_pool_destroy(pool);
        apr_thread_rwlock_unlock(global_lock);
        return ret;
    }
    return apr_thread_rwlock_unlock(global_lock);
}


static int
CmFreeInstance(OpenInstanceHandle instance) {
    int ret = apr_thread_rwlock_wrlock(global_lock);
    if (APR_SUCCESS != ret) {
        return ret;
    }

    ret = RemoveAndFreeInstanceInfo(instance);

    int unlock_ret = apr_thread_rwlock_unlock(global_lock);
    return (APR_SUCCESS == ret) ? unlock_ret : ret;
}

/**
 * Allocates and fills component manager virtual table.
 */
static int
Create() {
    TRACE("Cm.Create()");

    apr_pool_t* pool;
    int ret = apr_pool_create(&pool, global_pool);
    if (APR_SUCCESS != ret) {
        return ret;
    }

    assert(NULL == component_manager_impl);
    component_manager_impl = (_ComponentManagerImpl*)
        apr_pcalloc(pool, sizeof(_ComponentManagerImpl));
    if (NULL == component_manager_impl) {
        /* Out of memory */
        return APR_ENOMEM;
    }

    component_manager_impl->num_clients = 1;
    component_manager_impl->cm.GetComponent = CmGetComponent;
    component_manager_impl->cm.CreateInstance = CmCreateInstance;
    component_manager_impl->cm.FreeInstance = CmFreeInstance;
    component_manager_impl->pool = pool;

    return APR_SUCCESS;
}


/*
 * Implementation of public functions.
 */
int
CmAcquire(OpenComponentManagerHandle* p_cm) {
    TRACE("Cm.Acquire()");
    InitializeGlobalLock();

    PORT_VERIFY_SUCCESS(apr_thread_rwlock_wrlock(global_lock));
    if (NULL == component_manager_impl) {
        int ret = Create();
        if (APR_SUCCESS != ret) {
            PORT_VERIFY_SUCCESS(apr_thread_rwlock_unlock(global_lock));
            return ret;
        }
    } else {
        component_manager_impl->num_clients++;
    }
    *p_cm = (OpenComponentManagerHandle) component_manager_impl;
    PORT_VERIFY_SUCCESS(apr_thread_rwlock_unlock(global_lock));
    return APR_SUCCESS;
}

int
CmRelease() {
    PORT_VERIFY_SUCCESS(apr_thread_rwlock_wrlock(global_lock));
    component_manager_impl->num_clients--;

    int ret = APR_SUCCESS;
    if (component_manager_impl->num_clients == 0) {
        ret = Destroy();
    }
    
    PORT_VERIFY_SUCCESS(apr_thread_rwlock_unlock(global_lock));
    return ret;
}

int
CmAddComponent(OpenComponentInitializer init_func) {
    TRACE("Cm.AddComponent()");
    PORT_VERIFY_SUCCESS(apr_thread_rwlock_wrlock(global_lock));

    int ret = AddComponent(init_func, NULL, component_manager_impl->pool);

    PORT_VERIFY_SUCCESS(apr_thread_rwlock_unlock(global_lock));
    return ret;
}

int
CmLoadComponent(const char* path,
                const char* init_func_name) {
    TRACE("Cm.LoadComponent(\"" << path
        << "\", " << init_func_name << "())");

    PORT_VERIFY_SUCCESS(apr_thread_rwlock_wrlock(global_lock));

    DllHandle lib;
    int ret = LoadLib(&lib, path);
    if (APR_SUCCESS != ret) {
        PORT_VERIFY_SUCCESS(apr_thread_rwlock_unlock(global_lock));
        return ret;
    }
    
    OpenComponentInitializer init_func;
    ret = GetOpenComponentInitializer(&init_func, lib, init_func_name);
    if (APR_SUCCESS != ret) {
        /* Ignore error */
        ReleaseLib(lib);
        PORT_VERIFY_SUCCESS(apr_thread_rwlock_unlock(global_lock));
        return ret;
    }

    ret = AddComponent(init_func, lib, lib->pool);
    if (APR_SUCCESS != ret) {
        /* Ignore error */
        ReleaseLib(lib);
        PORT_VERIFY_SUCCESS(apr_thread_rwlock_unlock(global_lock));
    }

    PORT_VERIFY_SUCCESS(apr_thread_rwlock_unlock(global_lock));
    return APR_SUCCESS;
}

int
CmFreeComponent(const char* component_name) {
    PORT_VERIFY_SUCCESS(apr_thread_rwlock_wrlock(global_lock));
    int ret = RemoveAndFreeComponentInfo(component_name);
    PORT_VERIFY_SUCCESS(apr_thread_rwlock_unlock(global_lock));
    return ret;
}

