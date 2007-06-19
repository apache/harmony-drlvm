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
 * @version $Revision: 1.1.2.4.4.7 $
 */  

#include <string.h>
#include <assert.h>
#include <ctype.h>
#include <apr_strings.h>
#include <apr_env.h>

#include "open/gc.h" 
#include "open/vm_util.h"

#include "Class.h"
#include "properties.h"
#include "environment.h"
#include "assertion_registry.h"
#include "port_filepath.h"
#include "compile.h"
#include "vm_stats.h"
#include "nogc.h"
#include "version.h"

#include "classpath_const.h"

// Multiple-JIT support.
#include "jit_intf.h"
#include "dll_jit_intf.h"
#include "dll_gc.h"

#define LOG_DOMAIN "vm.core"
#include "cxxlog.h"

//------ Begin DYNOPT support --------------------------------------
//
#ifndef PLATFORM_POSIX       // for DYNOPT in Win64

extern JIT      *dynopt_jitc;
extern bool     dynamic_optimization;
extern bool     software_profile;
extern bool        data_ear;
extern unsigned long    data_ear_interval;
extern int              data_ear_filter;
extern bool        branch_trace;
extern unsigned long    btb_interval;

#endif // !PLATFORM_POSIX
//
//------ End DYNOPT support ----------------------------------------

#define EXECUTABLE_NAME "java"
#define USE_JAVA_HELP LECHO(29, "Use {0} -help to get help on command line options" << EXECUTABLE_NAME)
#define LECHO_VERSION LECHO(32, VERSION << VERSION_SVN_TAG << __DATE__ << VERSION_OS \
        << VERSION_ARCH << VERSION_COMPILER << VERSION_DEBUG_STRING)
#define LECHO_VM_VERSION LECHO(33, VM_VERSION)
extern bool dump_stubs;
extern bool parallel_jit;
extern const char * dump_file_name;

/**
 * Check if a string begins with another string. Note, even gcc
 * substitutes actual string length instead of strlen
 * for constant strings.
 * @param str
 * @param beginning
 */
static inline bool begins_with(const char* str, const char* beginning)
{
    return strncmp(str, beginning, strlen(beginning)) == 0;
}

void print_generic_help()
{
    LECHO(21, 
        "Usage: {0} [-options] class [args...]\n"
        "        (to execute a method main() of the class)\n"
        "    or {0} [-options] -jar jarfile [args...]\n"
        "        (to execute the jar file)\n"
        "\n"
        "where options include:\n"
        "    -classpath <class search path of directories and zip/jar files>\n"
        "    -cp        <class search path of directories and zip/jar files>\n"
        "                  A {1} separated list of directories, jar archives,\n"
        "                  and zip archives to search for class file\n"
        "    -client       select the 'client' VM (same as -Xem:client)\n"
        "    -server       select the 'server' VM (same as -Xem:server)\n"
        "    -D<name>=<value>\n"
        "                  set a system property\n"
        "    -showversion  print product version and continue\n"
        "    -version      print product version and exit\n"
        "    -verbose[:class|:gc|:jni]\n"
        "                  enable verbose output\n"
        "    -agentlib:<library name>[=<agent options>]\n"
        "                  load JVMTI agent library, library name is platform independent\n"
        "    -agentpath:<library name>[=<agent options]\n"
        "                  load JVMTI agent library, library name is platform dependent\n"
        "    -verify\n"
        "                  do full bytecode verification\n"
        "    -noverify\n"
        "                  do no bytecode verification\n"
        "    -enableassertions[:<package>...|:<class>]\n"
        "    -ea[:<package>...|:<class>]\n"
        "                  enable assertions\n"
        "    -disableassertions[:<package>...|:<class>]\n"
        "    -da[:<package>...|:<class>]\n"
        "                  disable assertions\n"
        "    -esa | -enablesystemassertions\n"
        "                  enable system assertions\n"
        "    -dsa | -disablesystemassertions\n"
        "                  disable system assertions\n"
        "    -? -help      print this help message\n"
        "    -help properties\n"
        "                  help on system properties\n"
        "    -X            print help on non-standard options" 
        <<  EXECUTABLE_NAME << PORT_PATH_SEPARATOR_STR);
}

