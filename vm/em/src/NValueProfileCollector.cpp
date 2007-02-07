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
        from <"Value profiling and optimization", B.Calder, P.Feller, Journal of Instruction-Level Parallelism, 1999>

*/
#include "NValueProfileCollector.h"

#include <algorithm>
#include <assert.h>
#include "cxxlog.h"
#include <sstream>

#define LOG_DOMAIN "em"

void ValueProfileCollector::simple_tnv_clear (struct Simple_TNV_Table * TNV_where)
{
    uint32 temp_index;
    for (temp_index = 0; temp_index < TNV_clear_size; temp_index++)
        TNV_where[temp_index].frequency = TNV_DEFAULT_CLEAR_VALUE; 
}

int32 ValueProfileCollector::min_in_tnv (struct Simple_TNV_Table * TNV_where, uint32 number_of_objects)
{
    uint32 temp_index;
    uint32 temp_min_index = 0;
    uint32 temp_min = TNV_where[temp_min_index].frequency;
    for (temp_index = 0; temp_index < number_of_objects; temp_index++){
        if (TNV_where[temp_index].frequency == TNV_DEFAULT_CLEAR_VALUE)
            return (temp_index); 
        if (TNV_where[temp_index].frequency < temp_min){
            temp_min = TNV_where[temp_index].frequency;
            temp_min_index = temp_index;
        }
    }
    return (temp_min_index);    
}

int32 ValueProfileCollector::search_in_tnv_table (struct Simple_TNV_Table * TNV_where, POINTER_SIZE_INT value_to_search, uint32 number_of_objects)
{
    uint32 search_index;
    for (search_index = 0; search_index < number_of_objects; search_index++){
        if (TNV_where[search_index].value == value_to_search)
            return (search_index);
    }
    return (-1);
}

void ValueProfileCollector::insert_into_tnv_table (struct Simple_TNV_Table* TNV_table, struct Simple_TNV_Table* TNV_clear_part, POINTER_SIZE_INT value_to_insert, uint32 times_met)
{
    uint32 insert_index, temp_min_index;
    POINTER_SIZE_INT temp_min_value;
    uint32 temp_min_freq;
    insert_index = search_in_tnv_table(TNV_table, value_to_insert, TNV_steady_size);
    if ((insert_index != -1)  && (TNV_table[insert_index].frequency != TNV_DEFAULT_CLEAR_VALUE)){
        TNV_table[insert_index].frequency += times_met;
    } else if ((TNV_algo_type == TNV_FIRST_N) || (TNV_algo_type != TNV_DIVIDED)){
        insert_index = min_in_tnv(TNV_table, TNV_steady_size);
        if (times_met > TNV_table[insert_index].frequency){
            TNV_table[insert_index].value = value_to_insert;
            TNV_table[insert_index].frequency = times_met;
        }
    } else if (TNV_algo_type == TNV_DIVIDED) {
        insert_index = search_in_tnv_table(TNV_clear_part, value_to_insert, TNV_clear_size);
        if (insert_index != -1){
            TNV_clear_part[insert_index].frequency = TNV_clear_part[insert_index].frequency + times_met;
            temp_min_index = min_in_tnv(TNV_table, TNV_steady_size);
            if (TNV_clear_part[insert_index].frequency > TNV_table[temp_min_index].frequency){
                temp_min_value = TNV_table[temp_min_index].value;
                temp_min_freq = TNV_table[temp_min_index].frequency;
                TNV_table[temp_min_index].value = TNV_clear_part[insert_index].value;
                TNV_table[temp_min_index].frequency = TNV_clear_part[insert_index].frequency;
                TNV_clear_part[insert_index].frequency = TNV_DEFAULT_CLEAR_VALUE;
                temp_min_index = min_in_tnv(TNV_clear_part, TNV_clear_size);
                if (temp_min_freq > TNV_clear_part[temp_min_index].frequency){
                    TNV_clear_part[temp_min_index].value = temp_min_value;
                    TNV_clear_part[temp_min_index].frequency = temp_min_freq;
                }
            }
        }
        else {
            temp_min_index = min_in_tnv(TNV_table, TNV_steady_size);
            if (times_met > TNV_table[temp_min_index].frequency)
            {
                temp_min_value = TNV_table[temp_min_index].value;
                temp_min_freq = TNV_table[temp_min_index].frequency;
                TNV_table[temp_min_index].value = value_to_insert;
                TNV_table[temp_min_index].frequency = times_met;
                temp_min_index = min_in_tnv(TNV_clear_part, TNV_clear_size);
                if (temp_min_freq > TNV_clear_part[temp_min_index].frequency)
                {
                    TNV_clear_part[temp_min_index].value = temp_min_value;
                    TNV_clear_part[temp_min_index].frequency = temp_min_freq;
                }
            }
            else {
                temp_min_index = min_in_tnv(TNV_clear_part, TNV_clear_size);
                if (times_met > TNV_clear_part[temp_min_index].frequency){
                    TNV_clear_part[temp_min_index].value = value_to_insert;
                    TNV_clear_part[temp_min_index].frequency = times_met;
                }
            }
        }
    }
}

