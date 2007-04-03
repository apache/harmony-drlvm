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

#include <assert.h>
#include "VMInterface.h"
#include "PlatformDependant.h"

namespace Jitrino {

/*
The following struct and array contains a mapping between RuntimeHelperId 
and its string representation. 
The array must be
    ordered by RuntimeHelperId
    must cover all available helpers
The id field exists only in debug build and is excluded from the release 
bundle. It's used to control whether the array is arranged properly.
*/
#ifdef _DEBUG
#define DECL_HELPER_ITEM(a) { CompilationInterface::Helper_##a, #a }
#else
#define DECL_HELPER_ITEM(a) { #a }
#endif

static struct {
#ifdef _DEBUG
    CompilationInterface::RuntimeHelperId id;
#endif
    const char *    name;
} 
runtime_helpers_names[] = {
    DECL_HELPER_ITEM(Null),
    DECL_HELPER_ITEM(NewObj_UsingVtable),
    DECL_HELPER_ITEM(NewVector_UsingVtable),
    DECL_HELPER_ITEM(NewObj),
    DECL_HELPER_ITEM(NewVector),
    DECL_HELPER_ITEM(NewMultiArray),
    DECL_HELPER_ITEM(LdInterface),
    DECL_HELPER_ITEM(LdRef),
    DECL_HELPER_ITEM(ObjMonitorEnter),
    DECL_HELPER_ITEM(ObjMonitorExit),
    DECL_HELPER_ITEM(TypeMonitorEnter),
    DECL_HELPER_ITEM(TypeMonitorExit),
    DECL_HELPER_ITEM(Cast),
    DECL_HELPER_ITEM(IsInstanceOf),
    DECL_HELPER_ITEM(InitType),
    DECL_HELPER_ITEM(IsValidElemType),
    DECL_HELPER_ITEM(Throw_KeepStackTrace),
    DECL_HELPER_ITEM(Throw_SetStackTrace),
    DECL_HELPER_ITEM(Throw_Lazy),
    DECL_HELPER_ITEM(EndCatch),
    DECL_HELPER_ITEM(NullPtrException),
    DECL_HELPER_ITEM(ArrayBoundsException),
    DECL_HELPER_ITEM(ElemTypeException),
    DECL_HELPER_ITEM(DivideByZeroException),
    DECL_HELPER_ITEM(Throw_LinkingException), 
    DECL_HELPER_ITEM(CharArrayCopy),
    DECL_HELPER_ITEM(DivI32),
    DECL_HELPER_ITEM(DivU32),
    DECL_HELPER_ITEM(DivI64),
    DECL_HELPER_ITEM(DivU64),
    DECL_HELPER_ITEM(DivSingle),
    DECL_HELPER_ITEM(DivDouble),
    DECL_HELPER_ITEM(RemI32),
    DECL_HELPER_ITEM(RemU32),
    DECL_HELPER_ITEM(RemI64),
    DECL_HELPER_ITEM(RemU64),
    DECL_HELPER_ITEM(RemSingle),
    DECL_HELPER_ITEM(RemDouble),
    DECL_HELPER_ITEM(MulI64),
    DECL_HELPER_ITEM(ShlI64),
    DECL_HELPER_ITEM(ShrI64),
    DECL_HELPER_ITEM(ShruI64),
    DECL_HELPER_ITEM(ConvStoI32),
    DECL_HELPER_ITEM(ConvStoI64),
    DECL_HELPER_ITEM(ConvDtoI32),
    DECL_HELPER_ITEM(ConvDtoI64),
    DECL_HELPER_ITEM(EnableThreadSuspension),
    DECL_HELPER_ITEM(GetTLSBase),
    DECL_HELPER_ITEM(MethodEntry),
    DECL_HELPER_ITEM(MethodExit),
    DECL_HELPER_ITEM(WriteBarrier),
#undef DECL_HELPER_ITEM
};
static const unsigned runtime_helpers_names_count = sizeof(runtime_helpers_names)/sizeof(runtime_helpers_names[0]);

#ifdef _DEBUG
static inline void checkArray(void) {
    static bool doCheck = true;
    if( !doCheck ) return;

    doCheck = false;
    // all helpers must be covered
    for( size_t i=0; i<runtime_helpers_names_count; i++ ) {
        // the map must be ordered by RuntimeHelperId
        assert( (size_t)runtime_helpers_names[i].id == i );
    }
}
#else
#define checkArray()
#endif 

const char*
CompilationInterface::getRuntimeHelperName( RuntimeHelperId helperId ){
    checkArray();
    assert( Num_Helpers > helperId );
    return runtime_helpers_names[helperId].name;
}

CompilationInterface::RuntimeHelperId CompilationInterface::str2rid( const char * helperName ) {
    checkArray();
    for( size_t i = 0; i<runtime_helpers_names_count; i++ ) {
        if( !strcmpi(helperName, runtime_helpers_names[i].name)) return (RuntimeHelperId)i;
    }
    return Helper_Null;
}


} // ~namespace Jitrino