static void print_help_on_nonstandard_options()
{
    LECHO(22, 
        "    -Xbootclasspath:<PATH>\n"
        "              Set bootclasspath to the specified value\n"
        "    -Xbootclasspath/a:<PATH>\n"
        "              Append specified directories and files to bootclasspath\n"
        "    -Xbootclasspath/p:<PATH>\n"
        "              Prepend specified directories and files to bootclasspath\n"
        "    -Xjit <JIT options>\n"
        "              Specify JIT specific options\n"
        "    -Xms<size>\n"
        "              Set Java heap size\n"
        "    -Xmx<size>\n"
        "              Set maximum Java heap size\n"
        "    -Xdebug\n"
        "              Does nothing, this is a compatibility option\n"
        "    -Xnoagent\n"
        "              Does nothing, this is a compatibility option\n"
        "    -Xrun\n"
        "              Specify debugger agent library\n"
        "    -Xverbose[:<category>[:<file>]\n"
        "              Switch logging on [for specified category only\n"
        "              [and log that category to a file]]\n"
        "    -Xwarn[:<category>[:<file>]\n"
        "              Switch verbose logging off [for specified category only\n"
        "              [and log that category to a file]]\n"
        "    -Xverboseconf:<file>\n"
        "              Set up logging via log4cxx configuration file\n"
        "    -Xverboselog:<file>\n"
        "              Log verbose output to a file\n"
        "    -Xverify[:none|all]\n"
        "              Do full bytecode verification\n"
        "    -Xinvisible\n"
        "              Retain invisible annotations at runtime\n"
        "    -Xfileline\n"
        "              Add source information to logging messages\n"
        "    -Xthread\n"
        "              Add thread id to logging messages\n"
        "    -Xcategory\n"
        "              Add category name to logging messages\n"
        "    -Xtimestamp\n"
        "              Add timestamp to logging messages\n"
        "    -Xfunction\n"
        "              Add function signature to logging messages");
#ifdef _DEBUG
    LECHO(23, 
        "    -Xlog[:<category>[:<file>]\n"
        "              Switch debug logging on [for specified category only\n"
        "              [and log that category to a file]]\n"
        "    -Xtrace[:<category>[:<file>]\n"
        "              Switch trace logging on [for specified category only\n"
        "              [and log that category to a file]]");
#endif //_DEBUG

#ifdef VM_STATS
    LECHO(24, 
        "    -Xstats:<mask>\n"
        "              Generates different statistics");
#endif // VM_STATS
    LECHO(25, 
        "    -Xint\n"
        "              Use interpreter to execute the program\n"
        "    -Xgc:<gc options>\n"
        "              Specify gc specific options\n"
        "    -Xem:<em options>\n"
        "              Specify em specific options\n"
        "    -Xdumpstubs\n"
        "              Writes stubs generated by LIL to disk\n"
        "    -Xparallel_jit\n"
        "              Launch compilation in parallel (default)\n"
        "    -Xno_parallel_jit\n"
        "              Do not launch compilation in parallel\n"
        "    -Xdumpfile:<file>\n"
        "              Specifies a file name for the dump\n"
        "    -XX:<name>=<value>\n"
        "              set an internal system property.\n"
        "              Boolean options may be turned on with -XX:+<option>\n"
        "              and turned off with -XX:-<option>\n"
        "              Numeric options are set with -XX:<option>=<number>.\n"
        "              Numbers can include suffix 'm' or 'M' for megabytes,\n"
        "              'k' or 'K' for kilobytes, and 'g' or 'G' for gigabytes\n" 
        "              (for example, 32k is the same as 32768).\n"
    );
} //print_help_on_nonstandard_options

