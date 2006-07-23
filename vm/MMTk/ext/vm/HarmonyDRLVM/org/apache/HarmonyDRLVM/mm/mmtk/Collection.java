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

/*
 * (C) Copyright Department of Computer Science,
 * Australian National University. 2004
 *
 * (C) Copyright IBM Corp. 2001, 2003
 */
package org.apache.HarmonyDRLVM.mm.mmtk;

import org.mmtk.plan.Plan;
import org.mmtk.plan.CollectorContext;
import org.mmtk.plan.MutatorContext;
import org.mmtk.utility.Constants;
import org.mmtk.utility.Finalizer;
import org.mmtk.utility.heap.HeapGrowthManager;
import org.mmtk.utility.ReferenceProcessor;
import org.mmtk.utility.options.Options;

/*  wjw we will need some analog to the below imports later on, keep the below as a reference
import com.ibm.JikesRVM.VM;
import com.ibm.JikesRVM.VM_CompiledMethod;
import com.ibm.JikesRVM.VM_CompiledMethods;
import com.ibm.JikesRVM.VM_Constants;
import com.ibm.JikesRVM.VM_Magic;
import com.ibm.JikesRVM.VM_Processor;
import com.ibm.JikesRVM.VM_Scheduler;
import com.ibm.JikesRVM.VM_Thread;
import com.ibm.JikesRVM.VM_Time;
import com.ibm.JikesRVM.classloader.VM_Atom;
import com.ibm.JikesRVM.classloader.VM_Method;
import com.ibm.JikesRVM.memoryManagers.mmInterface.VM_CollectorThread;
import com.ibm.JikesRVM.memoryManagers.mmInterface.MM_Interface;
import com.ibm.JikesRVM.memoryManagers.mmInterface.SelectedPlan;
import com.ibm.JikesRVM.memoryManagers.mmInterface.SelectedCollectorContext;
import com.ibm.JikesRVM.memoryManagers.mmInterface.SelectedMutatorContext;
*/

import org.vmmagic.unboxed.*;
import org.vmmagic.pragma.*;

public class Collection extends org.mmtk.vm.Collection implements Constants, Uninterruptible {

  /****************************************************************************
   *
   * Class variables
   */
  /***********************************************************************
   *
   * Initialization
   */

  /**
   * Initialization that occurs at <i>build</i> time.  The values of
   * statics at the completion of this routine will be reflected in
   * the boot image.  Any objects referenced by those statics will be
   * transitively included in the boot image.
   *
   * This is called from MM_Interface.
   */
  public static final void init() throws InterruptiblePragma {
        //wjw NoGC does not have a collector, do nothing for starts
      System.out.println("org.apache.HarmonyDRLVM.mm.mmtk.Collection.init() has been called");
  }

  /**
   * An enumerator used to forward root objects
   */
  /**
   * Triggers a collection.
   *
   * @param why the reason why a collection was triggered.  0 to
   * <code>TRIGGER_REASONS - 1</code>.
   */
  public final void triggerCollection(int why) throws InterruptiblePragma {
    System.out.println("org.apache.HarmonyDRLVM.mm.mmtk.Collection.triggerCollection() has been called why = " + why);
  }
  
