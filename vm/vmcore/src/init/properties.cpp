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
 * @author Intel, Alexey V. Varlamov, Gregory Shimansky
 * @version $Revision: 1.1.2.2.4.14 $
 */  


#define LOG_DOMAIN "init.properties"
#include "cxxlog.h"

#include "properties.h"
#include "open/vm_util.h"
#include <apr_file_io.h>
#include <apr_file_info.h>
#include <apr_env.h>
#include <apr_strings.h>
#include "port_dso.h"
#include "port_filepath.h"
#include "port_sysinfo.h"

#define PROP_ENV_NAME  "VM_PROPERTIES"
#define PROP_FILE_NAME "vm.properties"
#define BOOT_PROPS_FILE_NAME "bootclasspath.properties"
#define BOOTCLASSPATH_PROP_NAME "bootclasspath"
#define BOOTCLASSPATH_KERNEL_JAR "kernel.jar"

#define MAX_PROP_LINE 5120

// local memory pool for temporary allocation
static apr_pool_t *prop_pool;

const char *
properties_get_string_property(PropertiesHandle prop, const char *key)
{
    Prop_Value *val = ((Properties *) prop)->get(key);
    if (!val)
        return NULL;
    return val->as_string();
}

static void
read_properties(char *fname, Properties & prop)
{
    apr_file_t *f;
    if (APR_SUCCESS != apr_file_open(&f, fname, APR_FOPEN_READ, 0, prop_pool))
        return;
    WARN("USAGE OF vm.properties FILE IS DEPRECATED. THIS FUNCTIONALITY WILL BE REMOVED SOON. ALL PROPERTIES SHOULD BE SET ON COMMAND LINE");
    char buf[MAX_PROP_LINE];
    while (!apr_file_eof(f) && !apr_file_gets(buf, MAX_PROP_LINE, f))
    {
        prop.add(buf);
    }
    apr_file_close(f);
}

void 
properties_set_string_property(PropertiesHandle ph, const char* key, const char* value) 
{
    Properties* p = (Properties*)ph;
    add_pair_to_properties(*p, key, value);
}

PropertiesIteratorHandle
properties_iterator_create(PropertiesHandle ph) 
{
    Properties* p = (Properties*)ph;
    Properties::Iterator* it = p->getNewIterator();
    it->next();
    return (PropertiesIteratorHandle)it;
}

void
properties_iterator_destroy(PropertiesHandle ph, PropertiesIteratorHandle pih) 
{
    Properties* p = (Properties*)ph;
    Properties::Iterator* it = (Properties::Iterator*)pih;
    p->destroyIterator(it);
}

Boolean 
properties_iterator_advance(PropertiesIteratorHandle pih) 
{
    Properties::Iterator* it = (Properties::Iterator*)pih;
    return it->next()!=NULL;
}

const char* 
properties_get_name(PropertiesIteratorHandle pih) 
{
    Properties::Iterator* it = (Properties::Iterator*)pih;
    Prop_entry* p = it->currentEntry();
    return p->key;
}

const char* 
properties_get_string_value(PropertiesIteratorHandle pih)
{
    
    Properties::Iterator* it = (Properties::Iterator*)pih;
    Prop_entry* p = it->currentEntry();
    return p->value->as_string();
}

char *predefined_propeties[] =
{
    // The following properties are added in define_undefined_predefined_properties
    // function: java.home, vm.boot.library.path, java.library.path, user.name,
    // user.home, java.io.tmpdir

    // Hardcoded properties
    "java.vm.specification.version=1.0",
    "java.vm.specification.vendor=Sun Microsystems Inc.",
    "java.vm.specification.name=Java Virtual Machine Specification",
    "java.specification.version=1.5",
    "java.specification.vendor=Sun Microsystems Inc.",
    "java.specification.name=Java Platform API Specification",
    "java.version=11.2.0",
    "java.vendor=Intel DRL",
    "java.vm.vendor=Intel DRL",
    "java.vm.version=11.2.0",
    "java.vm.name=DRLVM",
    "java.vm.info=no info",
    NULL
};



