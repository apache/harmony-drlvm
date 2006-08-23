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

package org.apache.HarmonyDRLVM.mm.mmtk;

import org.mmtk.plan.Plan;
import org.mmtk.plan.CollectorContext;
import org.mmtk.plan.MutatorContext;
import org.mmtk.utility.Constants;
import org.mmtk.utility.Finalizer;
import org.mmtk.utility.heap.HeapGrowthManager;
import org.mmtk.utility.ReferenceProcessor;
import org.mmtk.utility.options.Options;
import org.mmtk.vm.*;

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
      System.out.println("org.apache.HarmonyDRLVM.mm.mmtk.Collection.init() has been called -1-");
      VM.assertions._assert(false);
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
      //System.out.println("org.apache.HarmonyDRLVM.mm.mmtk.Collection.triggerCollection() -2- has been called why = " + why);
      //VM.assertions._assert(false);
      CollectorContext cc = SelectedPlan.ap.collector();
      cc.collect();
  }
  

  /**
   * Triggers a collection without allowing for a thread switch.  This is needed
   * for Merlin lifetime analysis used by trace generation 
   *
   * @param why the reason why a collection was triggered.  0 to
   * <code>TRIGGER_REASONS - 1</code>.
   */
  public final void triggerCollectionNow(int why) 
    throws LogicallyUninterruptiblePragma {
    System.out.println("org.apache.HarmonyDRLVM.mm.mmtk.Collection.triggerCollection() has been called -3- why = " + why);
    VM.assertions._assert(false);
  }

  /**
   * Trigger an asynchronous collection, checking for memory
   * exhaustion first.
   */
  public final void triggerAsyncCollection()
    throws UninterruptiblePragma {
    System.out.println("org.apache.HarmonyDRLVM.mm.mmtk.Collection.triggerAsyncCollection() has been called -4-");
    VM.assertions._assert(false);
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
   VM.assertions._assert(false);
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
        //System.out.println("org.apache.HarmonyDRLVM.mm.mmtk.Collection.prepareMutator() has been called -5-"); 
       // VM.assertions._assert(false);
  }
  
  /**
   * Prepare a collector for a collection.
   *
   * @param c the collector to prepare
   */
  public final void prepareCollector(CollectorContext c) {
      //wjw since its single thread NoGC for now, do nothing
      //probably need to enumerate the roots (???)
      //System.out.println("org.apache.HarmonyDRLVM.mm.mmtk.Collection.prepareCollector() has been called -6- ");  
      //VM.assertions._assert(false);
  }

  /**
   * Rendezvous with all other processors, returning the rank
   * (that is, the order this processor arrived at the barrier).
   */
  private boolean rendFlag = false;
  public final int rendezvous(int where) throws UninterruptiblePragma {
      //wjw since its single thread NoGC for now, do nothing
      //probably need to wait on a sync barrier before proceeding
      if (rendFlag == false) 
      {
          System.out.println("org.apache.HarmonyDRLVM.mm.mmtk.Collection.rendezvous() has been called -7-"); 
          rendFlag = true;
      }
      ////////////////VM.assertions._assert(false);
    return 1;
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
      System.out.println("org.apache.HarmonyDRLVM.mm.mmtk.Collection.rendezvous() has been called -8-");
      VM.assertions._assert(false);
    }
}
