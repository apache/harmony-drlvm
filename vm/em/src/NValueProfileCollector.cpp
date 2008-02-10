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
*
* The idea of advanced Top-N-Value (with steady and clear parts, and clear
* interval) from <"Value profiling and optimization", B.Calder, P.Feller,
* Journal of Instruction-Level Parallelism, 1999>
*
*/
#include "NValueProfileCollector.h"

#include <algorithm>
#include <assert.h>
#include "cxxlog.h"
#include <sstream>

#include "port_threadunsafe.h"
#include "port_atomic.h"

#define LOG_DOMAIN "em"

VPInstructionProfileData* TNVTableManager::createProfileData()
{
    VPInstructionProfileData* data = new VPInstructionProfileData();
    data->TNV_Table =  new (struct Simple_TNV_Table[steadySize]);
    for (uint32 i = 0; i < steadySize; i++) {
        (data->TNV_Table[i]).frequency = 0;
        (data->TNV_Table[i]).value = 0;
    }
    if (clearSize > 0) {
        data->TNV_clear_part = new (struct Simple_TNV_Table[clearSize]);
        for (uint32 i = 0; i < clearSize; i++) {
            (data->TNV_clear_part[i]).frequency = 0;
            (data->TNV_clear_part[i]).value = 0;
        }
    }
    return data;
}

int32 TNVTableManager::find(TableT* where, ValueT value_to_search, uint32 size)
{
    uint32 search_index;
    for (search_index = 0; search_index < size; search_index++){
        if (where[search_index].value == value_to_search)
            return (search_index);
    }
    return (-1);
}

void TNVTableManager::clearTopElements(TableT* where)
{
    uint32 temp_index;
    for (temp_index = 0; temp_index < clearSize; temp_index++) {
        where[temp_index].frequency = TNV_DEFAULT_CLEAR_VALUE;
    }
}

int32 TNVTableManager::findMinIdx(TableT* where, uint32 size)
{
    uint32 temp_index;
    uint32 temp_min_index = 0;
    uint32 temp_min = where[temp_min_index].frequency;
    for (temp_index = 0; temp_index < size; temp_index++){
        if (where[temp_index].frequency == TNV_DEFAULT_CLEAR_VALUE) {
            return (temp_index); 
        }
        if (where[temp_index].frequency < temp_min){
            temp_min = where[temp_index].frequency;
            temp_min_index = temp_index;
        }
    }
    return (temp_min_index);
}

TNVTableManager::ValueT TNVTableManager::findMax(TableT *where)
{
    ValueT max_value = 0;
    uint32 temp_index, temp_max_frequency = 0;
    for (temp_index = 0; temp_index < steadySize; temp_index++) {
        TableT *current_tbl = &(where[temp_index]);
        if (current_tbl->frequency > temp_max_frequency){
            temp_max_frequency = current_tbl->frequency;
            max_value = current_tbl->value;
        }
    }
    return (max_value);
}

void TNVTableManager::flushLastValueCounter(VPData *instProfile)
{
    POINTER_SIZE_INT last_value = instProfile->last_value;
    uint32* num_times_profiled = &(instProfile->num_times_profiled);
    struct Simple_TNV_Table* clear_part = instProfile->TNV_clear_part;
    struct Simple_TNV_Table* steady_part = instProfile->TNV_Table;

    insert(steady_part, clear_part, last_value, *num_times_profiled);

    *num_times_profiled = 0;
}

void TNVTableManager::dumpValues
    (VPInstructionProfileData* data, std::ostream& os)
{
    os << ", num_times_profiled: " << data->num_times_profiled 
        << ", profile_tick: " << data->profile_tick << std::endl;
    struct Simple_TNV_Table * TNV_steady_part = data->TNV_Table;
    if (TNV_steady_part != NULL) {
        uint32 size = steadySize;
        os << "= TNV_steady_part, size = " << size << std::endl;
        for (uint32 i = 0; i < size; i++) {
            os << "== Frequency: " << TNV_steady_part[i].frequency << " = Value: ";
            POINTER_SIZE_INT value = TNV_steady_part[i].value;
            if (value != 0) {
                os << class_get_name(vtable_get_class((VTable_Handle)value));
            } else {
                os << "NULL";
            }
            os << " ==" << std::endl;
        }
    }
    struct Simple_TNV_Table * TNV_clear_part = data->TNV_clear_part;
    if (TNV_clear_part != NULL) {
        uint32 size = clearSize;
        os << "= TNV_clear_part, size = " << size << std::endl;
        for (uint32 i = 0; i < size; i++) {
            os << "== " << TNV_clear_part[i].frequency << " = Value: ";
            POINTER_SIZE_INT value = TNV_clear_part[i].value;
            if (value != 0) {
                os << class_get_name(vtable_get_class((VTable_Handle)value));
            } else {
                os << "NULL";
            }
            os << " ==" << std::endl;
        }
    }
}
//------------------------------------------------------------------------------