void print_vm_standard_properties()
{
    LECHO(26, 
        "Boolean-valued properties (set to one of {on,true,1,off,false,0} through -XD<name>=<value>):\n\n"
        "    vm.assert_dialog (default TRUE):\n"
        "            If false, prevent assertion failures from popping up a dialog box.");
#ifdef PLATFORM_POSIX
    LECHO(27, "    vm.crash_handler (default FALSE):\n"
        "            Invoke gdb on crashes.");
#endif // PLATFORM_POSIX
    LECHO(28, "    vm.finalize (default TRUE):\n"
        "            Run finalizers.\n"
        "    vm.jit_may_inline_sync (default TRUE):\n"
        "            The JIT is allowed to inline part of the synchronization sequence.\n"
        "    vm.use_interpreter (default FALSE):\n"
        "            Use interpreter not jit.\n"
        "    vm.use_verifier (default TRUE):\n"
        "            Use verifier.\n"
        "    vm.jvmti.enabled (default FALSE):\n"
        "            Whether JVMTI mode is enabled.\n"
        "    vm.jvmti.compiled_method_load.inlined (default FALSE):\n"
        "            Report inlined methods with JVMTI_EVENT_COMPILED_METHOD_LOAD. Makes sense for optimizing jit.\n"
        "    vm.bootclasspath.appendclasspath (default FALSE):\n"
        "            Append classpath to the bootclasspath.\n"
	"    thread.soft_unreservation (default FALSE):\n"
        "            Use soft unreservation scheme.\n"
        "\nOther properties:\n\n"
        "    vm.boot.library.path:\n"
        "            List of directories which contain additional dynamic libraries to load into VM.\n"
        "    vm.boot.class.path:\n"
        "            Virtual machine bootclasspath.\n"
        "    vm.dlls:\n"
        "            A '{0}'-delimited list of modular dlls (GC/etc.) to load at startup.\n"
        "    vm.em_dll:\n"
        "            A '{0}'-execution manager (EM) dll to load at startup.\n"
        "    vm.other_natives_dlls:\n"
        "            A '{0}'-delimited list of dlls contained native methods implementations to load at startup.\n"
        "    vm.components.<component>.classpath:\n" 
        "            Part of a <component>'s classpath to append to the JDK bootclasspath\n"
        "    vm.components.<component>.startupclass:\n"
        "            A <component> class to be initialized during startup" << PORT_PATH_SEPARATOR_STR);

}

static inline Assertion_Registry* get_assert_reg(Global_Env *p_env) {
    if (!p_env->assert_reg) {
        void * mem = apr_palloc(p_env->mem_pool, sizeof(Assertion_Registry));
        p_env->assert_reg = new (mem) Assertion_Registry(); 
    }
    return p_env->assert_reg;
}

static void add_assert_rec(Global_Env *p_env, const char* option, const char* cmdname, bool value) {
    const char* arg = option + strlen(cmdname);
    if ('\0' == arg[0]) {
        get_assert_reg(p_env)->enable_all = value ? ASRT_ENABLED : ASRT_DISABLED;
    } else if (':' != arg[0]) {
        LECHO(30, "Unknown option {0}" << option);
        USE_JAVA_HELP;
        LOGGER_EXIT(1);
    } else {
        unsigned len = (unsigned)strlen(++arg);
        if (len >= 3 && strncmp("...", arg + len - 3, 3) == 0) {
            get_assert_reg(p_env)->add_package(p_env, arg, len - 3, value);
        } else {
            get_assert_reg(p_env)->add_class(p_env, arg, len, value);
        }
    }
}

void* get_portlib_for_logger(Global_Env *p_env) {
    for (int i = 0; i < p_env->vm_arguments.nOptions; i++) {
        const char* option = p_env->vm_arguments.options[i].optionString;
        if (strcmp(option, "_org.apache.harmony.vmi.portlib") == 0) {
            return p_env->vm_arguments.options[i].extraInfo;
        }
    }
    return NULL;
}

