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
* @author Alexey V. Varlamov
* @version $Revision$
*/  
#ifndef _PORT_TIMER_H_
#define _PORT_TIMER_H_

#include "port_general.h"

#ifdef __cplusplus
extern "C" {
#endif

    /**
     * High resolution timer, in nanoseconds.
     */
    typedef apr_int64_t apr_nanotimer_t;

    /**
    * Returns the value of the system timer with the best possible accuracy.
    * This value is not tied to the absolute time, but intended for precise 
    * measuring of elapsed time intervals.
    */
    APR_DECLARE(apr_nanotimer_t) port_nanotimer();

#ifdef __cplusplus
}
#endif
#endif /*_PORT_TIMER_H_*/
