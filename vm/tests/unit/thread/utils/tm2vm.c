/*
 *  Copyright 2005-2006 The Apache Software Foundation or its licensors, as applicable.
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
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
 * @author Artem Aliev
 * @version $Revision$
 */  

#include "thread_private.h"
#include <open/hythread_ext.h>
#include "open/thread_externals.h"
#include "thread_unit_test_utils.h"

/**
 * This file contains the functions which eventually should become part of vmcore.
 * This localizes the dependencies of Thread Manager on vmcore component.
 */
 
void *vm_object_get_lockword_addr(jobject monitor){
    //return (*(ManagedObject**)monitor)->get_obj_info_addr();
    return (void *)&monitor->object->lockword;
}

void* vm_jthread_get_tm_data(jthread thread)
{
    /*
      JNIEnv *jenv = (JNIEnv*)jni_native_intf;        
      jclass jThreadClass = jenv->GetObjectClass(thread);
      jfieldID field_id = jenv->GetFieldID(jThreadClass, "vm_thread", "J");
      POINTER_SIZE_INT data = (POINTER_SIZE_INT)jenv->GetLongField(thread, field_id);
    
      return (void *)data;
        */
    return thread->object->data;
}

void vm_jthread_set_tm_data(jthread jt, void* nt) {
    /*
      JNIEnv *jenv = (JNIEnv*)jni_native_intf;        
      jclass jthreadclass = jenv->GetObjectClass(jt);
      jfieldID field_id = jenv->GetFieldID(jthreadclass, "vm_thread", "J");
      jenv->SetLongField(jt, field_id, (jlong)(POINTER_SIZE_INT)nt);
    */
    jt->object->data = nt;
}

IDATA jthread_throw_exception(char* name, char* message) {
    return 0;
}
VMEXPORT int jthread_throw_exception_object(jobject object) {
    return 0;
}

int vm_objects_are_equal(jobject obj1, jobject obj2){
    //ObjectHandle h1 = (ObjectHandle)obj1;
    //ObjectHandle h2 = (ObjectHandle)obj2;
    if (obj1 == NULL && obj2 == NULL){
        return 1;
    } 
    if (obj1 == NULL || obj2 == NULL){
        return 0;
    } 
    return obj1->object == obj2->object;
}

void * vm_get_object(jobject obj){
    if (obj == NULL){
        return NULL;
    } 
    return obj->object;
}


int ti_is_enabled(){
    return 1;
}
//--------------------------------------------------------------------------------

int vm_attach() {
    return 0;
}
int vm_detach() {
    return 0;
}
void jvmti_send_thread_start_end_event(int is_start) {
    //is_start ? process_jvmti_event(JVMTI_EVENT_THREAD_START, 0, 0)
    //    :process_jvmti_event(JVMTI_EVENT_THREAD_END, 1, 0);
}
void jvmti_send_wait_monitor_event(jobject monitor, jlong timeout) {
    hythread_t tm_native_thread = hythread_self();
    //TRACE2("jvmti.monitor.wait", "Monitor wait event, monitor = " << monitor);
    tm_native_thread->state &= ~TM_THREAD_STATE_RUNNABLE;
    tm_native_thread->state |= TM_THREAD_STATE_WAITING | TM_THREAD_STATE_IN_MONITOR_WAIT;
    tm_native_thread->state |= timeout ? TM_THREAD_STATE_WAITING_WITH_TIMEOUT :
        TM_THREAD_STATE_WAITING_INDEFINITELY;
    //process_jvmti_event(JVMTI_EVENT_MONITOR_WAIT, 1, monitor, timeout);
}
void jvmti_send_waited_monitor_event(jobject monitor, jboolean is_timed_out) {
    hythread_t tm_native_thread = hythread_self();
    //TRACE2("jvmti.monitor.waited", "Monitor wait event, monitor = " << monitor);
    tm_native_thread->state &= ~(TM_THREAD_STATE_WAITING | TM_THREAD_STATE_IN_MONITOR_WAIT |
                                 TM_THREAD_STATE_WAITING_INDEFINITELY | 
                                 TM_THREAD_STATE_WAITING_WITH_TIMEOUT);
    tm_native_thread->state |= TM_THREAD_STATE_RUNNABLE;
    //process_jvmti_event(JVMTI_EVENT_MONITOR_WAITED, 1, monitor, is_timed_out);
}
void jvmti_send_contended_enter_or_entered_monitor_event(jobject monitor, int isEnter) {
    hythread_t tm_native_thread = hythread_self();
    //TRACE2("jvmti.monitor.enter", "Monitor enter event, monitor = " << monitor << " is enter= " << isEnter);
    if(isEnter){
        tm_native_thread->state |= TM_THREAD_STATE_BLOCKED_ON_MONITOR_ENTER;
        tm_native_thread->state &= ~TM_THREAD_STATE_RUNNABLE;
        //process_jvmti_event(JVMTI_EVENT_MONITOR_CONTENDED_ENTER, 1, monitor);
    } else {
        tm_native_thread->state &= ~TM_THREAD_STATE_BLOCKED_ON_MONITOR_ENTER;
        tm_native_thread->state |= TM_THREAD_STATE_RUNNABLE;
        //process_jvmti_event(JVMTI_EVENT_MONITOR_CONTENDED_ENTERED, 1, monitor);
    }
}