ValueMethodProfile* ValueProfileCollector::createProfile(Method_Handle mh, uint32 numkeys, uint32 keys[])
{
    hymutex_lock(profilesLock);
    ValueMethodProfile* profile = new ValueMethodProfile(this, mh);
    VPInstructionProfileData* vpmap = new VPInstructionProfileData[numkeys];
    // Allocate space for value maps
    for (uint32 index = 0; index < numkeys; index++){
        VPInstructionProfileData* profileData = new VPInstructionProfileData();
        profileData->TNV_Table =  new (struct Simple_TNV_Table[TNV_steady_size]);
        for (uint32 i = 0; i < TNV_steady_size; i++) {
            (profileData->TNV_Table[i]).frequency = 0;
            (profileData->TNV_Table[i]).value = 0;
        }
        if (TNV_clear_size > 0) {
            profileData->TNV_clear_part = new (struct Simple_TNV_Table[TNV_clear_size]);
            for (uint32 i = 0; i < TNV_clear_size; i++) {
                (profileData->TNV_clear_part[i]).frequency = 0;
                (profileData->TNV_clear_part[i]).value = 0;
            }
        }
        (profile->ValueMap)[keys[index]] = profileData;
    }
    assert(profilesByMethod.find(mh) == profilesByMethod.end());
    profilesByMethod[mh] = profile;
    hymutex_unlock(profilesLock);
    return profile;
}

POINTER_SIZE_INT ValueProfileCollector::find_max(Simple_TNV_Table *TNV_where)
{
    POINTER_SIZE_INT max_value = 0;
    uint32 temp_index, temp_max_frequency = 0;
    for (temp_index = 0; temp_index < TNV_steady_size; temp_index++) {
        Simple_TNV_Table *TNV_current = &(TNV_where[temp_index]);
        if (TNV_current->frequency > temp_max_frequency){
            temp_max_frequency = TNV_current->frequency;
            max_value = TNV_current->value;
        }
    }
    return (max_value);
}

ValueProfileCollector::ValueProfileCollector(EM_PC_Interface* em, const std::string& name, JIT_Handle genJit, 
                                             uint32 _TNV_steady_size, uint32 _TNV_clear_size,
                                             uint32 _clear_interval, algotypes _TNV_algo_type)
                                           : ProfileCollector(em, name, EM_PCTYPE_VALUE, genJit), 
                                             TNV_steady_size(_TNV_steady_size), TNV_clear_size(_TNV_clear_size),
                                             clear_interval(_clear_interval), TNV_algo_type(_TNV_algo_type)

{
    hymutex_create(&profilesLock, TM_MUTEX_NESTED);
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
    hymutex_destroy(profilesLock);
}

ValueMethodProfile::ValueMethodProfile(ValueProfileCollector* pc, Method_Handle mh)
    : MethodProfile(pc, mh)
{
    hymutex_create(&lock, TM_MUTEX_DEFAULT);
}

ValueMethodProfile::~ValueMethodProfile()
{
    hymutex_destroy(lock);
}

