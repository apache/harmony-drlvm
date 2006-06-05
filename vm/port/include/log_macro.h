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
 * @author Alexey V. Varlamov, Evgueni Brevnov
 * @version $Revision: 1.1.2.1.4.4 $
 */  
#ifndef _UTIL_LOG_MACRO_H
#define _UTIL_LOG_MACRO_H

#include <stdio.h>
#include <stdlib.h>


/**
 * @file
 *
 * Basic logging is used during startup.
 */


/**
 * Macro expansion. It is used, for example, to combine
 * a filename and a line number in
 * <code>__FILELINE__</code> macro.
 */
#define EXPAND_(a) # a
#define EXPAND(a) EXPAND_(a)

/**
 * Concatenate the file name and the line number.
 */
#define LOG_FILELINE __FILE__ ":" EXPAND(__LINE__)

/**
 * Create a log message header.
 */
#define LOG_HEAD ":" LOG_FILELINE ": "

/**
 * @def ABORT(message)
 * @brief Lightweight implementation for error handling.
 *
 * It prints a simple string message to stderr
 * and exits the program. This macro call should be
 * used when logging subsystem is not yet fully initialized.
 */
#define ABORT(message) { \
    fprintf(stderr, "Internal error" LOG_HEAD message); \
    fflush(stderr); \
    abort(); \
}

/**
 * Lightweight macro to replace a standard C
 * <code>assert()</code>.   
 */
#ifdef NDEBUG
#    define LOG_ASSERT(assertion, message) /* Asserts are disabled, \
  * reset NDEBUG macro to turn them on. */
#    define LOG_DEBUG_F(printf_args) /* Debug is disabled, \
  * reset NDEBUG macro to turn it on. */
#else
#    define LOG_DEBUG_F(printf_args) printf printf_args
#    define LOG_ASSERT(assertion, message) { \
        if (!(assertion)) { \
            ABORT("Assert failed" message # assertion); \
        } \
}

#endif

#endif // _UTIL_LOG_MACRO_H 

