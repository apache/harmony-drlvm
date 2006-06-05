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
 * @author Alexander V. Astapchuk
 * @version $Revision: 1.4.12.3.4.4 $
 */
 
/**
 * @file 
 * @brief Debugging stuff declaration - tracing and logging utilities.
 */
 
#if !defined(__TRACE_H_INCLUDED__)
#define __TRACE_H_INCLUDED__

#include "../shared/PlatformDependant.h"

#include <string>

namespace Jitrino {
namespace Jet {

class JFrame;

/**
 * @def LOG_FILE_NAME
 * @brief Name of the file where the \link #dbg debug output \endlink goes.
 */
#define LOG_FILE_NAME           "jet.log"

/**
 * @def RUNTIME_LOG_FILE_NAME
 * @brief Name of the file where the \link #rt_dbg debug output from the 
 *        managed code \endlink goes.
 */
#define RUNTIME_LOG_FILE_NAME   "jet.rt.log"

/**
 * @brief Performs debugging output, logged to file #LOG_FILE_NAME
 *
 * The function creates file if it does not exist, or overwrites its content
 * if it does exist.
 * @note Use with caution in multi-threaded environment. The function itself 
 *       is thread-safe (no static variables used except of \c FILE*). 
 *       However no syncronization performed during the output, so the output
 *       from different threads may interleave with each other.
 * 
 * @param frmt - format specificator, same as printf()'s
 */
void dbg(const char * frmt, ...);

/**
 * @brief Used to perform debugging output from the managed code, logged to 
 *        #RUNTIME_LOG_FILE_NAME.
 *
 * The output goes to file #RUNTIME_LOG_FILE_NAME to avoid intermixing with
 * output produced by #dbg(const char*, ...).
 *
 * The function also uses 2 counters: depth counter and total number of 
 * outputs.
 *
 * The depth counter incremented if the \c msg string starts with 'enter',
 * and decremented if the string starts with 'exit' (case-insensitive). The 
 * 'enter' and 'exit' strings are the strings used to track method's 
 * \link #DBG_TRACE_EE enter/exit \endlink, so the depth reflects somehow the
 * real call depth.
 * 
 * Total number of outputs is simple counter incremented on each rt_dbg call.
 *  
 * The output is the \c msg string, preceded with depth counter, and with 
 * total outputs counter at the end.
 *
 * The counters may be used to install breakpoints by value.
 *
 * @note Use with caution in multi-threaded environment. The function itself
 *       is thread-safe, but counters work is perfromed without interlocked 
 *       operations, somay be inadequate and may show different values from 
 *       one run to another for multiple threads.
 * @note The depth counter does not reflect if method finishes abruptly.
 *
 * @param msg - message to print out
 */
void __stdcall rt_dbg(const char * msg) stdcall__;

/**
 * @brief Dumps the native stack frame, addressed by \c addr.
 * @note implementation is obsolete, need update.
 * @todo implementation is obsolete, need update.
 */
void dump_frame(char * ptr);

/**
 * @brief Prints out the state of the JFrame.
 * @param name - a name to print out before the JFrame dump, to identify the 
 *        dump in the trace log
 * @param pjframe - a JFrame instance to dump
 */
void dbg_dump_jframe(const char * name, const JFrame * pjframe);



 /**
  * @brief Removes leading and trailing white spaces.
  */
::std::string trim(const char * p);


}
}; // ~namespace Jitrino::Jet


#endif		// ~__TRACE_H_INCLUDED__