void parse_vm_arguments(Global_Env *p_env)
{
    bool version_printed = false;
#ifdef _DEBUG
    TRACE2("arguments", "p_env->vm_arguments.nOptions  = " << p_env->vm_arguments.nOptions);
    for (int _i = 0; _i < p_env->vm_arguments.nOptions; _i++)
        TRACE2("arguments", "p_env->vm_arguments.options[ " << _i << "] = " << p_env->vm_arguments.options[_i].optionString);
#endif //_DEBUG

    apr_pool_t *pool;
    apr_pool_create(&pool, 0);

    for (int i = 0; i < p_env->vm_arguments.nOptions; i++) {
        const char* option = p_env->vm_arguments.options[i].optionString;

        if (begins_with(option, XBOOTCLASSPATH)) {
            /*
             *  Override for bootclasspath - 
             *  set in the environment- the boot classloader will be responsible for 
             *  processing and setting up "vm.boot.class.path" and "sun.boot.class.path"
             *  Note that in the case of multiple arguments, the last one will be used
             */
            p_env->VmProperties()->set(XBOOTCLASSPATH, option + strlen(XBOOTCLASSPATH));
        }
        else if (begins_with(option, XBOOTCLASSPATH_A)) {
            /*
             *  addition to append to boot classpath
             *  set in environment - responsibility of boot classloader to process
             *  Note that we accumulate if multiple, appending each time
             */

            char *bcp_old = p_env->VmProperties()->get(XBOOTCLASSPATH_A);
            const char *value = option + strlen(XBOOTCLASSPATH_A);
            char *bcp_new = NULL;
            
            if (bcp_old) { 
                 char *tmp = (char *) STD_MALLOC(strlen(bcp_old) + strlen(PORT_PATH_SEPARATOR_STR)
                                                        + strlen(value) + 1);
            
                 strcpy(tmp, bcp_old);
                 strcat(tmp, PORT_PATH_SEPARATOR_STR);
                 strcat(tmp, value);
                 
                 bcp_new = tmp;
            }
            
            p_env->VmProperties()->set(XBOOTCLASSPATH_A, bcp_old ? bcp_new : value);                       
            p_env->VmProperties()->destroy(bcp_old);
            STD_FREE((void*)bcp_new);
        }
        else if (begins_with(option, XBOOTCLASSPATH_P)) {
            /*
             *  addition to prepend to boot classpath
             *  set in environment - responsibility of boot classloader to process
             *  Note that we accumulate if multiple, prepending each time
             */
             
            char *bcp_old = p_env->VmProperties()->get(XBOOTCLASSPATH_P);
            const char *value = option + strlen(XBOOTCLASSPATH_P);
            
            char *bcp_new = NULL;
            
            if (bcp_old) { 
                 char *tmp = (char *) STD_MALLOC(strlen(bcp_old) + strlen(PORT_PATH_SEPARATOR_STR)
                                                        + strlen(value) + 1);
            
                 strcpy(tmp, value);
                 strcat(tmp, PORT_PATH_SEPARATOR_STR);
                 strcat(tmp, bcp_old);
                 
                 bcp_new = tmp;
            }
            
            p_env->VmProperties()->set(XBOOTCLASSPATH_P, bcp_old ? bcp_new : value);
            p_env->VmProperties()->destroy(bcp_old);
            STD_FREE((void*)bcp_new);
        } 
        else if (begins_with(option, "-Xhelp:")) {
            const char* arg = option + strlen("-Xhelp:");

            if (begins_with(arg, "prop")) {
                print_vm_standard_properties();

            } else {
                LECHO(31, "Unknown argument {0} of -Xhelp: option" << arg);
            }

            LOGGER_EXIT(0);
        } else if (begins_with(option, "-Xjit:")) {
            // Do nothing here, just skip this option for later parsing
        } else if (strcmp(option, "-Xint") == 0) {
            p_env->VmProperties()->set("vm.use_interpreter", "true");
#ifdef VM_STATS
        } else if (begins_with(option, "-Xstats:")) {
            vm_print_total_stats = true;
            const char* arg = option + strlen("-Xstats:");
            vm_print_total_stats_level = atoi(arg);
#endif
        } else if (strcmp(option, "-version") == 0) {
            // Print the version number and exit
            LECHO_VERSION;
            LOGGER_EXIT(0);
        } else if (strcmp(option, "-showversion") == 0) {
            if (!version_printed) {
                // Print the version number and continue
                LECHO_VERSION;
                version_printed = true;
            }
        } else if (strcmp(option, "-fullversion") == 0) {
            // Print the version number and exit
            LECHO_VM_VERSION;
            LOGGER_EXIT(0);

        } else if (begins_with(option, "-Xgc:")) {
            // make prop_key to be "gc.<something>"
            char* prop_key = strdup(option + strlen("-X"));
            prop_key[2] = '.';
            TRACE2("init", prop_key << " = 1");
            p_env->VmProperties()->set(prop_key, "1");
            free(prop_key);

        } else if (begins_with(option, "-Xem:")) {
            const char* arg = option + strlen("-Xem:");
            p_env->VmProperties()->set("em.properties", arg);

        } else if (strcmp(option, "-client") == 0 || strcmp(option, "-server") == 0) {
            p_env->VmProperties()->set("em.properties", option + 1);

        } else if (begins_with(option, "-Xms")) {
            // cut -Xms
            const char* arg = option + 4;
            TRACE2("init", "gc.ms = " << arg);
            if (atoi(arg) == 0) {
                LECHO(34, "Negative or invalid heap size. Default value will be used!");
            }
            p_env->VmProperties()->set("gc.ms", arg);

        } else if (begins_with(option, "-Xmx")) {
            // cut -Xmx
            const char* arg = option + 4;
            TRACE2("init", "gc.mx = " << arg);
            if (atoi(arg) == 0) {
                LECHO(34, "Negative or invalid heap size. Default value will be used!");
            }
            p_env->VmProperties()->set("gc.mx", arg);
        } else if (begins_with(option, "-Xss")) {
            const char* arg = option + 4;
            TRACE2("init", "thread.stacksize = " << arg);
            if (atoi(arg) == 0) {
                LECHO(34, "Negative or invalid stack size. Default value will be used!");
            }
            p_env->VmProperties()->set("thread.stacksize", arg);
	}	  
        else if (begins_with(option, "-agentlib:")) {
            p_env->TI->addAgent(option);
        }
        else if (begins_with(option, "-agentpath:")) {
            p_env->TI->addAgent(option);
        }
        else if (begins_with(option, "-Xrun")) {
            // Compatibility with JNDI
            p_env->TI->addAgent(option);
        }
        else if (strcmp(option, "-Xnoagent") == 0) {
            // Do nothing, this option is only for compatibility with old JREs
        }
        else if (strcmp(option, "-Xdebug") == 0) {
            // Do nothing, this option is only for compatibility with old JREs
        }
        else if (strcmp(option, "-Xfuture") == 0) {
            // Do nothing, this option is only for compatibility with old JREs
        }
        else if (strcmp(option, "-Xinvisible") == 0) {
            p_env->retain_invisible_annotations = true;
        }
        else if (strcmp(option, "-Xverify") == 0) {
            p_env->verify_all = true;
        }
        else if (strcmp(option, "-Xverify:none") == 0 || strcmp(option, "-noverify") == 0) {
            p_env->VmProperties()->set("vm.use_verifier", "false");
        }
        else if (strcmp(option, "-Xverify:all") == 0) {
            p_env->verify_all = true;
            p_env->verify_strict = true;
        }
        else if (strcmp(option, "-Xverify:strict") == 0) {
            p_env->verify_all = true;
            p_env->verify_strict = true;
        }
        else if (strcmp(option, "-verify") == 0) {
            p_env->verify_all = true;
        }
        else if (begins_with(option, "-verbose")) {
            // Moved to set_log_levels_from_cmd
        } else if (begins_with(option, "-Xfileline")) {
            // Moved to set_log_levels_from_cmd
        } else if (begins_with(option, "-Xthread")) {
            // Moved to set_log_levels_from_cmd
        } else if (begins_with(option, "-Xcategory")) {
            // Moved to set_log_levels_from_cmd
        } else if (begins_with(option, "-Xtimestamp")) {
            // Moved to set_log_levels_from_cmd
        } else if (begins_with(option, "-Xverbose")) {
            // Moved to set_log_levels_from_cmd
        } else if (begins_with(option, "-Xwarn")) {
            // Moved to set_log_levels_from_cmd
        } else if (begins_with(option, "-Xfunction")) {
            // Moved to set_log_levels_from_cmd
#ifdef _DEBUG
        } else if (begins_with(option, "-Xlog")) {
            // Moved to set_log_levels_from_cmd
        } else if (begins_with(option, "-Xtrace")) {
            // Moved to set_log_levels_from_cmd
#endif //_DEBUG
        }
        else if (strncmp(option, "-D", 2) == 0) {
        }
        else if (strncmp(option, "-XD", 3) == 0 || strncmp(option, "-XX:", 4) == 0) {
        }
        else if (strcmp(option, "-Xdumpstubs") == 0) {
            dump_stubs = true;
        }
        else if (strcmp(option, "-Xparallel_jit") == 0) {
            parallel_jit = true;
        }
        else if (strcmp(option, "-Xno_parallel_jit") == 0) {
            parallel_jit = false;
        }
        else if (begins_with(option, "-Xdumpfile:")) {
            const char* arg = option + strlen("-Xdumpfile:");
            dump_file_name = arg;
        }
        else if (strcmp(option, "_org.apache.harmony.vmi.portlib") == 0) {
            // Store a pointer to the portlib
            p_env->portLib = p_env->vm_arguments.options[i].extraInfo;
        }
        else if (strcmp(option, "-help") == 0 
              || strcmp(option, "-h") == 0
              || strcmp(option, "-?") == 0) {
            print_generic_help();
            LOGGER_EXIT(0);
        }
        else if (strcmp(option,"-X") == 0) {
                print_help_on_nonstandard_options();
                LOGGER_EXIT(0);
        }
        else if (begins_with(option, "-enableassertions")) {
            add_assert_rec(p_env, option, "-enableassertions", true);
        }
        else if (begins_with(option, "-ea")) { 
            add_assert_rec(p_env, option, "-ea", true);
        }
        else if (begins_with(option, "-disableassertions")) { 
            add_assert_rec(p_env, option, "-disableassertions", false);
        }
        else if (begins_with(option, "-da")) { 
            add_assert_rec(p_env, option, "-da", false);
        }
        else if (strcmp(option, "-esa") == 0 
            || strcmp(option, "-enablesystemassertions") == 0) {
            get_assert_reg(p_env)->enable_system = true;
        }
        else if (strcmp(option, "-dsa") == 0 
            || strcmp(option, "-disablesystemassertions") == 0) {
                if (p_env->assert_reg) {
                    p_env->assert_reg->enable_system = false;
                }
        }

        else {
            LECHO(30, "Unknown option {0}" << option);
            USE_JAVA_HELP;
            LOGGER_EXIT(1);
       }
    } // for

    apr_pool_destroy(pool);
} //parse_vm_arguments

