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
 * @author Intel, Alexey V. Varlamov, Gregory Shimansky
 * @version $Revision: 1.1.2.4.4.7 $
 */  


#define LOG_DOMAIN "vm.core"
#include "cxxlog.h"

#include <apr_strings.h>
#include <apr_env.h>
#include "port_filepath.h"

#include <string.h>
#include <assert.h>
#include <ctype.h>


#include "Class.h"
#include "properties.h"
#include "environment.h"

#include "open/gc.h"
 
#include "open/vm_util.h"
#include "compile.h"
#include "vm_stats.h"
#include "nogc.h"
#include "version.h"

// Multiple-JIT support.
#include "jit_intf.h"
#include "dll_jit_intf.h"
#include "dll_gc.h"

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
#define USE_JAVA_HELP "\nUse " EXECUTABLE_NAME " -help to get help on" \
    " command line options"


extern bool dump_stubs;
extern bool parallel_jit;
extern const char * dump_file_name;

static void print_help_on_nonstandard_options();

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
    ECHO("Usage: " EXECUTABLE_NAME " [-options] class [args...]\n"
        "        (to execute a method main() of the class)\n"
        "    or " EXECUTABLE_NAME " [-options] -jar jarfile [args...]\n"
        "        (to execute the jar file)\n"
        "\n"
        "where options include:\n"
        "    -classpath <class search path of directories and zip/jar files>\n"
        "    -cp        <class search path of directories and zip/jar files>\n"
        "                  A " PORT_PATH_SEPARATOR_STR " separated list of directories, jar archives,\n"
        "                  and zip archives to search for class file\n"
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
        "    -enableassertions[:<package>|:<class>]\n"
        "    -ea[:<package>|:<class>]\n"
        "                  enable assertions - NOT SUPPORTED\n"
        "    -? -help      print this help message\n"
        "    -help properties\n"
        "                  help on system properties\n"
        "    -X            print help on non-standard options");
}