#define API_DLL1 "harmonyvm"
#define API_DLL2 "hythr"
#define API_DLL3 "hysig"
#define API_DLL4 "hyprt"
#define API_DLL5 "hyzlib"
#define API_DLL6 "hytext"
#define API_DLL7 "hynio"
#define API_DLL8 "vmi"
#define API_DLLA "hyluni"
#define API_DLLB "hyarchive"

#define GC_DLL "gc"
#define EM_DLL "em"

/**
 *  Compose a string of file names each of them beginning with path,
 *  names separated by PORT_PATH_SEPARATOR.  If patch is NULL, no path
 *  or separator willbe prefixed
 */
static char *compose_full_files_path_names_list(const char *path,
                                                const char **dll_names,
                                                const int names_number, 
                                                bool is_dll)
{
    char* full_name = "";
    for (int iii = 0; iii < names_number; iii++)
    {
        const char *tmp = dll_names[iii];
        if (is_dll) {
            tmp = port_dso_name_decorate(tmp, prop_pool);
        }
        
        /*
         *  if the path is non-null, prefix, otherwise do nothing
         *  to avoid the problem of "/libfoo.so" when we don't want
         *  a path attached
         */
         
        if (path != NULL) { 
	        tmp = port_filepath_merge(path, tmp, prop_pool);
        }
        
        full_name = apr_pstrcat(prop_pool, full_name, tmp, 
            (iii + 1 < names_number) ? PORT_PATH_SEPARATOR_STR : "", NULL);
    }

    return full_name;
}

int str_compare(const void *arg1, const void *arg2) {
    char *string1 = *(char**)arg1;
    char *string2 = *(char**)arg2;
    if (strlen(string1) < strlen(string2))
        return -1;
    if (strlen(string2) < strlen(string1))
        return 1;
    return strcmp(string1, string2);
}

static char *load_full_api_files_path_names_list(const char *path)
{
    char *full_name = "";
    int array_size = 30;
    char **props = (char**)STD_MALLOC(array_size*sizeof(char*));
    char *jre_file_path = apr_pstrcat(prop_pool, path, PORT_FILE_SEPARATOR_STR BOOT_PROPS_FILE_NAME, NULL);
    apr_file_t *f_jre;
    if (APR_SUCCESS == apr_file_open(&f_jre, jre_file_path, APR_FOPEN_READ, 0, prop_pool))
    {
        char jre_file_name[MAX_PROP_LINE];
        int props_count = 0;
        while (!apr_file_eof(f_jre) && !apr_file_gets(jre_file_name, MAX_PROP_LINE, f_jre)) {
	        if ((jre_file_name[0] != 0x0D || jre_file_name[0] != 0x0A)
				&& !strncmp(jre_file_name ,BOOTCLASSPATH_PROP_NAME, strlen(BOOTCLASSPATH_PROP_NAME))) {
                char *char_pos = jre_file_name + strlen(BOOTCLASSPATH_PROP_NAME);
                // Check that there are digits after dots so only bootclasspath
                // elements appear in the property
                if (char_pos[0] != '.' || char_pos[1] < '0' || char_pos[1] > '9')
                    continue;
                
                if(props_count == array_size) {
                    array_size *= 2;
                    props = (char**)STD_REALLOC(props, array_size*sizeof(char*));
                }
                unsigned length;
                char_pos = strchr(jre_file_name, 0x0D);
                if (!char_pos) {
                    char_pos = strchr(jre_file_name, 0x0A);
                }
                if (char_pos) {
                    *char_pos = '\0';
                    length = char_pos - jre_file_name + 1;
                } else {
                    length = strlen(jre_file_name) + 1;
                }
                char_pos = strchr(jre_file_name, '=');
                if (!char_pos) {
                    DIE("Malformed bootclasspath entry: " << jre_file_name);
                }
                *char_pos = '\0';
                props[props_count] = (char*)STD_MALLOC(length * sizeof(char));
                memcpy(props[props_count], jre_file_name, length);
                props_count++;
            }
	    }
    
		qsort(props, props_count, sizeof(char*), str_compare);
        
        full_name = apr_pstrcat(prop_pool, path, PORT_FILE_SEPARATOR_STR,
            BOOTCLASSPATH_KERNEL_JAR, NULL);
            
        TRACE2("init", "kernel jar path : " << full_name);
        
        for(int i = 0; i < props_count; i++){
            full_name = apr_pstrcat(prop_pool, full_name, PORT_PATH_SEPARATOR_STR,
                path, PORT_FILE_SEPARATOR_STR, props[i] + strlen(props[i]) + 1, NULL);
            STD_FREE(props[i]);
        }
        STD_FREE(props);
        apr_file_close(f_jre);
    }
    else
    {
        DIE("Can't find file : " << jre_file_path);
    }
	return full_name;
}


