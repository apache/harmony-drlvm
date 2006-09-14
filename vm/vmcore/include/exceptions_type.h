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
 * @author Pavel Afremov
 * @version $Revision: 1.1.2.1.2.2 $
 */

// This describes the core VM interface to exception manipulation, throwing, and catching

#ifndef _INTERFACE_EXCEPTIONS_TYPE_H_
#define _INTERFACE_EXCEPTIONS_TYPE_H_

//#include "jni.h"
#include "open/types.h"

#ifdef __cplusplus
extern "C" {
#endif
struct Exception {
    ManagedObject* exc_object;
    Class* exc_class;
    const char* exc_message;
    ManagedObject* exc_cause;
};

#ifdef __cplusplus
}
#endif
#endif // !_INTERFACE_EXCEPTIONS_TYPE_H_