void TNVTableFirstNManager::insert(TableT* where, TableT* clear_part,
        ValueT value_to_insert, uint32 times_met)
{
    uint32 insert_index = find(where, value_to_insert, steadySize);
    if ((insert_index != -1) &&
        (where[insert_index].frequency != TNV_DEFAULT_CLEAR_VALUE)){
        where[insert_index].frequency += times_met;
    } else {
        insert_index = findMinIdx(where, steadySize);
        if (times_met > where[insert_index].frequency){
            where[insert_index].value = value_to_insert;
            where[insert_index].frequency = times_met;
        }
    }
}

void TNVTableFirstNManager::addNewValue(ValueMethodProfile* methProfile,
            VPData* instProfile, ValueT curr_value)
{
    uint8* updating_ptr = methProfile->getUpdatingStatePtr();
    if (updateStrategy == UPDATE_FLAGGED_ALL) {
        // Checking a flag and modifying it atomically must be faster than
        // locking because it skips simultaneous updates. Faster but sacrifices
        // profile precision.
        if (port_atomic_cas8(updating_ptr, 1, 0) != 0) {
            return;
        }
    }
    UNSAFE_REGION_START
    ValueT* last_value = &(instProfile->last_value);
    uint32* num_times_profiled = &(instProfile->num_times_profiled);
    if (curr_value == *last_value){
        // We increment the counter safely only with UPDATE_FLAGGED_ALL
        (*num_times_profiled)++;
    } else {
        if (updateStrategy == UPDATE_LOCKED) {
            methProfile->lockProfile();
        }else if (updateStrategy == UPDATE_FLAGGED_INSERT) {
            if (port_atomic_cas8(updating_ptr, 1, 0) != 0) {
                return;
            }
        }
        struct Simple_TNV_Table* clear_part = instProfile->TNV_clear_part;
        struct Simple_TNV_Table* steady_part = instProfile->TNV_Table;
        flushLastValueCounter(instProfile);
        *num_times_profiled = 1;
        insert(steady_part, clear_part, curr_value, *num_times_profiled);
        *last_value = curr_value;
        if (updateStrategy == UPDATE_LOCKED) {
            methProfile->unlockProfile();
        }else if (updateStrategy == UPDATE_FLAGGED_INSERT) {
            *updating_ptr = 0;
        }
    }
    UNSAFE_REGION_END
    if (updateStrategy == UPDATE_FLAGGED_ALL) {
        *updating_ptr = 0;
    }
}
//------------------------------------------------------------------------------

void TNVTableDividedManager::insert(TableT* where, TableT* clear_part,
        ValueT value_to_insert, uint32 times_met)
{
    uint32 insert_index = find(where, value_to_insert, steadySize);
    if ((insert_index != -1) &&
        (where[insert_index].frequency != TNV_DEFAULT_CLEAR_VALUE)){
        where[insert_index].frequency += times_met;
    }else{
        ValueT temp_min_value;
        uint32 temp_min_index, temp_min_freq;
        insert_index = find(clear_part, value_to_insert, clearSize);
        if (insert_index != -1){
            clear_part[insert_index].frequency = clear_part[insert_index].frequency + times_met;
            temp_min_index = findMinIdx(where, steadySize);
            if (clear_part[insert_index].frequency > where[temp_min_index].frequency){
                temp_min_value = where[temp_min_index].value;
                temp_min_freq = where[temp_min_index].frequency;
                where[temp_min_index].value = clear_part[insert_index].value;
                where[temp_min_index].frequency = clear_part[insert_index].frequency;
                clear_part[insert_index].frequency = TNV_DEFAULT_CLEAR_VALUE;
                temp_min_index = findMinIdx(clear_part, clearSize);
                if (temp_min_freq > clear_part[temp_min_index].frequency){
                    clear_part[temp_min_index].value = temp_min_value;
                    clear_part[temp_min_index].frequency = temp_min_freq;
                }
            }
        } else {
            temp_min_index = findMinIdx(where, steadySize);
            if (times_met > where[temp_min_index].frequency)
            {
                temp_min_value = where[temp_min_index].value;
                temp_min_freq = where[temp_min_index].frequency;
                where[temp_min_index].value = value_to_insert;
                where[temp_min_index].frequency = times_met;
                temp_min_index = findMinIdx(clear_part, clearSize);
                if (temp_min_freq > clear_part[temp_min_index].frequency)
                {
                    clear_part[temp_min_index].value = temp_min_value;
                    clear_part[temp_min_index].frequency = temp_min_freq;
                }
            } else {
                temp_min_index = findMinIdx(clear_part, clearSize);
                if (times_met > clear_part[temp_min_index].frequency){
                    clear_part[temp_min_index].value = value_to_insert;
                    clear_part[temp_min_index].frequency = times_met;
                }
            }
        }
    }
}