void post_initialize_ee_dlls(PropertiesHandle ph) {
    if (!prop_pool) {
        apr_pool_create(&prop_pool, 0);
    }

    Properties *properties = (Properties*) ph;

    const char *file_list;
    if ((file_list = properties_get_string_property(ph, "vm.em_dll"))) {
        TRACE( "vm.em_dll = " << file_list);
        return; // properties already set (may be in command line)
    }

    const char *library_path =
        properties_get_string_property(ph, "vm.boot.library.path");

    const char *em_dll_files[] = {
        EM_DLL
    };
    const char ** dll_files= em_dll_files;
    const int n_dll_files = sizeof(dll_files) / sizeof(char *);

	/*
	 *  pass NULL for the pathname as we don't want 
	 *  any path pre-pended
	 */

    char* path_buf = compose_full_files_path_names_list(
            NULL, dll_files, n_dll_files, true);

    assert(path_buf);
    add_pair_to_properties(*properties, "vm.em_dll", path_buf);
    TRACE( "vm.em_dll = " << path_buf);
    
    apr_pool_destroy(prop_pool);
    prop_pool = 0;
}



static void define_undefined_predefined_properties(Properties & properties)
{
    char *path_buf = NULL;
    char *base_path_buf;

    if (port_executable_name(&base_path_buf, prop_pool) != APR_SUCCESS) {
        DIE("Failed to find executable location");
    }

    // directory for the executable
    char *p = strrchr(base_path_buf, PORT_FILE_SEPARATOR);
    if (NULL == p)
        DIE("Failed to determine executable directory");
    *p = '\0';
    
    /*
     *  vm.boot.library.path initialization, the value is the location of VM executable
     * 
     *  2006-09-06 gmj :  there's no reason to ever believe this is true given how the VM can be 
     *  launched in a mariad of ways, so just set to empty string.
     */
    add_pair_to_properties(properties, "vm.boot.library.path", "");
    TRACE( "vm.boot.library.path = " << base_path_buf);
    
    // Added for compatibility with the external java JDWP agent
    add_pair_to_properties(properties, "sun.boot.library.path", base_path_buf);
    TRACE( "sun.boot.library.path = " << base_path_buf);

    // vm.dlls initialization, the value is location of VM executable +
    // GC_DLL and on IPF JIT_DLL
    // FIXME, add JIT_DLL on other architectures when we have JIT in a dll on ia32
    const char *core_dll_files[] =
    {
        GC_DLL
    };

    const int n_core_dll_files = sizeof(core_dll_files) / sizeof(char *);

    // vm.other_natives_dlls initialization, the value is location of VM executable.
    const char *api_dll_files[] =
    {
        API_DLL1,
        API_DLL2,
        API_DLL3,
        API_DLL4,
        API_DLL5,
        API_DLL6,
        API_DLL7,
        API_DLL8,
        API_DLLA,
        API_DLLB
    };
    int n_api_dll_files = sizeof(api_dll_files) / sizeof(char *);


	/*
	 *  pass NULL for the pathname as we don't want 
	 *  any path pre-pended
	 */
    path_buf = compose_full_files_path_names_list(
            NULL, core_dll_files, n_core_dll_files, true);

    add_pair_to_properties(properties, "vm.dlls", path_buf);
    TRACE( "vm.dlls = " << path_buf);

	/*
	 *  pass NULL for the pathname as we don't want 
	 *  any path pre-pended
	 */

    path_buf = compose_full_files_path_names_list(
            NULL, api_dll_files, n_api_dll_files, true);

    add_pair_to_properties(properties, "vm.other_natives_dlls", path_buf);
    TRACE( "vm.other_natives_dlls = " << path_buf);

    // java.library.path initialization, the value is the location of VM executable,
    // prepended to OS library search path
    char *env;
    char *lib_path = base_path_buf;
    if (APR_SUCCESS == port_dso_search_path(&env, prop_pool))
    {
        lib_path = apr_pstrcat(prop_pool, base_path_buf, PORT_PATH_SEPARATOR_STR, env, NULL);
    }
        
    add_pair_to_properties(properties, "java.library.path", lib_path);
    TRACE( "java.library.path = " << lib_path);

    // java.home initialization, try to find absolute location of the executable and set
    // java.home to the parent directory.
    p =  strrchr(base_path_buf, PORT_FILE_SEPARATOR);
    if (NULL == p)
        DIE("Failed to determine executable parent directory");
    *p = '\0';
    add_pair_to_properties(properties, "java.home", base_path_buf);
    TRACE( "java.home = " << base_path_buf);

    char *ext_path = port_filepath_merge(base_path_buf, "lib" PORT_FILE_SEPARATOR_STR "ext", prop_pool);
    add_pair_to_properties(properties, "java.ext.dirs", ext_path);
    TRACE( "java.ext.dirs = " << ext_path);

    // vm.boot.class.path initialization. Value is
    // java.home PATH_SEPARATOR lib PATH_SEPARATOR API_CLASSES_ARCHIVE
    // where API_CLASSES_ARCHIVE is defined in this file
    char *boot_path = port_filepath_merge(base_path_buf, "lib" PORT_FILE_SEPARATOR_STR "boot", prop_pool);
	
    path_buf = load_full_api_files_path_names_list(boot_path);
    add_pair_to_properties(properties, "vm.boot.class.path", path_buf);
    TRACE( "vm.boot.class.path = " << path_buf);

    // Added for compatibility with a reference JDWP agent
    add_pair_to_properties(properties, "sun.boot.class.path", path_buf);
    TRACE( "sun.boot.class.path = " << path_buf);

    // user.name initialization, try to get the name from the system
    char *user_buf;
    apr_status_t status = port_user_name(&user_buf, prop_pool);
    if (APR_SUCCESS != status) {
        DIE("Failed to get user name from the system. Error code " << status);
    }
    add_pair_to_properties(properties, "user.name", user_buf);
    TRACE( "user.name = " << user_buf);

    // user.home initialization, try to get home from the system.
    char *user_home;
    status = port_user_home(&user_home, prop_pool);
    if (APR_SUCCESS != status) {
        DIE("Failed to get user home from the system. Error code " << status);
    }
    add_pair_to_properties(properties, "user.home", user_home);
    TRACE( "user.home = " << user_home);

    // java.io.tmpdir initialization. 
    const char *tmpdir;
    status = apr_temp_dir_get(&tmpdir, prop_pool);
    if (APR_SUCCESS != status) {
        tmpdir = user_home;
    }

    add_pair_to_properties(properties, "java.io.tmpdir", tmpdir);
    TRACE( "java.io.tmpdir = " << tmpdir);

    // FIXME: This is a workaround code for third party APIs which
    // depend on this property.
    add_pair_to_properties(properties, "java.util.prefs.PreferencesFactory",
#ifdef PLATFORM_NT
        "java.util.prefs.RegistryPreferencesFactoryImpl");
#else
        "java.util.prefs.FilePreferencesFactoryImpl");
