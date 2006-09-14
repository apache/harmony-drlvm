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
 * @author Intel, Mikhail Y. Fursov
 * @version $Revision: 1.6.24.4 $
 *
 */

#ifndef _EXCEPTIONS_H_
#define _EXCEPTIONS_H_

#include "assert.h"

class IndexOutOfBoundsException {
public:
    IndexOutOfBoundsException() : msg("") { assert(0); }
    IndexOutOfBoundsException(const char* message) : msg(message) { assert(0); }
    const char* getMessage() {return msg;}
private:
    const char* msg;
};

class TranslationException {
public:
    TranslationException() { assert(0); }
};

#endif // _EXCEPTIONS_H_