void parse_jit_arguments(JavaVMInitArgs* vm_arguments)
{
    const char* prefix = "-Xjit:";
    for (int arg_num = 0; arg_num < vm_arguments->nOptions; arg_num++)
        {
        char *option = vm_arguments->options[arg_num].optionString;
        if (begins_with(option, prefix))
        {
            // split option on 2 parts
            char *arg = option + strlen(prefix);
            JIT **jit;
            for(jit = jit_compilers; *jit; jit++)
                (*jit)->next_command_line_argument("-Xjit", arg);
        }
    }
} //parse_jit_arguments

// converts exposed verbosity category names 
// to the internally used logger category names.
static char* convert_logging_category(char* category) {
    if (0 == strcmp("gc", category)) {
        // hijack the standard category "gc" (-verbose:gc) to the
        // more specific internal category "gc.verbose"
        return "gc.verbose";
    } else if (0 == strcmp("gc*", category) || 0 == strcmp("gc.*", category)) {
        // handle the non-standard logging category specification
        return "gc";
    } else {
        return category;
    }
} //convert_logging_category()

static void set_threshold_list(char* list, LoggingLevel level, const char* file, bool convert) {
    char *next;
    while (list) {
        if ( (next = strchr(list, ',')) ) {
            *next = '\0';
        }

        char* category = (convert) ? convert_logging_category(list) : list;
        if (0 == strcmp("*", category)) { //alias to root category
            category = "root";
        }
        set_threshold(category, level);
        if (file) {
            set_out(category, file);
        }

        if (next) {
            *next = ',';
            list = next + 1;
        } else {
            break;
        }
    }
}

