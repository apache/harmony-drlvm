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

import org.mmtk.plan.TraceLocal;
import org.mmtk.utility.scan.*;
import org.mmtk.utility.Constants;
import org.mmtk.vm.VM;
import org.vmmagic.unboxed.*;
import org.vmmagic.pragma.*;

public class Scanning extends org.mmtk.vm.Scanning implements Constants, Uninterruptible {
  /****************************************************************************
   *
   * Class variables
   */
  private static final boolean TRACE_PRECOPY = false; // DEBUG

  /** Counter to track index into thread table for root tracing.  */
  private static SynchronizedCounter threadCounter = new SynchronizedCounter();

  /**
   * Delegated scanning of a object, processing each pointer field
   * encountered. 
   * @param object The object to be scanned.
   */
  public final void scanObject(TraceLocal trace, ObjectReference object) 
    throws UninterruptiblePragma, InlinePragma {
    VM.assertions._assert(false);  //debug when you hit this
  }
  
  /**
   * Delegated precopying of a object's children, processing each pointer field
   * encountered.
   *
   * @param object The object to be scanned.
   */
  public final void precopyChildren(TraceLocal trace, ObjectReference object) 
    throws UninterruptiblePragma, InlinePragma {
   VM.assertions._assert(false);  //debug when you hit this
  }
  
  /**
   * Delegated enumeration of the pointers in an object, calling back
   * to a given plan for each pointer encountered. 
   * @param object The object to be scanned.
   * @param _enum the Enumerator object through which the trace
   * is made
   */
  public final void enumeratePointers(ObjectReference object, Enumerator _enum) 
    throws UninterruptiblePragma, InlinePragma {
    VM.assertions._assert(false);  //debug when you hit this
  }

  /**
   * Prepares for using the <code>computeAllRoots</code> method.  The
   * thread counter allows multiple GC threads to co-operatively
   * iterate through the thread data structure (if load balancing
   * parallel GC threads were not important, the thread counter could
   * simply be replaced by a for loop).
   */
  public final void resetThreadCounter() {
    threadCounter.reset();
  }

  /**
   * Pre-copy all potentially movable instances used in the course of
   * GC.  This includes the thread objects representing the GC threads
   * themselves.  It is crucial that these instances are forwarded
   * <i>prior</i> to the GC proper.  Since these instances <i>are
   * not</i> enqueued for scanning, it is important that when roots
   * are computed the same instances are explicitly scanned and
   * included in the set of roots.  The existence of this method
   * allows the actions of calculating roots and forwarding GC
   * instances to be decoupled. 
   * 
   * The thread table is scanned in parallel by each processor, by striding
   * through the table at a gap of chunkSize*numProcs.  Feel free to adjust
   * chunkSize if you want to tune a parallel collector.
   * 
   * Explicitly no-inlined to prevent over-inlining of collectionPhase.
   * 
   * TODO Experiment with specialization to remove virtual dispatch ?
   */
  public final void preCopyGCInstances(TraceLocal trace) 
  throws NoInlinePragma {
        System.out.println("org.apache.HarmonyDRLVM.mm.mmtk.Scanning.preCopyGCInstances() was called");
        VM.assertions._assert(false);
  }
  
 
  /**
   * Enumerator the pointers in an object, calling back to a given plan
   * for each pointer encountered. <i>NOTE</i> that only the "real"
   * pointer fields are enumerated, not the TIB.
   *
   * @param trace The trace object to use to report precopy objects.
   * @param object The object to be scanned.
   */
  private static void precopyChildren(TraceLocal trace, Object object) 
    throws UninterruptiblePragma, InlinePragma {
        System.out.println("org.apache.HarmonyDRLVM.mm.mmtk.Scanning.precopyChildren() was called");
        VM.assertions._assert(false);
  }

 /**
   * Computes all roots.  This method establishes all roots for
   * collection and places them in the root values, root locations and
   * interior root locations queues.  This method should not have side
   * effects (such as copying or forwarding of objects).  There are a
   * number of important preconditions:
   *
   * <ul> 
   * <li> All objects used in the course of GC (such as the GC thread
   * objects) need to be "pre-copied" prior to calling this method.
   * <li> The <code>threadCounter</code> must be reset so that load
   * balancing parallel GC can share the work of scanning threads.
   * </ul>
   * 
   * TODO rewrite to avoid the per-thread synchronization, like precopy.
   *
   * @param trace The trace object to use to report root locations.
   */
  public final void computeAllRoots(TraceLocal trace) {
    //System.out.println("*****************************org.apache.HarmonyDRLVM.mm.mmtk.Scanning.computeAllRoots() was called TraceLocal = "  + trace);
    /////////////////VM.assertions._assert(false);
      return;
  }
}
