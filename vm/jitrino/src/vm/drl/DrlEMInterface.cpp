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
/* COPYRIGHT_NOTICE */

/**
* @author Mikhail Y. Fursov
* @version $Revision$
*/

#include "open/em_profile_access.h"
#include "DrlEMInterface.h"
#include "DrlVMInterface.h"
#include "JITInstanceContext.h"

#include <assert.h>

namespace Jitrino {

PC_Handle DrlProfilingInterface::getPCHandle(ProfileType type) const {
    switch (type) {
        case ProfileType_EntryBackedge:
            return ebPCHandle;
        case ProfileType_Edge:
            return edgePCHandle;
        case ProfileType_Value:
            return valuePCHandle;
        default:
            assert(0);
    }
    return NULL;
}

MethodProfile* DrlProfilingInterface::getMethodProfile(MemoryManager& mm, ProfileType type, MethodDesc& md, JITProfilingRole role) const {
    
    Method_Profile_Handle mpHandle = profileAccessInterface->get_method_profile(emHandle, getPCHandle(type), ((DrlVMMethodDesc&)md).getDrlVMMethod());
    if (mpHandle==0) {
        return NULL;
    }
    MethodProfile* p = NULL;
    if (type == ProfileType_Edge) {
        p = new (mm) DrlEdgeMethodProfile(mpHandle, md, profileAccessInterface);
    } else if (type == ProfileType_Value) {
        p = new (mm) DrlValueMethodProfile(mpHandle, md, profileAccessInterface);
    } else {
        uint32* eCounter = (uint32*)profileAccessInterface->eb_profiler_get_entry_counter_addr(mpHandle);
        uint32* bCounter = (uint32*)profileAccessInterface->eb_profiler_get_backedge_counter_addr(mpHandle);
        p = new (mm) DrlEntryBackedgeMethodProfile(mpHandle, md, eCounter, bCounter);
    }
    return p;
}

Method_Profile_Handle DrlProfilingInterface::getMethodProfileHandle(ProfileType type, MethodDesc& md) const {
    return profileAccessInterface->get_method_profile(emHandle, getPCHandle(type), ((DrlVMMethodDesc&)md).getDrlVMMethod());
}


bool DrlProfilingInterface::hasMethodProfile(ProfileType type, MethodDesc& md, JITProfilingRole role) const {

    PC_Handle pcHandle = getPCHandle(type);

    if (pcHandle == NULL) {
        return false;
    }
    if (jitRole != role) {
        return false;
    }
    if (profileAccessInterface != NULL) {
        Method_Profile_Handle mpHandle = profileAccessInterface->get_method_profile(emHandle, pcHandle, ((DrlVMMethodDesc&)md).getDrlVMMethod());
        return mpHandle!=0;
    }
    return false;
}

uint32 DrlProfilingInterface::getProfileMethodCount(MethodDesc& md, JITProfilingRole role) const {
    assert(jitRole == role);
    PC_Handle pcHandle = getPCHandle(ProfileType_Edge);
    ProfileType pcType = ProfileType_Edge;
    if (pcHandle == NULL) {
        pcHandle = getPCHandle(ProfileType_EntryBackedge);
        pcType = ProfileType_EntryBackedge;
    }
    assert (pcHandle != NULL);
    Method_Handle methodHandle = ((DrlVMMethodDesc&)md).getDrlVMMethod();
    Method_Profile_Handle mph = profileAccessInterface->get_method_profile(emHandle, pcHandle, methodHandle);
    if (mph == NULL) {
        return 0;
    }
    uint32* counterAddr = NULL;
    if (pcType == ProfileType_Edge) { 
        counterAddr = (uint32*)profileAccessInterface->edge_profiler_get_entry_counter_addr(mph);
    } else {
        counterAddr = (uint32*)profileAccessInterface->eb_profiler_get_entry_counter_addr(mph);
    }
    return *counterAddr;
}

bool DrlProfilingInterface::enableProfiling(PC_Handle pc, JITProfilingRole role) {
    EM_PCTYPE _pcType =  profileAccessInterface->get_pc_type(emHandle, pc);
    if (_pcType != EM_PCTYPE_EDGE && _pcType != EM_PCTYPE_ENTRY_BACKEDGE && _pcType != EM_PCTYPE_VALUE) {
        return false;
    }
    JITInstanceContext* jitMode = JITInstanceContext::getContextForJIT(jitHandle);
    if (jitMode->isJet()) {
        if (role == JITProfilingRole_GEN) {
            profilingEnabled = _pcType == EM_PCTYPE_ENTRY_BACKEDGE; 
        } else {
            profilingEnabled = false;
        }
    } else { //OPT
        profilingEnabled = true;
    }
    if (profilingEnabled) {
        jitRole = role;
        switch(_pcType)
        {
        case EM_PCTYPE_EDGE:
            edgePCHandle = pc;
            break;
        case EM_PCTYPE_ENTRY_BACKEDGE:
            ebPCHandle = pc;
            break;
        case EM_PCTYPE_VALUE:
            valuePCHandle = pc;
            break;
        default:
            assert(0);
            return false;
        }
    }
    return profilingEnabled;
}

bool DrlProfilingInterface::isProfilingEnabled(ProfileType pcType, JITProfilingRole role) const {
    if(!profilingEnabled || (jitRole != role) || (getPCHandle(pcType) == NULL)){
        return false;
    }
    return true;
}

EntryBackedgeMethodProfile* DrlProfilingInterface::createEBMethodProfile(MemoryManager& mm, MethodDesc& md) {
    assert(isProfilingEnabled(ProfileType_EntryBackedge, JITProfilingRole_GEN));
    PC_Handle pcHandle = getPCHandle(ProfileType_EntryBackedge);
    Method_Profile_Handle mpHandle = profileAccessInterface->eb_profiler_create_profile(pcHandle, ((DrlVMMethodDesc&)md).getDrlVMMethod());
    assert(mpHandle!=0);
    uint32* eCounter = (uint32*)profileAccessInterface->eb_profiler_get_entry_counter_addr(mpHandle);
    uint32* bCounter = (uint32*)profileAccessInterface->eb_profiler_get_backedge_counter_addr(mpHandle);

    DrlEntryBackedgeMethodProfile* p = new (mm) DrlEntryBackedgeMethodProfile(mpHandle, md, eCounter, bCounter);
    return p;
}


EdgeMethodProfile* DrlProfilingInterface::createEdgeMethodProfile( MemoryManager& mm,
                                                                  MethodDesc& md,
                                                                  uint32 numCounters,
                                                                  uint32* counterKeys,
                                                                  uint32 checkSum )
{
    assert(isProfilingEnabled(ProfileType_Edge, JITProfilingRole_GEN));
    PC_Handle pcHandle = getPCHandle(ProfileType_Edge);
    Method_Profile_Handle mpHandle =  profileAccessInterface->edge_profiler_create_profile( 
        pcHandle, ((DrlVMMethodDesc&)md).getDrlVMMethod(), numCounters, counterKeys, checkSum);
    assert( mpHandle != NULL );

    DrlEdgeMethodProfile* p = new (mm) DrlEdgeMethodProfile(mpHandle, md, profileAccessInterface);
    return p;
}

ValueMethodProfile* DrlProfilingInterface::createValueMethodProfile(MemoryManager& mm,
                                                                    MethodDesc& md,
                                                                    uint32 numKeys,
                                                                    uint32* Keys)
{
    assert(isProfilingEnabled(ProfileType_Value, JITProfilingRole_GEN));
    PC_Handle pcHandle = getPCHandle(ProfileType_Value);
    Method_Profile_Handle mpHandle =  profileAccessInterface->value_profiler_create_profile( 
        pcHandle, ((DrlVMMethodDesc&)md).getDrlVMMethod(), numKeys, Keys);
    assert(mpHandle != NULL);

    DrlValueMethodProfile* p = new (mm) DrlValueMethodProfile(mpHandle, md, profileAccessInterface);
    return p;
}


uint32 DrlProfilingInterface::getMethodEntryThreshold() const {
    PC_Handle pcHandle = getPCHandle(ProfileType_Edge);
    if (pcHandle != NULL) {
        return profileAccessInterface->edge_profiler_get_entry_threshold(pcHandle);
    } else if ((pcHandle = getPCHandle(ProfileType_EntryBackedge)) != NULL) {
        return profileAccessInterface->eb_profiler_get_entry_threshold(pcHandle);
    }
    assert(0);
    return 0;
}

uint32 DrlProfilingInterface::getBackedgeThreshold() const {
    PC_Handle pcHandle = getPCHandle(ProfileType_Edge);
    if (pcHandle != NULL) {
        return profileAccessInterface->edge_profiler_get_backedge_threshold(pcHandle);
    } else if ((pcHandle = getPCHandle(ProfileType_EntryBackedge)) != NULL) {
        return profileAccessInterface->eb_profiler_get_backedge_threshold(pcHandle);
    }
    assert(0);
    return 0;
}

bool DrlProfilingInterface::isEBProfilerInSyncMode() const {
    PC_Handle pcHandle = getPCHandle(ProfileType_EntryBackedge);
    assert(pcHandle!=NULL);
    return profileAccessInterface->eb_profiler_is_in_sync_mode(pcHandle)!=0;
}

PC_Callback_Fn* DrlProfilingInterface::getEBProfilerSyncModeCallback() const {
    assert(profileAccessInterface->eb_profiler_sync_mode_callback!=NULL);
    return (PC_Callback_Fn*)profileAccessInterface->eb_profiler_sync_mode_callback;
}



DrlEdgeMethodProfile::DrlEdgeMethodProfile (Method_Profile_Handle handle, MethodDesc& md, 
                                            EM_ProfileAccessInterface* _profileAccessInterface) 
: EdgeMethodProfile(handle, md),  profileAccessInterface(_profileAccessInterface)
{
}


uint32  DrlEdgeMethodProfile::getNumCounters() const {
    return profileAccessInterface->edge_profiler_get_num_counters(getHandle());
}

uint32  DrlEdgeMethodProfile::getCheckSum() const {
    return profileAccessInterface->edge_profiler_get_checksum(getHandle());
}

uint32* DrlEdgeMethodProfile::getEntryCounter() const {
    return (uint32*)profileAccessInterface->edge_profiler_get_entry_counter_addr(getHandle());
}

uint32* DrlEdgeMethodProfile::getCounter(uint32 key) const  {
    uint32* counter = (uint32*)profileAccessInterface->edge_profiler_get_counter_addr(getHandle(), key);
    return counter;
}

// Value profile
DrlValueMethodProfile::DrlValueMethodProfile(Method_Profile_Handle handle, MethodDesc& md,  EM_ProfileAccessInterface* _profileAccessInterface)
: ValueMethodProfile(handle, md),  profileAccessInterface(_profileAccessInterface) {
}

POINTER_SIZE_INT DrlValueMethodProfile::getTopValue(uint32 instructionKey) const {
    return profileAccessInterface->value_profiler_get_top_value(getHandle(), instructionKey);
}

void DrlValueMethodProfile::dumpValues(std::ostream& os) const {
    profileAccessInterface->value_profiler_dump_values(getHandle(), os);
}


} //namespace