#endif

    // user.timezone initialization, required by java.util.TimeZone implementation
    char *user_tz;
    status = port_user_timezone(&user_tz, prop_pool);
    if (APR_SUCCESS != status) {
        INFO("Failed to get user timezone from the system. Error code " << status);
        user_tz = "GMT";
    }
    add_pair_to_properties(properties, "user.timezone", user_tz);
    TRACE( "user.timezone = " << user_tz);
}

void
initialize_properties(Global_Env * p_env, Properties & prop)
{
    if (!prop_pool) {
        apr_pool_create(&prop_pool, 0);
    }
/*
 * gregory -
 * 0. Add predefined properties from property table
 */
    define_undefined_predefined_properties(p_env->properties);

    char **pp = predefined_propeties;
    while (NULL != *pp)
    {
        p_env->properties.add(*pp);
        pp++;
    }

/* 
 * 1. VM looks for an environment variable, say, 
 *    VM_PROPERTIES=d:\xyz\eee\vm.Properties, read the Properties;
 */
    char *pf;
    if (apr_env_get(&pf, PROP_ENV_NAME, prop_pool) == APR_SUCCESS){
        read_properties(pf, prop);
    }
/*
 * 2. Looks for vm.Properties in the directory where vm executable resides 
 *    (it's also a global file), read the Properties, if key is duplicated, 
 *    override the value;
 */
    char *buf;
    if (port_executable_name(&buf, prop_pool) == APR_SUCCESS)
    {
        char *p = strrchr(buf, PORT_FILE_SEPARATOR);
        if (p)
        {
            *(p + 1) = '\0';
            buf = apr_pstrcat(prop_pool, buf, PROP_FILE_NAME, NULL);
            read_properties(buf, prop);
        }
    }

/*
 * 3. Looks for it in current directory(it's an app-specific file), read the 
 *    Properties, if key is duplicated, override the value;
 */
    apr_filepath_get(&buf, 0, prop_pool);
    buf = port_filepath_merge(buf, PROP_FILE_NAME, prop_pool);
    read_properties(buf, prop);
/*
 * 4. Check whether there is a command line option, say, 
 *    -Properties-file "d:\xyz\eee\vm.Properties" or -Droperties key=value, 
 *    read the Properties, if key is duplicated, override the value. 
 */
    for (int arg_num = 0; arg_num < p_env->vm_arguments.nOptions; arg_num++)
    {
        char *option = p_env->vm_arguments.options[arg_num].optionString;

        if (strncmp(option, "-D", 2) == 0)
        {
            TRACE("setting property " << option + 2);
            add_to_properties(p_env->properties, option + 2);
        }
    }
    apr_pool_clear(prop_pool);
}

