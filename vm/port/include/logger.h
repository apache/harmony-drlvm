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
* @author Roman S. Bushmanov, Dmitry B. Yershov
* @version $Revision: 1.1.2.2.4.3 $
*/  

#ifndef LOGGER_H
#define LOGGER_H

#define LOG4CXX
#define LOG4CXX_STATIC 

#include "open/types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
    * Enum of logging levels
    */
typedef enum {
    DIE  = 0,
    WARN,
    INFO,
    LOG,
    TRACE
} LoggingLevel;

/**
 * Header format flags. Binary AND operation should be used to combine flags.
 */
typedef unsigned HeaderFormat;
#define HEADER_EMPTY 0
#define HEADER_TIMESTAMP 1
#define HEADER_FILELINE 2
#define HEADER_CATEGORY 4
#define HEADER_THREAD_ID 8
#define HEADER_LEVEL 16
#define HEADER_FUNCTION 32

#if !defined(__LOG4CXX_FUNC__)
#   if defined(_MSC_VER) && (defined(__INTEL_COMPILER) || _MSC_VER >= 1300)
#       define __LOG4CXX_FUNC__ __FUNCSIG__
#   elif defined(__GNUC__)
#       define __LOG4CXX_FUNC__ __PRETTY_FUNCTION__
#   else
#       define __LOG4CXX_FUNC__ ""
#   endif
#endif


/**
* Inits log system.  
*/
VMEXPORT void init_log_system(void *portLib);

/**
* shutdown log system.  
*/
VMEXPORT void shutdown_log_system();

/**
* Sets loggers logging levels from file  
*/
VMEXPORT void set_logging_level_from_file(const char* filename);

/**
 * Passes the message specified with level assigned to the category specified for logging.  
 */
VMEXPORT void log4cxx_from_c(const char *category, LoggingLevel level, 
                             const char* message, const char* file, 
                             const char* func, int line);

/**
 * Assigns threshold to a category. All the messages having a lower logging level will 
 * be ignored by the category.   
 */
VMEXPORT void set_threshold(const char *category, LoggingLevel level);

/**
 * Checks if the logging level specified is enabled for the category given. 
 */
VMEXPORT unsigned is_enabled(const char *category, LoggingLevel level);

VMEXPORT unsigned is_warn_enabled(const char *category);
VMEXPORT unsigned is_info_enabled(const char *category);

typedef enum {
    DISABLED  = 0,
    ENABLED,
    UNKNOWN
} CachedState;

struct LogSite {
    CachedState state;
    struct LogSite *next;
};

typedef struct LogSite LogSite;

VMEXPORT unsigned is_log_enabled(const char *category, LogSite *site);
VMEXPORT unsigned is_trace_enabled(const char *category, LogSite *site);

/**
* Redirects category (and all subcategories) output to a file. 
* If the file is <code>NULL</code>, removes previously assigned redirection (if any).
*/
VMEXPORT void set_out(const char *category, const char* file);

/**
 * Sets the header format for the category specified. Use <code>HeaderFormat</code>
 * flags combined by AND operation to configure format.
 */
VMEXPORT void set_header_format(const char *category, HeaderFormat format);

/**
 * Sets the log file path pattern to use for thread specific logging.
 * Specify <code>NULL</code> pattern to turn off per-thread output.
 * Use %t specifier to be replaced by a thread id.
 */
VMEXPORT void set_thread_specific_out(const char* category, const char* pattern);    

/**
 * Write formatted data to a newly allocated string. 
 * Use STD_FREE to release allocated memory.
 */
VMEXPORT const char* log_printf(const char* format, ...);

#ifdef __cplusplus
}
#endif

#endif // LOGGER_H

