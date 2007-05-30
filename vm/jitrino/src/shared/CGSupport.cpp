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
* @author Intel, Vitaly N. Chaiko
* @version $Revision: 1.6.22.4 $
*/

#include "CGSupport.h"
#include "Stl.h"
#include <string>
#include <functional>
#include "Type.h"

#define QUAL_NAME_LENGTH 1024

namespace Jitrino {

const char* bcOffset2LIRHandlerName = "bcOffset2InstructionLIRHandler";
const char* bcOffset2HIRHandlerName = "bcOffset2InstructionHIRHandler";
const char* lostBCOffsetHandlerName = "bcOffsetHandler";


void initHandleMap(MemoryManager& mm, MethodDesc* meth) {
    StlHashMap< POINTER_SIZE_INT, void* >* handleMap;
    handleMap = new(mm) StlHashMap< POINTER_SIZE_INT, void* >(mm);
    assert(meth->getHandleMap() == NULL);
    meth->setHandleMap(handleMap);
}
//static Timer* addMapHandlerTimer = NULL;
void addContainerHandler(void* mapHandler, const char* name, MethodDesc* meth) {
    StlHashMap< POINTER_SIZE_INT, void* >* handleMap;
    handleMap = (StlHashMap< POINTER_SIZE_INT, void* >*) meth->getHandleMap();
    POINTER_SIZE_INT keyVal = (POINTER_SIZE_INT) name;

    assert(handleMap->find(keyVal) == handleMap->end());
    (*handleMap)[keyVal] =  mapHandler;
}

//static Timer* getMapHandlerTimer = NULL;
void* getContainerHandler(const char* name, MethodDesc* meth) {  

    StlHashMap< POINTER_SIZE_INT, void* >* handleMap;
    handleMap = (StlHashMap< POINTER_SIZE_INT, void* >*) meth->getHandleMap();
    POINTER_SIZE_INT keyVal = (POINTER_SIZE_INT) name;
    assert(handleMap->find(keyVal) != handleMap->end());
    return handleMap->find(keyVal)->second; 
}



/************************************************************************/
size_t getNumBCMapEntries(void* vectorHandler) { 
    assert(vectorHandler);
    StlVector<uint16>* theVector = (StlVector<uint16>*) vectorHandler;
    return theVector->size();
}

uint16 getBCMappingEntry(void* vectorHandler, uint32 key) { 
    assert(vectorHandler);
    StlVector<uint16>* theVector = (StlVector<uint16>*) vectorHandler;
    if (key >= theVector->size()) return ILLEGAL_BC_MAPPING_VALUE;
    return (*theVector)[key];
}

void setBCMappingEntry(void* vectorHandler, uint32 key, uint16 value) {
    assert(vectorHandler);
    StlVector<uint16>* theVector = (StlVector<uint16>*) vectorHandler;
    if (key >=  theVector->size()) {
        assert(key<1000*1000);//we do not have methods with 1M insts..
        size_t newSize = (size_t)(key * 1.5);
        theVector->resize(newSize, ILLEGAL_BC_MAPPING_VALUE);
    }
    assert(key<theVector->size());
    (*theVector)[key] =  value;
}

}
