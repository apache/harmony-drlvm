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
#include "atomics.h"

// MSVC barrier intrinsics setup
#if _MSC_VER < 1400
    // VC++ 2003
    extern "C" void _ReadWriteBarrier();
    extern "C" void _WriteBarrier();
#else
    // VC++ 2005
    #include <intrin.h>
#endif
#pragma intrinsic (_ReadWriteBarrier)
#pragma intrinsic (_WriteBarrier)

void MemoryReadWriteBarrier() {
    _ReadWriteBarrier();
}

void MemoryWriteBarrier() {
    _WriteBarrier();
}

