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
* @author Roman S. Bushmanov
* @version $Revision: 1.1.2.1.4.3 $
*/  

#ifndef _C_LOG_H_
#define _C_LOG_H_

#include <assert.h>
#include "logger.h"
#include "port_malloc.h"

#define LOGGER_EXIT(code) { \
    shutdown_log_system(); \
    ::exit(code); \
}

#define DIE2(category, message) { \
    const char* formatted = log_printf message; \
    log4cxx_from_c(category, DIE, formatted, __FILE__, __LOG4CXX_FUNC__, __LINE__); \
    STD_FREE((void*)formatted); \
    shutdown_log_system(); \
    abort(); \
}

#define WARN2(category, message) { \
    if (is_warn_enabled(category)) { \
    const char* formatted = log_printf message; \
    log4cxx_from_c(category, WARN, formatted, __FILE__, __LOG4CXX_FUNC__, __LINE__); \
    STD_FREE((void*)formatted); \
    } \
}

#define INFO2(category, message) { \
    if (is_info_enabled(category)) { \
    const char* formatted = log_printf message; \
    log4cxx_from_c(category, INFO, formatted, __FILE__, __LOG4CXX_FUNC__, __LINE__); \
    STD_FREE((void*)formatted); \
    } \
}

#ifdef NDEBUG

#define LOG2(category, message) 
#define TRACE2(category, message)

#else // NDEBUG

#define LOG2(category, message) { \
    static LogSite logSite = {UNKNOWN, NULL}; \
    if (logSite.state && is_log_enabled(category, &logSite)) { \
    const char* formatted = log_printf message; \
    log4cxx_from_c(category, LOG, formatted, __FILE__, __LOG4CXX_FUNC__, __LINE__); \
    STD_FREE((void*)formatted); \
    } \
}

#define TRACE2(category, message) { \
    static LogSite logSite = {UNKNOWN, NULL}; \
    if (logSite.state && is_trace_enabled(category, &logSite)) { \
    const char* formatted = log_printf message; \
    log4cxx_from_c(category, TRACE, formatted, __FILE__, __LOG4CXX_FUNC__, __LINE__); \
    STD_FREE((void*)formatted); \
    } \
}


#endif // NDEBUG

#ifdef LOG_DOMAIN

#define DIE(message) DIE2(LOG_DOMAIN, message)
#define WARN(message) WARN2(LOG_DOMAIN, message)
#define INFO(message) INFO2(LOG_DOMAIN, message)
#define LOG(message) LOG2(LOG_DOMAIN, message)
#define TRACE(message) TRACE2(LOG_DOMAIN, message)

#ifdef NDEBUG

#define VERIFY(expr, message) { \
    if (!(expr)) { \
    const char* formatted_msg = log_printf message; \
    DIE(("Assertion failed: %s\n %s", #expr, formatted_msg)); \
    STD_FREE((void*)formatted_msg); \
    } \
}
#define ASSERT(expr, message)

#else //NDEBUG

#define VERIFY(expr, message) {\
        if(!(expr)) { \
        const char* formatted_msg = log_printf message; \
        const char* complete_msg = log_printf("Assertion failed: %s\n %s", #expr, formatted_msg); \
        log4cxx_from_c(LOG_DOMAIN, DIE, complete_msg, __FILE__, __LOG4CXX_FUNC__, __LINE__); \
        STD_FREE((void*)formatted_msg); \
        STD_FREE((void*)complete_msg); \
        shutdown_log_system(); \
        assert(expr); \
        }\
}

#define ASSERT(expr, message)  VERIFY(expr, message)

#endif //NDEBUG 

#endif // LOG_DOMAIN

#endif // _C_LOG_H_
