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

#ifndef _EMINTERFACE_H_
#define _EMINTERFACE_H_

#include "VMInterface.h"

namespace Jitrino {

enum ProfileType {
    ProfileType_Invalid = 0,
    ProfileType_EntryBackedge = 1,
    ProfileType_Edge = 2,
    ProfileType_Value = 3
};

enum JITProfilingRole{
    JITProfilingRole_GEN = 1,
    JITProfilingRole_USE = 2
};

//M1 implementation of profiling interfaces
class MethodProfile;
class MemoryManager;
class EntryBackedgeMethodProfile;
class EdgeMethodProfile;
class ValueMethodProfile;

typedef void PC_Callback_Fn(Method_Profile_Handle);

class ProfilingInterface {
public:
    virtual ~ProfilingInterface(){};

    virtual MethodProfile* getMethodProfile(MemoryManager& mm, ProfileType type, MethodDesc& md, JITProfilingRole role=JITProfilingRole_USE) const = 0;
    // Returns EM method profile handle. This method is needed when we need to update method profile
    // at run-time i.e. when there is no any memory managers available.
    virtual Method_Profile_Handle getMethodProfileHandle(ProfileType type, MethodDesc& md) const = 0;
    virtual EM_ProfileAccessInterface* getEMProfileAccessInterface() const = 0;

    virtual bool hasMethodProfile(ProfileType type, MethodDesc& md, JITProfilingRole role=JITProfilingRole_USE) const = 0;
    virtual bool enableProfiling(PC_Handle pc, JITProfilingRole role) = 0;
    virtual bool isProfilingEnabled(ProfileType pcType, JITProfilingRole jitRole) const = 0;


    virtual uint32 getProfileMethodCount(MethodDesc& md, JITProfilingRole role = JITProfilingRole_USE) const = 0;

    virtual EntryBackedgeMethodProfile* createEBMethodProfile(MemoryManager& mm, MethodDesc& md) =0;
    virtual bool isEBProfilerInSyncMode() const = 0;
    virtual PC_Callback_Fn* getEBProfilerSyncModeCallback() const = 0;


    virtual EdgeMethodProfile* createEdgeMethodProfile(MemoryManager& mm, MethodDesc& md, uint32 numEdgeCounters, uint32* counterKeys, uint32 checkSum) =0;


    virtual uint32 getMethodEntryThreshold() const = 0;
    virtual uint32 getBackedgeThreshold() const = 0;

    virtual EntryBackedgeMethodProfile* getEBMethodProfile(MemoryManager& mm, MethodDesc& md, JITProfilingRole role=JITProfilingRole_USE) const {
        return (EntryBackedgeMethodProfile*)getMethodProfile(mm, ProfileType_EntryBackedge, md, role);
    }

    virtual EdgeMethodProfile* getEdgeMethodProfile(MemoryManager& mm, MethodDesc& md, JITProfilingRole role=JITProfilingRole_USE) const {
        return (EdgeMethodProfile*)getMethodProfile(mm, ProfileType_Edge, md, role);    
    }
    
    
    // value profiler
    virtual ValueMethodProfile* createValueMethodProfile (MemoryManager& mm, MethodDesc& md, uint32 numKeys, uint32* Keys) = 0;
    
    virtual ValueMethodProfile* getValueMethodProfile(MemoryManager& mm, MethodDesc& md, JITProfilingRole role=JITProfilingRole_USE) const {
        return (ValueMethodProfile*)getMethodProfile(mm, ProfileType_Value, md, role);    
    }
};

class MethodProfile {
public:
    MethodProfile(Method_Profile_Handle _handle, ProfileType _type, MethodDesc& _md)
        : handle(_handle), type(_type), md(_md){}
        virtual ~MethodProfile(){};
        Method_Profile_Handle getHandle() const { return handle;} 
        MethodDesc& getMethod() const {return md;}
        ProfileType getProfileType() const {return type;}
private:
    Method_Profile_Handle handle;
    ProfileType type;
    MethodDesc& md;
};

class EntryBackedgeMethodProfile : public MethodProfile {
public:
    EntryBackedgeMethodProfile (Method_Profile_Handle handle, MethodDesc& md): MethodProfile(handle, ProfileType_EntryBackedge, md){}

    virtual uint32 getEntryExecCount() const = 0;
    virtual uint32 getBackedgeExecCount() const = 0;
    virtual uint32* getEntryCounter() const = 0;
    virtual uint32* getBackedgeCounter() const = 0;
};

class EdgeMethodProfile : public MethodProfile {
public:
    EdgeMethodProfile (Method_Profile_Handle handle, MethodDesc& md): MethodProfile(handle, ProfileType_Edge, md){}

    virtual uint32  getNumCounters() const = 0;
    virtual uint32  getCheckSum() const = 0;
    virtual uint32* getEntryCounter() const = 0;
    virtual uint32* getCounter(uint32 key) const = 0;
};

class ValueMethodProfile: public MethodProfile {
public:
    ValueMethodProfile (Method_Profile_Handle handle, MethodDesc& md) : MethodProfile(handle, ProfileType_Value, md){}

    virtual POINTER_SIZE_INT getTopValue(uint32 instructionKey) const = 0;
    virtual void dumpValues(std::ostream& os) const = 0;
};


};//namespace

#endif //_EMINTERFACE_H_
