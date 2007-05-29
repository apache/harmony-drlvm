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

#ifndef _CG_SUPPORT_H_
#define _CG_SUPPORT_H_

#include "open/types.h"
#include "MemoryManager.h"
#include "VMInterface.h"

//FIXME 64-bit usage
#define ILLEGAL_VALUE 0xFFFFFFFF

#define ESTIMATED_LIR_SIZE_PER_HIR 0x3
#define ESTIMATED_HIR_SIZE_PER_BYTECODE 0x8

namespace Jitrino {

extern const char* bcOffset2LIRHandlerName;
extern const char* bcOffset2HIRHandlerName;
extern const char* lostBCOffsetHandlerName;

void initHandleMap(MemoryManager& mm, MethodDesc* meth);
void enumerateHandlMap(MethodDesc* meth);

//  Old interface to work with MapHandlers is deprecated since
//  it inefficient due to problems with reenterability on resolution

//void addMapHandler(void *mapHandler, const char* name);
//void removeMapHandler(const char* name);
//void* getMapHandler(const char* name);
//uint64 getHandlerSize(const char* name);
//bool isHandlerExist(const char* name);

void addContainerHandler(void* contHandler, const char* name, MethodDesc* meth);
void removeContainerHandler(const char* name, MethodDesc* meth);
void* getContainerHandler(const char* name, MethodDesc* meth);
bool isContainerHandlerExist(const char* name, MethodDesc* meth);

void incVectorHandlerSize(const char* name, MethodDesc* meth, size_t incSize);
uint64 getVectorSize(const char* name, MethodDesc* meth);

uint64 getMapHandlerSize(const char* name, MethodDesc* meth);

uint64 getMapEntry(void* mapHandler, uint64 key);
void setMapEntry(void* mapHandler, uint64 key, uint64 value);
void removeMapEntry(void* mapHandler, uint64 key);

uint64 getVectorEntry(void* vectorHandler, uint64 key);
void setVectorEntry(void* vectorHandler, uint64 key, uint64 value);
void removeVectorEntry(void* vectorHandler, uint64 key);

}

#endif