void ValueMethodProfile::addNewValue(uint32 instructionKey, POINTER_SIZE_INT valueToAdd)
{
    POINTER_SIZE_INT curr_value = valueToAdd;
    lockProfile();
    VPInstructionProfileData* _temp_vp = ValueMap[instructionKey];
    POINTER_SIZE_INT* last_value = &(_temp_vp->last_value);
    uint32* profile_tick = &(_temp_vp->profile_tick);
    uint32* num_times_profiled = &(_temp_vp->num_times_profiled);
    struct Simple_TNV_Table* TNV_clear_part = _temp_vp->TNV_clear_part;
    struct Simple_TNV_Table* TNV_steady_part = _temp_vp->TNV_Table;
    if ( getVPC()->TNV_algo_type == ValueProfileCollector::TNV_DIVIDED){
        if (*profile_tick == getVPC()->clear_interval){
            *profile_tick = 0;
            getVPC()->simple_tnv_clear(TNV_clear_part);
        }
        (*profile_tick)++;
    }
    if (curr_value == *last_value){
        (*num_times_profiled)++;
    }
    else {
        flushInstProfile(_temp_vp);
        *num_times_profiled = 1;
        getVPC()->insert_into_tnv_table (TNV_steady_part, TNV_clear_part, valueToAdd, *num_times_profiled);
        *last_value = curr_value;
    }
    unlockProfile();
}


POINTER_SIZE_INT ValueMethodProfile::getResult(uint32 instructionKey)
{
    lockProfile();
    VPInstructionProfileData* _temp_vp = ValueMap[instructionKey];
    if (_temp_vp == NULL) {
        assert(0);
        unlockProfile();
        return 0;
    }
    flushInstProfile(_temp_vp);
    POINTER_SIZE_INT result = getVPC()->find_max(_temp_vp->TNV_Table);
    unlockProfile();
    return result; 
}

void ValueMethodProfile::flushInstProfile(VPInstructionProfileData* instProfile)
{
    POINTER_SIZE_INT last_value = instProfile->last_value;
    uint32* num_times_profiled = &(instProfile->num_times_profiled);
    struct Simple_TNV_Table* TNV_clear_part = instProfile->TNV_clear_part;
    struct Simple_TNV_Table* TNV_steady_part = instProfile->TNV_Table;
    getVPC()->insert_into_tnv_table (TNV_steady_part, TNV_clear_part, last_value, *num_times_profiled);
    *num_times_profiled = 0;
}

void ValueMethodProfile::dumpValues(std::ostream& os)
{
    std::map<uint32, VPInstructionProfileData*>::const_iterator mapIter;
    assert(pc->type == EM_PCTYPE_VALUE);
    lockProfile();
    os << "===== Value profile dump, " << ValueMap.size() << " element(s) ===" << std::endl;
    for (mapIter = ValueMap.begin(); mapIter != ValueMap.end(); mapIter++) {
        os << "=== Instruction key: " << mapIter->first;
        VPInstructionProfileData* _temp_vp = mapIter->second;
        flushInstProfile(_temp_vp);
        os << ", num_times_profiled: " << _temp_vp->num_times_profiled << ", profile_tick: " << _temp_vp->profile_tick << std::endl;
        struct Simple_TNV_Table * TNV_steady_part = _temp_vp->TNV_Table;
        if (TNV_steady_part != NULL) {
            uint32 size = ((ValueProfileCollector*)pc)->TNV_steady_size;
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
        struct Simple_TNV_Table * TNV_clear_part = _temp_vp->TNV_clear_part;
        if (TNV_clear_part != NULL) {
            uint32 size = ((ValueProfileCollector*)pc)->TNV_clear_size;
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
    unlockProfile();
    os << "====== End of dump ======================" << std::endl;
}

ValueProfileCollector* ValueMethodProfile::getVPC() const {
    assert(pc->type == EM_PCTYPE_VALUE);
    return ((ValueProfileCollector*)pc);
}


MethodProfile* ValueProfileCollector::getMethodProfile(Method_Handle mh) const
{
    ValueProfilesMap::const_iterator it = profilesByMethod.find(mh);
    if (it == profilesByMethod.end()) {
        return NULL;
    }
    return it->second;
}



POINTER_SIZE_INT value_profiler_get_top_value (Method_Profile_Handle mph, uint32 instructionKey)
{
    assert(mph != NULL);
    MethodProfile* mp = (MethodProfile*)mph;
    assert(mp->pc->type == EM_PCTYPE_VALUE);
    ValueMethodProfile* vmp = (ValueMethodProfile*)mp;
    return vmp->getResult(instructionKey);
}

void value_profiler_add_value (Method_Profile_Handle mph, uint32 instructionKey, POINTER_SIZE_INT valueToAdd)
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
