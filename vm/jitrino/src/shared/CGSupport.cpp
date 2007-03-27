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
//#include "Timer.h"

#define QUAL_NAME_LENGTH 1024

namespace Jitrino {

typedef ::std::pair<POINTER_SIZE_INT, POINTER_SIZE_INT> KeyPair;

StlHashMap< uint64, void* > * handlMap = NULL;

const char* bcOffset2LIRHandlerName = "bcOffset2InstructionLIRHandler";
const char* bcOffset2HIRHandlerName = "bcOffset2InstructionHIRHandler";
const char* lostBCOffsetHandlerName = "bcOffsetHandler";

MemoryManager* handleMapMM = NULL;


//static Timer* mapTimer = NULL;
//static Timer* mapHandlerTimer = NULL;

void initHandleMap(MemoryManager& mm, MethodDesc* meth) {
    StlHashMap< POINTER_SIZE_INT, void* >* handleMap;
    handleMap = new(mm) StlHashMap< POINTER_SIZE_INT, void* >(mm);
    assert(meth->getHandleMap() == NULL);
    meth->setHandleMap(handleMap);
    //handleMapMM = &mm;
}
//static Timer* addMapHandlerTimer = NULL;
void addContainerHandler(void* mapHandler, const char* name, MethodDesc* meth) {
    //POINTER_SIZE_INT methID = (POINTER_SIZE_INT)meth;
    //ltoa((long)methID, ptrToMethStr, 8);
    //::std::string qualHandlerName(name);
    //qualHandlerName += "::";
    //qualHandlerName += meth->getParentType()->getName();
    //qualHandlerName += "::";
    //qualHandlerName += meth->getName(); 
    //qualHandlerName += meth->getSignatureString();
    //const char* qualName = qualHandlerName.c_str();

    //char qualName[QUAL_NAME_LENGTH] = "";
    //strcpy(qualName, name);
    //strcat(qualName, meth->getParentType()->getName());
    //strcat(qualName, meth->getName());
    //strcat(qualName, meth->getSignatureString());

    StlHashMap< POINTER_SIZE_INT, void* >* handleMap;
    handleMap = (StlHashMap< POINTER_SIZE_INT, void* >*) meth->getHandleMap();
    POINTER_SIZE_INT keyVal = (POINTER_SIZE_INT) name;
    //KeyPair keyVal((POINTER_SIZE_INT)name, (POINTER_SIZE_INT) meth);

    assert(handleMap->find(keyVal) == handleMap->end());
    (*handleMap)[keyVal] =  mapHandler;
}
//static Timer* remMapHandlerTimer = NULL;
void removeContainerHandler(const char* name, MethodDesc* meth) {  
    //PhaseTimer tm(remMapHandlerTimer, "remMapHandler");
    //::std::string qualHandlerName(name);
    //qualHandlerName += "::";
    //qualHandlerName += meth->getParentType()->getName();
    //qualHandlerName += "::";
    //qualHandlerName += meth->getName(); 
    //qualHandlerName += meth->getSignatureString();
    //const char* qualName = qualHandlerName.c_str();

    //char qualName[QUAL_NAME_LENGTH] = "";
    //strcpy(qualName, name);
    //strcat(qualName, meth->getParentType()->getName());
    //strcat(qualName, meth->getName());
    //strcat(qualName, meth->getSignatureString());
    //assert(handlMap->find(qualName) != handlMap->end());

    StlHashMap< POINTER_SIZE_INT, void* >* handleMap;
    handleMap = (StlHashMap< POINTER_SIZE_INT, void* >*) meth->getHandleMap();
    POINTER_SIZE_INT keyVal = (POINTER_SIZE_INT) name;
    //KeyPair keyVal((POINTER_SIZE_INT)name, (POINTER_SIZE_INT) meth);

    assert(handleMap->find(keyVal) != handleMap->end());
    handleMap->erase(keyVal); 
}
//static Timer* getMapHandlerTimer = NULL;
void* getContainerHandler(const char* name, MethodDesc* meth) {  
    //PhaseTimer tm(getMapHandlerTimer, "getMapHandler");
    //::std::string qualHandlerName(name);
    //qualHandlerName += "::";
    //qualHandlerName += meth->getParentType()->getName();
    //qualHandlerName += "::";
    //qualHandlerName += meth->getName(); 
    //qualHandlerName += meth->getSignatureString();
    //const char* qualName = qualHandlerName.c_str();

    //char qualName[QUAL_NAME_LENGTH] = "";
    //strcpy(qualName, name);
    //strcat(qualName, meth->getParentType()->getName());
    //strcat(qualName, meth->getName());
    //strcat(qualName, meth->getSignatureString());
    //assert(handlMap->find(qualName) != handlMap->end());
    
    StlHashMap< POINTER_SIZE_INT, void* >* handleMap;
    handleMap = (StlHashMap< POINTER_SIZE_INT, void* >*) meth->getHandleMap();
    POINTER_SIZE_INT keyVal = (POINTER_SIZE_INT) name;
    //KeyPair keyVal((POINTER_SIZE_INT)name, (POINTER_SIZE_INT) meth);

    assert(handleMap->find(keyVal) != handleMap->end());
    return handleMap->find(keyVal)->second; 
}

bool isContainerHandlerExist(const char* name, MethodDesc* meth) {  
    //PhaseTimer tm(mapHandlerTimer, "MapHandler");
    //::std::string qualHandlerName(name);
    //qualHandlerName += "::";
    //qualHandlerName += meth->getParentType()->getName();
    //qualHandlerName += "::";
    //qualHandlerName += meth->getName(); 
    //qualHandlerName += meth->getSignatureString();
    //const char* qualName = qualHandlerName.c_str();

    //char qualName[QUAL_NAME_LENGTH] = "";
    //strcpy(qualName, name);
    //strcat(qualName, meth->getParentType()->getName());
    //strcat(qualName, meth->getName());
    //strcat(qualName, meth->getSignatureString());

    StlHashMap< POINTER_SIZE_INT, void* >* handleMap;
    handleMap = (StlHashMap< POINTER_SIZE_INT, void* >*) meth->getHandleMap();
    POINTER_SIZE_INT keyVal = (POINTER_SIZE_INT) name;
    //KeyPair keyVal((POINTER_SIZE_INT)name, (POINTER_SIZE_INT) meth);

    return (handleMap->find(keyVal) != handleMap->end());
}
uint64 getMapHandlerSize(const char* name, MethodDesc* meth) {  
    //PhaseTimer tm(mapHandlerTimer, "MapHandler");
    //::std::string qualHandlerName(name);
    //qualHandlerName += "::";
    //qualHandlerName += meth->getParentType()->getName();
    //qualHandlerName += "::";
    //qualHandlerName += meth->getName(); 
    //qualHandlerName += meth->getSignatureString();
    //const char* qualName = qualHandlerName.c_str();

    //char qualName[QUAL_NAME_LENGTH] = "";
    //strcpy(qualName, name);
    //strcat(qualName, meth->getParentType()->getName());
    //strcat(qualName, meth->getName());
    //strcat(qualName, meth->getSignatureString());

    StlHashMap< POINTER_SIZE_INT, void* >* handleMap;
    handleMap = (StlHashMap< POINTER_SIZE_INT, void* >*) meth->getHandleMap();
    POINTER_SIZE_INT keyVal = (POINTER_SIZE_INT) name;
    //KeyPair keyVal((POINTER_SIZE_INT)name, (POINTER_SIZE_INT) meth);

    assert(handleMap->find(keyVal) != handleMap->end());
    StlHashMap<uint64, uint64>* mapHandler = (StlHashMap<uint64, uint64>*) handleMap->find(keyVal)->second;

    return (uint64)mapHandler->size();
}
void incVectorHandlerSize(const char* name, MethodDesc* meth, size_t incSize) {
    StlHashMap< POINTER_SIZE_INT, void* >* handleMap;
    handleMap = (StlHashMap< POINTER_SIZE_INT, void* >*) meth->getHandleMap();
    POINTER_SIZE_INT keyVal = (POINTER_SIZE_INT) name;
    //KeyPair keyVal((POINTER_SIZE_INT)name, (POINTER_SIZE_INT) meth);

    assert(handleMap->find(keyVal) != handleMap->end());
    StlVector<uint64>* vector = (StlVector<uint64>*)handleMap->find(keyVal)->second;  
    vector->resize(vector->size() + incSize, ILLEGAL_VALUE);
}
uint64 getVectorSize(const char* name, MethodDesc* meth) {
    StlHashMap< POINTER_SIZE_INT, void* >* handleMap;
    handleMap = (StlHashMap< POINTER_SIZE_INT, void* >*) meth->getHandleMap();
    POINTER_SIZE_INT keyVal = (POINTER_SIZE_INT) name;
    //KeyPair keyVal((POINTER_SIZE_INT)name, (POINTER_SIZE_INT) meth);

    assert(handleMap->find(keyVal) != handleMap->end());
    StlVector<uint64>* vector = (StlVector<uint64>*)handleMap->find(keyVal)->second;  
    return (uint64)vector->size();
}
void enumerateHandlMap(MethodDesc* meth) {  
    StlHashMap< POINTER_SIZE_INT, void* >* handleMap;
    handleMap = (StlHashMap< POINTER_SIZE_INT, void* >*) meth->getHandleMap();

    StlHashMap< POINTER_SIZE_INT, void* >::const_iterator citer;

    ::std::cout << "=================" << ::std::endl;
    ::std::cout<<"Method: "<<meth->getName()<<::std::endl;

    for (citer = handleMap->begin(); citer != handleMap->end(); citer++) {
        ::std::cout << (const char*)citer->first << " " << citer->second << ::std::endl;
    }
    ::std::cout << "=================" << ::std::endl;
}
/************************************************************************/
//static Timer* getEntryTimer = NULL;
uint64 getMapEntry(void* mapHandler, uint64 key) { 
    //PhaseTimer tm(getEntryTimer, "getEntry");
    assert(mapHandler);
    StlHashMap<uint64, uint64>* theMap = (StlHashMap<uint64, uint64>*) mapHandler;
    if (theMap->find(key) == theMap->end()) return ILLEGAL_VALUE;
    return theMap->find(key)->second;
}
//static Timer* setEntryTimer = NULL;
void setMapEntry(void* mapHandler, uint64 key, uint64 value) {
    //PhaseTimer tm(setEntryTimer, "setEntry");
    assert(mapHandler);
    //PhaseTimer tm(mapTimer, "setEntry");
    StlHashMap<uint64, uint64>* theMap = (StlHashMap<uint64, uint64>*) mapHandler;
    (*theMap)[key] =  value;
}
void removeMapEntry(void* mapHandler, uint64 key) {
    //PhaseTimer tm(mapTimer, "Entry");
    assert(mapHandler);
    StlHashMap<uint64, uint64>* theMap = (StlHashMap<uint64, uint64>*) mapHandler;
    theMap->erase(key);
}
/************************************************************************/
uint64 getVectorEntry(void* vectorHandler, uint64 key) { 
    //PhaseTimer tm(getEntryTimer, "getEntry");
    assert(vectorHandler);
    StlVector<uint64>* theVector = (StlVector<uint64>*) vectorHandler;
    if (key >= (uint64) theVector->size()) return ILLEGAL_VALUE;
    return (*theVector)[(size_t)key];
}
//static Timer* setEntryTimer = NULL;
void setVectorEntry(void* vectorHandler, uint64 key, uint64 value) {
    //PhaseTimer tm(setEntryTimer, "setEntry");
    assert(vectorHandler);
    StlVector<uint64>* theVector = (StlVector<uint64>*) vectorHandler;
    assert((size_t)key <  theVector->size()); // increase ESTIMATED_LIR_SIZE_PER_HIR 
                                                              // or ESTIMATED_HIR_SIZE_PER_BYTECODE 
    (*theVector)[(size_t)key] =  value;
}
void removeVectorEntry(void* vectorHandler, uint64 key) {
    //PhaseTimer tm(mapTimer, "Entry");
    assert(vectorHandler);
    StlVector<uint64>* theVector = (StlVector<uint64>*) vectorHandler;
    (*theVector)[(size_t)key] = (uint64)ILLEGAL_VALUE;
}

/************************************************************************/
//void addMapHandler(void* mapHandler, const char* name) {
//    assert(handlMap->find(name) == handlMap->end());
//    (*handlMap)[name] =  mapHandler;
//}
//
//void removeMapHandler(const char* name) {  
//    assert(handlMap->find(name) != handlMap->end());
//    handlMap->erase(name); 
//}
//
//void* getMapHandler(const char* name) { 
//    assert(handlMap->find(name) != handlMap->end());
//    return handlMap->find(name)->second; 
//}
//bool isHandlerExist(const char* name) {
//    return (handlMap->find(name) != handlMap->end());
//}

/************************************************************************/


}