static void parse_logger_arg(char* arg, const char* cmd, LoggingLevel level) {
    char* next_sym = arg + strlen(cmd);
    if (*next_sym == '\0') {
        set_threshold("root", level);
    } else if (*next_sym == ':') { // -cmd:category
        next_sym++;
        char *out = strchr(next_sym, ':');
        if (out) { // -cmd:category:file
            *out = '\0';
        }
        set_threshold_list(next_sym, level, out ? out + 1 : NULL, false);
        if (out){
            *out = ':';
        }
    } else {
        LECHO(30, "Unknown option {0}" << arg);
        USE_JAVA_HELP;
        LOGGER_EXIT(1); 
    }
}

void set_log_levels_from_cmd(JavaVMInitArgs* vm_arguments)
{
    HeaderFormat logger_header = HEADER_EMPTY;
    int arg_num = 0;
    for (arg_num = 0; arg_num < vm_arguments->nOptions; arg_num++) {
        char *option = vm_arguments->options[arg_num].optionString;

        if (begins_with(option, "-Xfileline")) {
            logger_header |= HEADER_FILELINE;
        } else if (begins_with(option, "-Xthread")) {
            logger_header |= HEADER_THREAD_ID;
        } else if (begins_with(option, "-Xcategory")) {
            logger_header |= HEADER_CATEGORY;
        } else if (begins_with(option, "-Xtimestamp")) {
            logger_header |= HEADER_TIMESTAMP;
        } else if (begins_with(option, "-Xfunction")) {
            logger_header |= HEADER_FUNCTION;
        }
    } 
    // set logging filter if one is set
    if (logger_header != HEADER_EMPTY) {
        set_header_format("root", logger_header);
    }

    for (arg_num = 0; arg_num < vm_arguments->nOptions; arg_num++) {
        char *option = vm_arguments->options[arg_num].optionString;

        if (begins_with(option, "-verbose")) {
            /*
            * -verbose[:class|:gc|:jni]
            * Set specification log filters.
            */
            char* next_sym = option + 8;
            if (*next_sym == '\0') {
                set_threshold(util::CLASS_LOGGER, INFO);
                set_threshold(util::GC_LOGGER, INFO);
                set_threshold(util::JNI_LOGGER, INFO);
            } else if (*next_sym == ':') { // -verbose:
                next_sym++;
                set_threshold_list(next_sym, INFO, NULL, true); // true = convert standard categories to internal
            } else {
                LECHO(30, "Unknown option {0}" << option);
                USE_JAVA_HELP;
                LOGGER_EXIT(1);
            }
        } else if (begins_with(option, "-Xverboseconf:")) {
            set_logging_level_from_file(option + strlen("-Xverboseconf:"));
        } else if (begins_with(option, "-Xverboselog:")) {
            set_out("root", option + strlen("-Xverboselog:"));
        } else if (begins_with(option, "-Xverbose")) {
            parse_logger_arg(option, "-Xverbose", INFO);
        } else if (begins_with(option, "-Xwarn")) {
            parse_logger_arg(option, "-Xwarn", WARN);
#ifdef _DEBUG
        } else if (begins_with(option, "-Xlog")) {
            parse_logger_arg(option, "-Xlog", LOG);
        } else if (begins_with(option, "-Xtrace")) {
            parse_logger_arg(option, "-Xtrace", TRACE);
#endif //_DEBUG
        }
    } // for (arg_num)
} //set_log_levels_from_cmd

void initialize_vm_cmd_state(Global_Env *p_env, JavaVMInitArgs* arguments)
{
    p_env->vm_arguments.version = arguments->version;
    p_env->vm_arguments.nOptions = arguments->nOptions;
    p_env->vm_arguments.ignoreUnrecognized = arguments->ignoreUnrecognized;
    JavaVMOption *options = p_env->vm_arguments.options =
        (JavaVMOption*)STD_MALLOC(sizeof(JavaVMOption) * (arguments->nOptions));
    assert(options);

    memcpy(options, arguments->options, sizeof(JavaVMOption) * (arguments->nOptions));
} //initialize_vm_cmd_state