    /* whw -- toss all of triggerCollectionStatic()
  public static final void triggerCollectionStatic(int why) throws InterruptiblePragma {
    if (VM.VerifyAssertions) VM._assert((why >= 0) && (why < TRIGGER_REASONS)); 
    Plan.collectionInitiated();

    if (Options.verbose.getValue() >= 4) {
      VM.sysWriteln("Entered VM_Interface.triggerCollection().  Stack:");
      VM_Scheduler.dumpStack();
    }
    if (why == EXTERNAL_GC_TRIGGER) {
      SelectedPlan.get().userTriggeredGC();
      if (Options.verbose.getValue() == 1 || Options.verbose.getValue() == 2) 
        VM.sysWrite("[Forced GC]");
    }
    if (Options.verbose.getValue() > 2) 
      VM.sysWriteln("Collection triggered due to ", triggerReasons[why]);
    Extent sizeBeforeGC = HeapGrowthManager.getCurrentHeapSize();
    long start = VM_Time.cycles();
    VM_CollectorThread.collect(VM_CollectorThread.handshake, why);
    long end = VM_Time.cycles();
    double gcTime = VM_Time.cyclesToMillis(end - start);
    if (Options.verbose.getValue() > 2) VM.sysWriteln("Collection finished (ms): ", gcTime);

    if (SelectedPlan.get().isLastGCFull() && 
   sizeBeforeGC.EQ(HeapGrowthManager.getCurrentHeapSize()))
      checkForExhaustion(why, false);
    
    Plan.checkForAsyncCollection();
  }
  */

  /**
   * Triggers a collection without allowing for a thread switch.  This is needed
   * for Merlin lifetime analysis used by trace generation 
   *
   * @param why the reason why a collection was triggered.  0 to
   * <code>TRIGGER_REASONS - 1</code>.
   */
  public final void triggerCollectionNow(int why) 
    throws LogicallyUninterruptiblePragma {
    System.out.println("org.apache.HarmonyDRLVM.mm.mmtk.Collection.triggerCollection() has been called why = " + why);
  }

  /**
   * Trigger an asynchronous collection, checking for memory
   * exhaustion first.
   */
  public final void triggerAsyncCollection()
    throws UninterruptiblePragma {
    System.out.println("org.apache.HarmonyDRLVM.mm.mmtk.Collection.triggerAsyncCollection() has been called"); 
  }

  /**
   * Determine whether a collection cycle has fully completed (this is
   * used to ensure a GC is not in the process of completing, to
   * avoid, for example, an async GC being triggered on the switch
   * from GC to mutator thread before all GC threads have switched.
   *
   * @return True if GC is not in progress.
   */
 public final boolean noThreadsInGC() throws UninterruptiblePragma {
   return true;   //wjw for starts, we are doing NoGC with single thread
 }

  /**
   * Prepare a mutator for a collection.
   *
   * @param m the mutator to prepare
   */
  public final void prepareMutator(MutatorContext m) {
        //wjw since its single thread NoGC for now, do nothing
        //probably need to suspend the mutator at a safepoint
        System.out.println("org.apache.HarmonyDRLVM.mm.mmtk.Collection.prepareMutator() has been called"); 
  }
  
  /**
   * Prepare a collector for a collection.
   *
   * @param c the collector to prepare
   */
  public final void prepareCollector(CollectorContext c) {
      //wjw since its single thread NoGC for now, do nothing
      //probably need to enumerate the roots (???)
      System.out.println("org.apache.HarmonyDRLVM.mm.mmtk.Collection.prepareCollector() has been called");  
  }

  /**
   * Rendezvous with all other processors, returning the rank
   * (that is, the order this processor arrived at the barrier).
   */
  public final int rendezvous(int where) throws UninterruptiblePragma {
      //wjw since its single thread NoGC for now, do nothing
      //probably need to wait on a sync barrier before proceeding
      System.out.println("org.apache.HarmonyDRLVM.mm.mmtk.Collection.rendezvous() has been called"); 
    return 0;
  }

  /***********************************************************************
   *
   * Finalizers
   */
  
  /**
   * Schedule the finalizerThread, if there are objects to be
   * finalized and the finalizerThread is on its queue (ie. currently
   * idle).  Should be called at the end of GC after moveToFinalizable
   * has been called, and before mutators are allowed to run.
   */
  public static final void scheduleFinalizerThread ()
    throws UninterruptiblePragma {
      //wjw since its single thread NoGC for now, do nothing
      System.out.println("org.apache.HarmonyDRLVM.mm.mmtk.Collection.rendezvous() has been called");
    }
}