void parse_vm_arguments(Global_Env *p_env)
{
#ifdef _DEBUG
    TRACE2("arguments", "p_env->vm_arguments.nOptions  = " << p_env->vm_arguments.nOptions);
    for (int _i = 0; _i < p_env->vm_arguments.nOptions; _i++)
        TRACE2("arguments", "p_env->vm_arguments.options[ " << _i << "] = " << p_env->vm_arguments.options[_i].optionString);
#endif //_DEBUG

    apr_pool_t *pool;
    apr_pool_create(&pool, 0);

    for (int i = 0; i < p_env->vm_arguments.nOptions; i++) {
        const char* option = p_env->vm_arguments.options[i].optionString;

        if (begins_with(option, "-Xbootclasspath:")) {
            // replace boot class path by argument
            add_pair_to_properties(p_env->properties, "vm.boot.class.path", option + 16); 
            add_pair_to_properties(p_env->properties, "sun.boot.class.path", option + 16); 
        } else if (begins_with(option, "-Xbootclasspath/a:")) {
            // append argument to boot class path 
            char *bcp_old = (char *)properties_get_string_property((PropertiesHandle)&p_env->properties, "vm.boot.class.path");
            assert(bcp_old);
            char *bcp_new = apr_pstrcat(pool, bcp_old, PORT_PATH_SEPARATOR_STR, option + 18, NULL);
            add_pair_to_properties(p_env->properties, "vm.boot.class.path", bcp_new);
            add_pair_to_properties(p_env->properties, "sun.boot.class.path", bcp_new);
        } else if (begins_with(option, "-Xbootclasspath/p:")) {
            // prepend argument to boot class path
            char *bcp_old = (char*)properties_get_string_property((PropertiesHandle)&p_env->properties, "vm.boot.class.path");
            assert(bcp_old);
            char *bcp_new = apr_pstrcat(pool, option + 18, PORT_PATH_SEPARATOR_STR, bcp_old, NULL);
            add_pair_to_properties(p_env->properties, "vm.boot.class.path", bcp_new);
            add_pair_to_properties(p_env->properties, "sun.boot.class.path", bcp_new);
        } else if (begins_with(option, "-Xhelp:")) {
            const char* arg = option + strlen("-Xhelp:");

            if (begins_with(arg, "prop")) {
                print_vm_standard_properties();

            } else {
                ECHO("Unknown argument " << arg << " of -Xhelp: option");
            }

            LOGGER_EXIT(0);
        } else if (begins_with(option, "-Xjit:")) {
            // Do nothing here, just skip this option for later parsing
        } else if (strcmp(option, "-Xint") == 0) {
            add_pair_to_properties(p_env->properties, "vm.use_interpreter", "true");
#ifdef VM_STATS
        } else if (begins_with(option, "-Xstats:")) {
            vm_print_total_stats = true;
            const char* arg = option + strlen("-Xstats:");
            vm_print_total_stats_level = atoi(arg);
#endif
        } else if (strcmp(option, "-version") == 0) {
            // Print the version number and exit
            ECHO(VERSION);
            LOGGER_EXIT(0);
        } else if (strcmp(option, "-showversion") == 0) {
            // Print the version number and continue
            ECHO(VERSION);
        } else if (strcmp(option, "-fullversion") == 0) {
            // Print the version number and exit
            ECHO(VM_VERSION);
            LOGGER_EXIT(0);

        } else if (begins_with(option, "-Xgc:")) {
            // make prop_key to be "gc.<something>"
            char* prop_key = strdup(option + strlen("-X"));
            prop_key[2] = '.';
            TRACE2("init", prop_key << " = 1");
            add_pair_to_properties(p_env->properties, prop_key, "1");
            free(prop_key);

        } else if (begins_with(option, "-Xem:")) {
            const char* arg = option + strlen("-Xem:");
            add_pair_to_properties(p_env->properties, "em.properties", arg);

        } else if (begins_with(option, "-Xms")) {
            // cut -Xms
            const char* arg = option + 4;
            TRACE2("init", "gc.ms = " << arg);
            if (atoi(arg) == 0) {
                ECHO("Negative or invalid heap size. Default value will be used!");
            }
            add_pair_to_properties(p_env->properties, "gc.ms", arg);

        } else if (begins_with(option, "-Xmx")) {
            // cut -Xmx
            const char* arg = option + 4;
            TRACE2("init", "gc.mx = " << arg);
            if (atoi(arg) == 0) {
                ECHO("Negative or invalid heap size. Default value will be used!");
            }
            add_pair_to_properties(p_env->properties, "gc.mx", arg);
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
        else if (strcmp(option, "-Xverify") == 0) {
            p_env->verify_all = true;
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
        else if (strcmp(option, "-Xdumpstubs") == 0) {
            dump_stubs = true;
        }
        else if (strcmp(option, "-Xparallel_jit") == 0) {
            parallel_jit = true;
        }
        else if (strcmp(option, "-Xno_parallel_jit") == 0) {
            parallel_jit = false;
        }
        else if (strcmp(option, "-Xdumpfile:") == 0) {
            const char* arg = option + strlen("-Xdumpfile:");
            dump_file_name = arg;
        }
        else if (strcmp(option, "-XcleanupOnExit") == 0) {
            add_pair_to_properties(p_env->properties, "vm.cleanupOnExit", "true");       
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
        else {
            ECHO("Unknown option " << option << USE_JAVA_HELP);
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

static void set_threshold_list(char* list, LoggingLevel level, bool convert = false) {
    char *next;
    while (list) {
        if ( (next = strchr(list, ',')) ) {
            *next = '\0';
        }

        char* category = (convert) ? convert_logging_category(list) : list;
        set_threshold(category, level);

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
            set_out(next_sym, out + 1);
        }
        set_threshold_list(next_sym, level, false);
        if (out){
            *out = ':';
        }
    } else {
        ECHO("Unknown option " << arg << USE_JAVA_HELP);
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
                set_threshold_list(next_sym, INFO, true); // true = convert standard categories to internal
            } else {
                ECHO("Unknown option " << option << USE_JAVA_HELP);
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

struct cmd_arg
{
    bool substring;
    char *param;
    int length;
    int args;
};

static const cmd_arg supported_parameters[] =
{
    {false, "-classpath",                    strlen("-classpath"),                    1},
    {false, "-cp",                           strlen("-cp"),                           1},
    {true,  "-Xbootclasspath:",              strlen("-Xbootclasspath:"),              0},
    {true,  "-Xbootclasspath/a:",            strlen("-Xbootclasspath/a:"),            0},
    {true,  "-Xbootclasspath/p:",            strlen("-Xbootclasspath/p:"),            0},
    {false, "-?",                            strlen("-?"),                            0},
    {false, "-help",                         strlen("-help"),                         1},
    {true,  "-Xjit",                         strlen("-Xjit"),                         1},
    {false, "-Xint",                         strlen("-Xint"),                         0},
#ifdef VM_STATS
    {false, "-Xstats",                       strlen("-Xstats"),                       1},
#endif
    {false, "-version",                      strlen("-version"),                      0},
    {false, "-showversion",                  strlen("-showversion"),                  0},
    {false, "-fullversion",                  strlen("-fullversion"),                  0},
    {true,  "-ea",                           strlen("-ea"),                           0},
    {true,  "-enableassertions",             strlen("-enableassertions"),             0},
    {false, "-Xgc",                          strlen("-Xgc"),                          1},
    {true,  "-Xem",                          strlen("-Xem"),                          1},
    {true,  "-Xms",                          strlen("-Xms"),                          0},
    {true,  "-Xmx",                          strlen("-Xmx"),                          0},
    {true,  "-agentlib:",                    strlen("-agentlib:"),                    0},
    {true,  "-agentpath:",                   strlen("-agentpath:"),                   0},
    {false, "-Xdebug",                       strlen("-Xdebug"),                       0},
    {false, "-Xverify",                      strlen("-Xverify"),                      0},
    {false, "-verify",                       strlen("-verify"),                       0},
    {false, "-Xnoagent",                     strlen("-Xnoagent"),                     0},
    {true,  "-Xrun",                         strlen("-Xrun"),                         0},
    {true,  "-verbose",                      strlen("-verbose"),                      0},
    {true,  "-Xverbose",                     strlen("-Xverbose"),                     0},
    {true,  "-Xverboseconf:",                strlen("-Xverboseconf:"),                0},
    {true,  "-Xverboselog:",                 strlen("-Xverboselog:"),                 0},
    {true,  "-Xfileline",                    strlen("-Xfileline"),                    0},
    {true,  "-Xthread",                      strlen("-Xthread"),                      0},
    {true,  "-Xcategory",                    strlen("-Xcategory"),                    0},
    {true,  "-Xtimestamp",                   strlen("-Xtimestamp"),                   0},
    {true,  "-Xwarn",                        strlen("-Xwarn"),                        0},
    {true,  "-Xfunction",                    strlen("-Xfunction"),                        0},
#ifdef _DEBUG
    {true,  "-Xtrace",                       strlen("-Xtrace"),                       0},
    {true,  "-Xlog",                         strlen("-Xlog"),                         0},
#endif //_DEBUG
    {true,  "-D",                            strlen("-D"),                            0},
    {false, "-Xdumpstubs",                   strlen("-Xdumpstubs"),                   0},
    {false, "-Xparallel_jit",                strlen("-Xparallel_jit"),                 0},
    {false, "-Xno_parallel_jit",             strlen("-Xno_parallel_jit"),             0},
    {false, "-Xdumpfile",                    strlen("-Xdumpfile"),                    1},
    {false, "-XcleanupOnExit",               strlen("-XcleanupOnExit"),                0},
    {false, "-jar",                          strlen("-jar"),                          0}
}; //supported_parameters

static void print_help_on_nonstandard_options()
{
#ifdef _DEBUG
#    define DEBUG_OPTIONS_HELP \
         "    -Xlog[:<category>[:<file>]\n" \
         "              Switch debug logging on [for specified category only\n" \
         "              [and log that category to a file]]\n" \
         "    -Xtrace[:<category>[:<file>]\n" \
         "              Switch trace logging on [for specified category only\n" \
         "              [and log that category to a file]]\n"
#else
#    define DEBUG_OPTIONS_HELP
#endif //_DEBUG
#ifdef VM_STATS
#    define STATS_OPTIONS_HELP \
         "    -Xstats <mask>\n" \
         "              Generates different statistics\n"
#else
#    define STATS_OPTIONS_HELP
#endif // VM_STATS

    ECHO("    -Xbootclasspath:<PATH>\n"
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
         "    -Xverify\n"
         "              Do full bytecode verification\n"
         "    -Xfileline\n"
         "              Add source information to logging messages\n"
         "    -Xthread\n"
         "              Add thread id to logging messages\n"
         "    -Xcategory\n"
         "              Add category name to logging messages\n"
         "    -Xtimestamp\n"
         "              Add timestamp to logging messages\n"
         "    -Xfunction\n"
         "              Add function signature to logging messages\n"
         DEBUG_OPTIONS_HELP
         STATS_OPTIONS_HELP
         "    -Xint\n"
         "              Use interpreter to execute the program\n"
         "    -Xgc <gc options>\n"
         "              Specify gc specific options\n"
         "    -Xem <em options>\n"
         "              Specify em specific options\n"
		 "    -Xdumpstubs\n"
		 "              Writes stubs generated by LIL to disk\n"
		 "    -Xparallel_jit\n"
		 "              Launch compilation in parallel (default)\n"
		 "    -Xno_parallel_jit\n"
		 "              Do not launch compilation in parallel\n"
		 "    -Xdumpfile <file>\n"
		 "              Specifies a file name for the dump\n"
         "    -XcleanupOnExit\n"
         "              Excplicitly free VM resources before exit\n");
} //print_help_on_nonstandard_options

static JavaVMInitArgs* create_vm_arguments(int options_capacity)
{
    JavaVMInitArgs* vm_arguments = (JavaVMInitArgs*) STD_MALLOC(sizeof(JavaVMInitArgs));
    assert(vm_arguments);
    vm_arguments->version = JNI_VERSION_1_4;
    vm_arguments->nOptions = 0;
    vm_arguments->ignoreUnrecognized = JNI_FALSE;
    vm_arguments->options =
        (JavaVMOption*)STD_MALLOC(sizeof(JavaVMOption) * (options_capacity));
    assert(vm_arguments->options);

    return vm_arguments;
} //create_vm_arguments

void clear_vm_arguments(JavaVMInitArgs* vm_args)
{
    STD_FREE(vm_args->options);
    STD_FREE(vm_args);
} 

static void vm_arguments_append_classpath(JavaVMInitArgs* vm_arguments, const char* jar_file)
{
    static const char prefix[] = "-Djava.class.path=";

    // search for the last java.class.path property declaration
    for (int i = vm_arguments->nOptions - 1; i >= 0 ; i--)
    {
        const char* option = vm_arguments->options[i].optionString;
        if (strncmp(option, prefix, strlen(prefix)) == 0)
        {
            // if found, append jar file name
            char* new_option = (char*) STD_MALLOC(strlen(option) + 
                    strlen(PORT_PATH_SEPARATOR_STR) + strlen(jar_file) + 1);
            assert(new_option);

            strcpy(new_option, option);
            strcat(new_option, PORT_PATH_SEPARATOR_STR);
            strcat(new_option, jar_file);

            vm_arguments->options[i].optionString = new_option;
            return;
        }
    }

    // if not found, define java.class.path with jar file name
    char* option = (char*) STD_MALLOC(strlen(prefix) + strlen(jar_file) + 1);
    assert(option);

    strcpy(option, prefix);
    strcat(option, jar_file);

    vm_arguments->options[vm_arguments->nOptions].optionString = option;
    vm_arguments->nOptions ++;
    return;
} //vm_arguments_append_classpath

static int parse_vm_option(JavaVMInitArgs* vm_arguments, int argc, char *argv[], int i)
{
    // return 0, if arguments are over
    if (i >= argc)
        return 0;

    // if '-jar' met, thean vm options are over
    if (strcmp(argv[i], "-jar") == 0)
        return 0;

    const cmd_arg* supported_parameter;
    bool found = false;
    for (unsigned j = 0; j < sizeof(supported_parameters) / sizeof(cmd_arg); j++)
    {
        supported_parameter = &(supported_parameters[j]);
        if ((supported_parameters[j].substring &&
            strncmp(argv[i], supported_parameters[j].param,
            supported_parameters[j].length) == 0) ||
            (!supported_parameters[j].substring &&
            strcmp(argv[i], supported_parameters[j].param) == 0))
        {
            found = true;
            break;
        }
    }

    if (found) 
    {
        char* option;

        if (strcmp(argv[i], "-classpath") == 0 || strcmp(argv[i], "-cp") == 0) 
        {
            if (i + 1 >= argc) {
                ECHO("Classpath option "
                    << argv[i]
                    << " should be followed by classpath value"
                    USE_JAVA_HELP);
                LOGGER_EXIT(1);
            }

            char* class_path = argv[i + 1];
            static const char prefix[] = "-Djava.class.path=";

            option = (char*) STD_MALLOC(strlen(prefix) + strlen(class_path) + 1);
            assert(option);

            strcpy(option, prefix);
            strcat(option, class_path);
        } 
        else if (strcmp(argv[i], "-help") == 0 || strcmp(argv[i], "-?") == 0) 
        {
            if (i + 1 >= argc) 
            {
                // out a generic help message
                print_generic_help();
                LOGGER_EXIT(0);
            }

            const char* arg = argv[i + 1];
            if (strcmp(arg, "jit") != 0 && !begins_with(arg, "prop")) 
            {
                // out a generic help message
                print_generic_help();
                LOGGER_EXIT(0);
            }

            static const char prefix[] = "-Xhelp:";

            option = (char*) STD_MALLOC(strlen(prefix) + strlen(arg) + 1);
            assert(option);

            strcpy(option, prefix);
            strcat(option, arg);

        } 
        else if (supported_parameter->args == 1)
        {
            if (i + 1 >= argc) 
            {
                ECHO("Option " << argv[i] << " should be followed by a additional parameter");
                LOGGER_EXIT(1);
            }

            option = (char*) STD_MALLOC(strlen(argv[i]) + 1 + strlen(argv[i + 1]) + 1);
            assert(option);

            strcpy(option, argv[i]);
            strcat(option, ":");
            strcat(option, argv[i + 1]);
        }
        else if (strncmp(argv[i], "-enableassertions", 
                strlen("-enableassertions")) == 0 || 
                strncmp(argv[i], "-ea", strlen("-ea")) == 0) 
        {
            ECHO("Error: Assertions are not supported.\n"
                << " please invoke VM without -ea and -enableassertions options...");
            LOGGER_EXIT(1);
        }
        else
        {
            option = argv[i];
        }

        vm_arguments->options[vm_arguments->nOptions].optionString = option;
        vm_arguments->nOptions ++;

        return  1 + supported_parameter->args;
    }

    if (argv[i][0] != '-')
        return 0;

    if (strcmp(argv[i], "-X") == 0) 
    {
        print_help_on_nonstandard_options();
        LOGGER_EXIT(0);
    } else {
        ECHO("Unknown option " << argv[i] << USE_JAVA_HELP);
        LOGGER_EXIT(1);
    }

} //parse_vm_option

JavaVMInitArgs* parse_cmd_arguments(int argc, char *argv[], 
        char **p_class_name, char **p_jar_file, int *p_java_arg_num)
{
    *p_class_name = NULL;
    *p_jar_file = NULL;

    JavaVMInitArgs* vm_arguments = create_vm_arguments(argc);

    int i = 1; // skip argv[0], since it is a program name
    int inc = 0;
    do
    {
        inc = parse_vm_option(vm_arguments, argc, argv, i);
        i += inc;
    } while (inc > 0);

    if (i < argc)
    {
        if (strcmp(argv[i], "-jar") == 0)
        {
            i++;
            if (i >= argc) {
                ECHO("Option -jar must be followed by a jar file name");
                LOGGER_EXIT(1);
            }

            *p_jar_file = argv[i];
            vm_arguments_append_classpath(vm_arguments, *p_jar_file);
        }
        else
        {
            *p_class_name = argv[i];
        }
    }

    *p_java_arg_num = argc - i - 1;

    return vm_arguments;
} //parse_cmd_arguments

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
