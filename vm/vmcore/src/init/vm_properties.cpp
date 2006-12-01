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

#include <apr_file_io.h>
#include <apr_file_info.h>
#include <apr_env.h>
#include <apr_strings.h>
#include "port_dso.h"
#include "port_filepath.h"
#include "port_sysinfo.h"

#define LOG_DOMAIN "init.properties"
#include "cxxlog.h"
#include "properties.h"
#include "vm_properties.h"
#include "init.h"

inline char* unquote(char *str)
{
    const char *tokens = " \t\n\r\'\"";
    size_t i = strspn(str, tokens);
    str += i;
    char *p = str + strlen(str) - 1;
    while(strchr(tokens, *p) && p >= str)
        *(p--) = '\0';
    return str;
}

#define PROP_ENV_NAME  "VM_PROPERTIES"
#define PROP_FILE_NAME "vm.properties"

#define MAX_PROP_LINE 5120

// local memory pool for temporary allocation
static apr_pool_t *prop_pool;

static void
read_properties(char *fname, Properties & prop)
{
    apr_file_t *f;
    char *src, *tok;
    if (APR_SUCCESS != apr_file_open(&f, fname, APR_FOPEN_READ, 0, prop_pool))
        return;
    WARN("USAGE OF vm.properties FILE IS DEPRECATED. THIS FUNCTIONALITY WILL BE REMOVED SOON. ALL PROPERTIES SHOULD BE SET ON COMMAND LINE");
    char buf[MAX_PROP_LINE];
    while (!apr_file_eof(f) && !apr_file_gets(buf, MAX_PROP_LINE, f))
    {
        if (buf[0] != '#') {
            src = strdup(buf);
            tok = strchr(src, '=');
            if(tok)
            {
                *tok = '\0';
                prop.set(unquote(src), unquote(tok + 1));
            }
            STD_FREE(src);
        }
    }
    apr_file_close(f);
}

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

#define GC_DLL "gc_cc"

/**
 *  Compose a string of file names each of them beginning with path,
 *  names separated by PORT_PATH_SEPARATOR.  If patch is NULL, no path
 *  or separator will be prefixed
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

static void init_java_properties(Properties & properties)
{
    //java part
    //!!! java.compiler property must be defined by EM

    char *os_name, *os_version, *path;
    const char *tmp;
    char *path_buf = NULL;

    port_OS_name_version(&os_name, &os_version, prop_pool);
    apr_filepath_get(&path, APR_FILEPATH_NATIVE, prop_pool);
    if (APR_SUCCESS != apr_temp_dir_get(&tmp, prop_pool)) {
        tmp = ".";
    }
    properties.set("java.version", "1.5.0");
    properties.set("java.vendor", "Apache Software Foundation");
    properties.set("java.vendor.url", "http://harmony.apache.org");
    // java.home initialization, try to find absolute location of the executable and set
    // java.home to the parent directory.
    char *base_path_buf;
    if (port_executable_name(&base_path_buf, prop_pool) != APR_SUCCESS) {
        DIE("Failed to find executable location");
    }
    // directory for the executable
    char *p = strrchr(base_path_buf, PORT_FILE_SEPARATOR);
    if (NULL == p)
        DIE("Failed to determine executable parent directory");
    *p = '\0';
    // home directory
    char* home_path = apr_pstrdup(prop_pool, base_path_buf);
    p = strrchr(home_path, PORT_FILE_SEPARATOR);
    if (NULL == p)
        DIE("Failed to determine java home directory");
    *p = '\0';

    properties.set("java.home", home_path);
    properties.set("java.vm.specification.version", "1.0");
    properties.set("java.vm.specification.vendor", "Sun Microsystems Inc.");
    properties.set("java.vm.specification.name", "Java Virtual Machine Specification");
    properties.set("java.vm.version", "11.2.0");
    properties.set("java.vm.vendor", "Apache Software Foundation");
    properties.set("java.vm.name", "DRLVM");
    properties.set("java.specification.version", "1.5");
    properties.set("java.specification.vendor", "Sun Microsystems Inc.");
    properties.set("java.specification.name", "Java Platform API Specification");
    properties.set("java.class.version", "49.0");
    properties.set("java.class.path", ".");

    // java.library.path initialization, the value is the location of VM executable,
    // prepended to OS library search path
    char *env;
    char *lib_path = base_path_buf;
    if (APR_SUCCESS == port_dso_search_path(&env, prop_pool))
    {
        lib_path = apr_pstrcat(prop_pool, base_path_buf, PORT_PATH_SEPARATOR_STR,
                               base_path_buf, PORT_FILE_SEPARATOR_STR, "default",
                               PORT_PATH_SEPARATOR_STR, env, NULL);
    }
    properties.set("java.library.path", lib_path);
    //java.ext.dirs initialization.
    char *ext_path = port_filepath_merge(home_path, "lib" PORT_FILE_SEPARATOR_STR "ext", prop_pool);
    properties.set("java.ext.dirs", ext_path);
    properties.set("os.name", os_name);
    properties.set("os.arch", port_CPU_architecture());
    properties.set("os.version", os_version);
    properties.set("file.separator", PORT_FILE_SEPARATOR_STR);
    properties.set("path.separator", PORT_PATH_SEPARATOR_STR);
    properties.set("line.separator", APR_EOL_STR);
    // user.name initialization, try to get the name from the system
    char *user_buf;
    apr_status_t status = port_user_name(&user_buf, prop_pool);
    if (APR_SUCCESS != status) {
        DIE("Failed to get user name from the system. Error code " << status);
    }
    properties.set("user.name", user_buf);
    // user.home initialization, try to get home from the system.
    char *user_home;
    status = port_user_home(&user_home, prop_pool);
    if (APR_SUCCESS != status) {
        DIE("Failed to get user home from the system. Error code " << status);
    }
    properties.set("user.home", user_home);
    // java.io.tmpdir initialization. 
    const char *tmpdir;
    status = apr_temp_dir_get(&tmpdir, prop_pool);
    if (APR_SUCCESS != status) {
        tmpdir = user_home;
    }
    properties.set("java.io.tmpdir", tmpdir);
    properties.set("user.dir", path);

    // FIXME??? other (not required by api specification) properties
    
    properties.set("java.vm.info", "no info");
    properties.set("java.tmpdir", tmp);
    properties.set("user.language", "en");
    properties.set("user.region", "US");
    properties.set("file.encoding", "8859_1");

    // FIXME user.timezone initialization, required by java.util.TimeZone implementation
    char *user_tz;
    status = port_user_timezone(&user_tz, prop_pool);
    if (APR_SUCCESS != status) {
        INFO("Failed to get user timezone from the system. Error code " << status);
        user_tz = "GMT";
    }
    
    properties.set("user.timezone", user_tz);

    // FIXME: This is a workaround code for third party APIs which depend on this property.
    properties.set("java.util.prefs.PreferencesFactory",
#ifdef PLATFORM_NT
        "java.util.prefs.RegistryPreferencesFactoryImpl");
#else
        "java.util.prefs.FilePreferencesFactoryImpl");
#endif

    // Added for compatibility with the external java JDWP agent
    properties.set("sun.boot.library.path", base_path_buf);

    /*
    *  it's possible someone forgot to set this property - set to default of .
    */
    if (!properties.is_set(O_A_H_VM_VMDIR)) {
        TRACE2("init", "o.a.h.vm.vmdir not set - setting predefined value of as '.'");
        properties.set(O_A_H_VM_VMDIR, ".");
    }

    /*
    *  also, do the same for java.class.path
    */
    if (!properties.is_set("java.class.path")) {
        TRACE2("init", "java.class.path not set - setting predefined value of as '.'");
        properties.set("java.class.path", ".");
    }
}