void
add_to_properties(Properties & prop, const char *line)
{
    prop.add(line);
}

void
add_pair_to_properties(Properties & prop, const char *key, const char *value)
{
    Prop_entry *e = new Prop_entry();
    char* key_copy = strdup(key);
    char* value_copy = strdup(value);
    e->key = strdup(unquote(key_copy));
    e->value = new Prop_String(strdup(unquote(value_copy)));
    prop.add(e);
    free(key_copy);
    free(value_copy);
}

void
Properties::add(Prop_entry * e) /*Caller can NOT delete e */
{
    const char *vmPrefix = "vm.";
    if (!strncmp(vmPrefix, e->key, strlen(vmPrefix)))
    {
        check_vm_standard_property(e->key, e->value->as_string());
    }

    int idx = index_of(e->key);
    if (bucket[idx])
    {
        Prop_entry *itr = bucket[idx];
        Prop_entry *prev = NULL;
        while (itr)
        {
            int cmp = itr->compareTo(e->key);
            if (cmp > 0)
            {
                e->next = itr;
                if (!prev)
                    bucket[idx] = e;
                else
                    prev->next = e;
                break;
            }
            if (cmp == 0)
            {
                itr->replace(e);
                delete e;       /*Because this, e will be invalid */
                break;
            }
            prev = itr;
            itr = itr->next;
        }
        if (!itr)
            prev->next = e;
    }
    else
        bucket[idx] = e;
}