void TNVTableDividedManager::addNewValue(ValueMethodProfile* methProfile,
            VPData* instProfile, ValueT curr_value)
{
    methProfile->lockProfile();
    struct Simple_TNV_Table* clear_part = instProfile->TNV_clear_part;
    uint32* profile_tick = &(instProfile->profile_tick);
    if (*profile_tick == clearInterval){
        *profile_tick = 0;
        clearTopElements(clear_part);
    }
    (*profile_tick)++;

    ValueT* last_value = &(instProfile->last_value);
    uint32* num_times_profiled = &(instProfile->num_times_profiled);
    if (curr_value == *last_value){
        (*num_times_profiled)++;
    } else {
        flushLastValueCounter(instProfile);
        *num_times_profiled = 1;
        struct Simple_TNV_Table* steady_part = instProfile->TNV_Table;
        insert(steady_part, clear_part, curr_value, *num_times_profiled);
        *last_value = curr_value;
    }
    methProfile->unlockProfile();
}
//------------------------------------------------------------------------------

ValueMethodProfile* ValueProfileCollector::createProfile
    (Method_Handle mh, uint32 numkeys, uint32 keys[])
{
    hymutex_lock(&profilesLock);
    ValueMethodProfile* profile = new ValueMethodProfile(this, mh);
    // Allocate space for value maps
    for (uint32 index = 0; index < numkeys; index++){
        VPInstructionProfileData* profileData =
            getTnvMgr()->createProfileData();
        uint32 key = keys[index];
        (profile->ValueMap)[key] = profileData;
    }
    assert(profilesByMethod.find(mh) == profilesByMethod.end());
    profilesByMethod[mh] = profile;
    hymutex_unlock(&profilesLock);
    return profile;
}

ValueProfileCollector::ValueProfileCollector(EM_PC_Interface* em, const std::string& name, JIT_Handle genJit, 
                                             uint32 _TNV_steady_size, uint32 _TNV_clear_size,
                                             uint32 _clear_interval, algotypes _TNV_algo_type,
                                             ProfileUpdateStrategy update_strategy)
                                           : ProfileCollector(em, name, EM_PCTYPE_VALUE, genJit),
                                             updateStrategy(update_strategy)
{
    hymutex_create(&profilesLock, TM_MUTEX_NESTED);
    if (_TNV_algo_type == TNV_DIVIDED) {
        tnvTableManager = new TNVTableDividedManager
            (_TNV_steady_size, _TNV_clear_size, _clear_interval, update_strategy);
    }else if (_TNV_algo_type == TNV_FIRST_N) {
        tnvTableManager = new TNVTableFirstNManager
            (_TNV_steady_size, _TNV_clear_size, _clear_interval, update_strategy);
    }
    catName = std::string(LOG_DOMAIN) + ".profiler." + name;
    loggingEnabled =  is_info_enabled(LOG_DOMAIN) ||  is_info_enabled(catName.c_str());
    if (loggingEnabled) {
        std::ostringstream msg;
        msg<< "EM: value profiler intialized: "<<name;
        INFO2(catName.c_str(), msg.str().c_str());
    }
}


ValueProfileCollector::~ValueProfileCollector()
{
    ValueProfilesMap::iterator it;
    for( it = profilesByMethod.begin(); it != profilesByMethod.end(); it++ ){
        ValueMethodProfile* profile = it->second;
        delete profile;
    }
    delete tnvTableManager;
    hymutex_destroy(&profilesLock);
}