//vm part
static void init_vm_properties(Properties & properties)
{
        properties.set("vm.assert_dialog", "true");
        properties.set("vm.crash_handler", "false");
        properties.set("vm.finalize", "true");
        properties.set("vm.jit_may_inline_sync", "true");
        properties.set("vm.use_interpreter", "false");
        properties.set("vm.use_verifier", "true");
        properties.set("vm.jvmti.enabled", "false");
        properties.set("vm.cleanupOnExit", "false");
        properties.set("vm.bootclasspath.appendclasspath", "false");

        /*
        *  vm.boot.library.path initialization, the value is the location of VM executable
        * 
        *  2006-09-06 gmj :  there's no reason to ever believe this is true given how the VM can be 
        *  launched in a mariad of ways, so just set to empty string.
        */
        properties.set("vm.boot.library.path", "");

        properties.set("vm.dlls", PORT_DSO_NAME(GC_DLL));

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
        char* path_buf = compose_full_files_path_names_list(NULL, api_dll_files, n_api_dll_files, true);
        properties.set("vm.other_natives_dlls", path_buf);
}

void
initialize_properties(Global_Env * p_env)
{
    if (!prop_pool) {
        apr_pool_create(&prop_pool, 0);
    }
/*
 * 0. Add predefined properties from property table
 */
     
    init_java_properties(*p_env->JavaProperties());
    init_vm_properties(*p_env->VmProperties());

/* 
 * 1. VM looks for an environment variable, say, 
 *    VM_PROPERTIES=d:\xyz\eee\vm.Properties, read the Properties;
 */
    char *pf;
    if (apr_env_get(&pf, PROP_ENV_NAME, prop_pool) == APR_SUCCESS){
        read_properties(pf, *p_env->JavaProperties());
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
            read_properties(buf, *p_env->JavaProperties());
        }
    }

/*
 * 3. Looks for it in current directory(it's an app-specific file), read the 
 *    Properties, if key is duplicated, override the value;
 */
    apr_filepath_get(&buf, 0, prop_pool);
    buf = port_filepath_merge(buf, PROP_FILE_NAME, prop_pool);
    read_properties(buf, *p_env->JavaProperties());
/*
 * 4. Check whether there is a command line option, say, 
 *    -Properties-file "d:\xyz\eee\vm.Properties" or -Droperties key=value, 
 *    read the Properties, if key is duplicated, override the value. 
 */
    char *src, *tok;
    for (int arg_num = 0; arg_num < p_env->vm_arguments.nOptions; arg_num++)
    {
        char *option = p_env->vm_arguments.options[arg_num].optionString;
        if (strncmp(option, "-D", 2) == 0)
        {
            TRACE("setting property " << option + 2);
            src = strdup(option + 2);
            tok = strchr(src, '=');
            if(tok)
            {
                *tok = '\0';
                p_env->JavaProperties()->set(unquote(src), unquote(tok + 1));
            }
            STD_FREE(src);
        } 
        else if (strncmp(option, "-XD", 3) == 0)
        {
            TRACE("setting internal property " << option + 3);
            src = strdup(option + 3);
            tok = strchr(src, '=');
            if(tok)
            {
                *tok = '\0';
                p_env->VmProperties()->set(unquote(src), unquote(tok + 1));
            }
            STD_FREE(src);
        }
    }
    apr_pool_clear(prop_pool);
}
