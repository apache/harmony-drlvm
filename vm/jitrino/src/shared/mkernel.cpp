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
 * @version $Revision: 1.2.8.2.4.4 $
 */
 /**
  * @file
  * @brief Contains implementation of utilities declared in #mkernel.h.
  */
#include "mkernel.h"

#ifdef PLATFORM_POSIX
    #include <unistd.h>
#endif

namespace Jitrino {

const unsigned Runtime::num_cpus = Runtime::init_num_cpus();

unsigned Runtime::init_num_cpus(void)
{
#ifdef PLATFORM_POSIX
    int num = (int)sysconf(_SC_NPROCESSORS_ONLN);
    return num == -1 ? 0 : 1;
#else
    SYSTEM_INFO sinfo;
    GetSystemInfo(&sinfo);
    return sinfo.dwNumberOfProcessors;
#endif    
}

}; // ~namespace Jitrino
