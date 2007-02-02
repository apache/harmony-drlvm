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
* @author Yuri Kashnikov
* @version $Revision$
*/
/*
        The idea of advanced Top-N-Value (with steady and clear parts, and clear interval)
        from <"Value profilling and optimization", B.Calder, P.Feller, Journal of Instruction-Level Parallelism, 1999>

*/
#ifndef _VALUE_PROFILE_COLLECTOR_H_
#define _VALUE_PROFILE_COLLECTOR_H_

#include "DrlProfileCollectionFramework.h"
#include "open/vm_util.h"

#include <map>

#define TNV_DEFAULT_CLEAR_VALUE 0

class ValueMethodProfile;
class VPInstructionProfileData;

struct Simple_TNV_Table
{
    POINTER_SIZE_INT value;
    uint32 frequency;
};

class ValueProfileCollector : public ProfileCollector {
public:
    enum algotypes {TNV_DIVIDED, TNV_FIRST_N};
    uint32 TNV_steady_size;
    uint32 TNV_clear_size;
    uint32 clear_interval;
    algotypes TNV_algo_type;
public:
    ValueProfileCollector(EM_PC_Interface* em, const std::string& name, JIT_Handle genJit,
                                                uint32 _TNV_steady_size, uint32 _TNV_clear_size,
                                                uint32 _clear_interval, algotypes _TNV_algo_type);
    virtual TbsEMClient* getTbsEmClient() const {return (NULL);}
    virtual ~ValueProfileCollector();
    MethodProfile* getMethodProfile(Method_Handle mh) const ;
    ValueMethodProfile* createProfile(Method_Handle mh, uint32 numkeys, uint32 keys[]);
    
    int32  search_in_tnv_table (struct Simple_TNV_Table * TNV_where, POINTER_SIZE_INT value_to_search, uint32 number_of_objects);
    int32  min_in_tnv (struct Simple_TNV_Table * TNV_where, uint32 number_of_objects);
    void insert_into_tnv_table (struct Simple_TNV_Table* TNV_table, struct Simple_TNV_Table* TNV_clear_part, POINTER_SIZE_INT value_to_insert, uint32 times_met);
    POINTER_SIZE_INT find_max(struct Simple_TNV_Table* TNV_where);
    void simple_tnv_clear (struct Simple_TNV_Table* TNV_where);

private:
    std::string catName;
    bool   loggingEnabled;
    typedef std::map<Method_Handle, ValueMethodProfile*> ValueProfilesMap;
    ValueProfilesMap profilesByMethod;
    hymutex_t profilesLock;
};

class VPInstructionProfileData
{
public:
    struct Simple_TNV_Table* TNV_Table;
    struct Simple_TNV_Table * TNV_clear_part;
public:
    VPInstructionProfileData() : last_value(TNV_DEFAULT_CLEAR_VALUE), num_times_profiled(0), profile_tick(0) {}
public:
    POINTER_SIZE_INT last_value;
    uint32 num_times_profiled;
    uint32 profile_tick;
};

class ValueMethodProfile : public MethodProfile {
public:
    std::map<uint32, VPInstructionProfileData*> ValueMap;
public:
    ValueMethodProfile(ValueProfileCollector* pc, Method_Handle mh);
    ~ValueMethodProfile();
    void lockProfile() {hymutex_lock(lock);}
    void unlockProfile() {hymutex_unlock(lock);}
    void dumpValues(std::ostream& os);
    void addNewValue(uint32 instructionKey, POINTER_SIZE_INT valueToAdd);
    POINTER_SIZE_INT getResult(uint32 instructionKey);
private:
    // unsynchronized method - must be called from synchronized ones
    void flushInstProfile(VPInstructionProfileData* instProfile);
    ValueProfileCollector* getVPC() const;

    hymutex_t lock;
};

POINTER_SIZE_INT value_profiler_get_top_value (Method_Profile_Handle mph, uint32 instructionKey);
void value_profiler_add_value (Method_Profile_Handle mph, uint32 instructionKey, POINTER_SIZE_INT valueToAdd);
Method_Profile_Handle value_profiler_create_profile(PC_Handle pch, Method_Handle mh, uint32 numkeys, uint32 keys[]);
void value_profiler_dump_values(Method_Profile_Handle mph, std::ostream& os);

#endif
