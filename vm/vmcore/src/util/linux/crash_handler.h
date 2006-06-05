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
 * @author Intel, Evgueni Brevnov
 * @version $Revision: 1.1.2.1.4.3 $
 */  

#ifndef _CRASH_HANDLER_H
#define _CRASH_HANDLER_H

/**
 * \file
 * Provides definition needed to install crash handler (from crash_handler.cpp)
 */

/**
 * Initializes the static state needed for crash handler.
 */
void init_crash_handler();

/**
 * Installs specified signal handler to call gdb.
 *
 * @param signum A signal number constant, e.g. SIGABRT or SIGSEGV
 */
void install_crash_handler(int signum);

#endif // _CRASH_HANDLER_H
