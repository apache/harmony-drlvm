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
 * @author Gregory Shimansky
 * @version $Revision: 1.1.2.1.4.4 $
 */  
/*
 * JVMTI heap API
 */

#include "jvmti_direct.h"
#include "jvmti_utils.h"
#include "cxxlog.h"

#include "open/gc.h"

#include "suspend_checker.h"

/*
 * Get Tag
 *
 * Retrieve the tag associated with an object. The tag is a long
 * value typically used to store a unique identifier or pointer to
 * object information. The tag is set with SetTag. Objects for
 * which no tags have been set return a tag value of zero.
 *
 * OPTIONAL Functionality
 */
jvmtiError JNICALL
jvmtiGetTag(jvmtiEnv* env,
            jobject UNREF object,
            jlong* UNREF tag_ptr)
{
    TRACE2("jvmti.heap", "GetTag called");
    SuspendEnabledChecker sec;
    /*
     * Check given env & current phase.
     */
    jvmtiPhase phases[] = {JVMTI_PHASE_START, JVMTI_PHASE_LIVE};

    CHECK_EVERYTHING();

    //TBD

    return JVMTI_NYI;
}

/*
 * Set Tag
 *
 * Set the tag associated with an object. The tag is a long value
 * typically used to store a unique identifier or pointer to object
 * information. The tag is visible with GetTag.
 *
 * OPTIONAL Functionality
 */
jvmtiError JNICALL
jvmtiSetTag(jvmtiEnv* env,
            jobject UNREF object,
            jlong UNREF tag)
{
    TRACE2("jvmti.heap", "SetTag called");
    SuspendEnabledChecker sec;
    /*
     * Check given env & current phase.
     */
    jvmtiPhase phases[] = {JVMTI_PHASE_START, JVMTI_PHASE_LIVE};

    CHECK_EVERYTHING();

    //TBD

    return JVMTI_NYI;
}

/*
 * Force Garbage Collection
 *
 * Force the VM to perform a garbage collection. The garbage
 * collection is as complete as possible. This function does
 * not cause finalizers to be run. This function does not return
 * until the garbage collection is finished.
 *
 * REQUIRED Functionality
 */
jvmtiError JNICALL
jvmtiForceGarbageCollection(jvmtiEnv* env)
{
    TRACE2("jvmti.heap", "ForceGarbageCollection called");
    SuspendEnabledChecker sec;
    /*
     * Check given env & current phase.
     */
    jvmtiPhase phases[] = {JVMTI_PHASE_LIVE};

    CHECK_EVERYTHING();

    assert(hythread_is_suspend_enabled());
    // no matter how counter-intuitive,
    // gc_force_gc() expects gc_enabled_status == disabled,
    // but, obviously, at a GC safepoint.
    // See gc-safety (aka suspend-safety) rules explained elsewhere
    // -salikh 2005-05-12
    tmn_suspend_disable();
    gc_force_gc();
    tmn_suspend_enable();

    return JVMTI_NYI;
}

/*
 * Iterate Over Objects Reachable From Object
 *
 * This function iterates over all objects that are directly and
 * indirectly reachable from the specified object.
 *
 * OPTIONAL Functionality
 */
jvmtiError JNICALL
jvmtiIterateOverObjectsReachableFromObject(jvmtiEnv* env,
                                           jobject UNREF object,
                                           jvmtiObjectReferenceCallback UNREF object_reference_callback,
                                           void* UNREF user_data)
{
    TRACE2("jvmti.heap", "IterateOverObjectsReachableFromObject called");
    SuspendEnabledChecker sec;
    /*
     * Check given env & current phase.
     */
    jvmtiPhase phases[] = {JVMTI_PHASE_LIVE};

    CHECK_EVERYTHING();

    //TBD

    return JVMTI_NYI;
}

/*
 * Iterate Over Reachable Objects
 *
 * This function iterates over the root objects and all objects
 * that are directly and indirectly reachable from the root
 * objects. The root objects comprise the set of system classes,
 * JNI globals, references from thread stacks, and other objects
 * used as roots for the purposes of garbage collection.
 *
 * OPTIONAL Functionality
 */
jvmtiError JNICALL
jvmtiIterateOverReachableObjects(jvmtiEnv* env,
                                 jvmtiHeapRootCallback UNREF heap_root_callback,
                                 jvmtiStackReferenceCallback UNREF stack_ref_callback,
                                 jvmtiObjectReferenceCallback UNREF object_ref_callback,
                                 void* UNREF user_data)
{
    TRACE2("jvmti.heap", "IterateOverReachableObjects called");
    SuspendEnabledChecker sec;
    /*
     * Check given env & current phase.
     */
    jvmtiPhase phases[] = {JVMTI_PHASE_LIVE};

    CHECK_EVERYTHING();

    //TBD

    return JVMTI_NYI;
}

/*
 * Iterate Over Heap
 *
 * Iterate over all objects in the heap. This includes both
 * reachable and unreachable objects.
 *
 * OPTIONAL Functionality
 */
jvmtiError JNICALL
jvmtiIterateOverHeap(jvmtiEnv* env,
                     jvmtiHeapObjectFilter UNREF object_filter,
                     jvmtiHeapObjectCallback UNREF heap_object_callback,
                     void* UNREF user_data)
{
    TRACE2("jvmti.heap", "IterateOverHeap called");
    SuspendEnabledChecker sec;
    /*
     * Check given env & current phase.
     */
    jvmtiPhase phases[] = {JVMTI_PHASE_LIVE};

    CHECK_EVERYTHING();

    //TBD

    return JVMTI_NYI;
}

/*
 * Iterate Over Instances Of Class
 *
 * Iterate over all objects in the heap that are instances of
 * the specified class. This includes both reachable and
 * unreachable objects.
 *
 * OPTIONAL Functionality
 */
jvmtiError JNICALL
jvmtiIterateOverInstancesOfClass(jvmtiEnv* env,
                                 jclass UNREF klass,
                                 jvmtiHeapObjectFilter UNREF object_filter,
                                 jvmtiHeapObjectCallback UNREF heap_object_callback,
                                 void* UNREF user_data)
{
    TRACE2("jvmti.heap", "IterateOverInstancesOfClass called");
    SuspendEnabledChecker sec;
    /*
     * Check given env & current phase.
     */
    jvmtiPhase phases[] = {JVMTI_PHASE_LIVE};

    CHECK_EVERYTHING();

    //TBD

    return JVMTI_NYI;
}

/*
 * Get Objects With Tags
 *
 * Return objects in the heap with the specified tags. The format
 * is parallel arrays of objects and tags.
 *
 * OPTIONAL Functionality
 */
jvmtiError JNICALL
jvmtiGetObjectsWithTags(jvmtiEnv* env,
                        jint UNREF tag_count,
                        const jlong* UNREF tags,
                        jint* UNREF count_ptr,
                        jobject** UNREF object_result_ptr,
                        jlong** UNREF tag_result_ptr)
{
    TRACE2("jvmti.heap", "GetObjectsWithTags called");
    SuspendEnabledChecker sec;
    /*
     * Check given env & current phase.
     */
    jvmtiPhase phases[] = {JVMTI_PHASE_LIVE};

    CHECK_EVERYTHING();

    //TBD

    return JVMTI_NYI;
}