MethodProfile* ValueProfileCollector::getMethodProfile(Method_Handle mh) const
{
    MethodProfile* res = NULL;
    hymutex_lock(&profilesLock);
    ValueProfilesMap::const_iterator it = profilesByMethod.find(mh);
    if (it != profilesByMethod.end()) {
        res =  it->second;
    }
    hymutex_unlock(&profilesLock);
    return res;
}
//------------------------------------------------------------------------------

ValueMethodProfile::ValueMethodProfile(ValueProfileCollector* pc, Method_Handle mh)
    : MethodProfile(pc, mh), updatingState(0)
{
    hymutex_create(&lock, TM_MUTEX_DEFAULT);
}

ValueMethodProfile::~ValueMethodProfile()
{
    hymutex_destroy(&lock);
}

void ValueMethodProfile::addNewValue
    (uint32 instructionKey, POINTER_SIZE_INT valueToAdd)
{
    VPDataMap::const_iterator it =  ValueMap.find(instructionKey);
    assert(it != ValueMap.end());

    getVPC()->getTnvMgr()->addNewValue(this, it->second, valueToAdd);
}

POINTER_SIZE_INT ValueMethodProfile::getResult(uint32 instructionKey)
{
    lockProfile();
    VPDataMap::const_iterator it =  ValueMap.find(instructionKey);
    if (it == ValueMap.end()) {
        unlockProfile();
        return 0;
    }
    VPInstructionProfileData* _temp_vp = it->second;
    assert(_temp_vp);
    if (_temp_vp == NULL) {
        unlockProfile();
        return 0;
    }
    getVPC()->getTnvMgr()->flushLastValueCounter(_temp_vp);
    POINTER_SIZE_INT result = getVPC()->getTnvMgr()->findMax(_temp_vp->TNV_Table);
    unlockProfile();
    return result; 
}

void ValueMethodProfile::dumpValues(std::ostream& os)
{
    VPDataMap::const_iterator mapIter;
    assert(pc->type == EM_PCTYPE_VALUE);
    lockProfile();
    os << "===== Value profile dump, " << ValueMap.size() << " element(s) ===" << std::endl;
    for (mapIter = ValueMap.begin(); mapIter != ValueMap.end(); mapIter++) {
        os << "=== Instruction key: " << mapIter->first;
        VPInstructionProfileData* _temp_vp = mapIter->second;
        TNVTableManager* tnvMgr = getVPC()->getTnvMgr();
        tnvMgr->flushLastValueCounter(_temp_vp);
        tnvMgr->dumpValues(_temp_vp, os);
    }
    unlockProfile();
    os << "====== End of dump ======================" << std::endl;
}

ValueProfileCollector* ValueMethodProfile::getVPC() const {
    assert(pc->type == EM_PCTYPE_VALUE);
    return ((ValueProfileCollector*)pc);
}
//------------------------------------------------------------------------------

POINTER_SIZE_INT value_profiler_get_top_value(Method_Profile_Handle mph, uint32 instructionKey)
{
    assert(mph != NULL);
    MethodProfile* mp = (MethodProfile*)mph;
    assert(mp->pc->type == EM_PCTYPE_VALUE);
    ValueMethodProfile* vmp = (ValueMethodProfile*)mp;
    return vmp->getResult(instructionKey);
}

void value_profiler_add_value(Method_Profile_Handle mph, uint32 instructionKey, POINTER_SIZE_INT valueToAdd)
{
    assert(mph != NULL);
    MethodProfile* mp = (MethodProfile*)mph;
    assert(mp->pc->type == EM_PCTYPE_VALUE);
    ValueMethodProfile* vmp = (ValueMethodProfile*)mp;
    return vmp->addNewValue(instructionKey, valueToAdd);
}

void value_profiler_dump_values(Method_Profile_Handle mph, std::ostream& os)
{
    assert(mph != NULL);
    MethodProfile* mp = (MethodProfile*)mph;
    assert(mp->pc->type == EM_PCTYPE_VALUE);
    ValueMethodProfile* vmp = (ValueMethodProfile*)mp;
    vmp->dumpValues(os);
}

Method_Profile_Handle value_profiler_create_profile(PC_Handle pch, Method_Handle mh, uint32 numkeys, uint32 keys[])
{
    assert(pch!=NULL);
    ProfileCollector* pc = (ProfileCollector*)pch;
    assert(pc->type == EM_PCTYPE_VALUE);
    ValueMethodProfile* profile = ((ValueProfileCollector*)pc)->createProfile(mh, numkeys, keys);
    return (Method_Profile_Handle)profile;
}
