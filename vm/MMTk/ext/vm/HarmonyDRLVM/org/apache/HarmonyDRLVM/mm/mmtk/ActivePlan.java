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
import org.mmtk.plan.PlanConstraints;

import org.vmmagic.pragma.*;

/**
 * This class contains interfaces to access the current plan, plan local and
 * plan constraints instances.
 */
public final class ActivePlan extends org.mmtk.vm.ActivePlan implements Uninterruptible {

  /* Collector and Mutator Context Management */
  private static final int MAX_CONTEXTS = 1;  //wjw -- just do single thread for starts
  private static CollectorContext[] collectors = new CollectorContext[MAX_CONTEXTS];
  private static int collectorCount = 0; // Number of collector instances 
  private static MutatorContext[] mutators = new MutatorContext[MAX_CONTEXTS];
  private static int mutatorCount = 0; // Number of mutator instances 
  private static SynchronizedCounter mutatorCounter = new SynchronizedCounter();

  /** @return The active Plan instance. */
  public final Plan global() throws InlinePragma {
    return SelectedPlan.get();
  } 
  
  /** @return The active PlanConstraints instance. */
  public final PlanConstraints constraints() throws InlinePragma {
    return SelectedPlanConstraints.get();
  } 
  
  /** @return The active CollectorContext instance. */
  public final CollectorContext collector() throws InlinePragma {
    return SelectedCollectorContext.get();
  }
  
  /** @return The active MutatorContext instance. */
  public final MutatorContext mutator() throws InlinePragma {
    return SelectedMutatorContext.get();
  }
  
  /**
   * Return the MutatorContext instance given its unique identifier.
   * 
   * @param id The identifier of the MutatorContext to return
   * @return The specified MutatorContext
   */ 
  public final MutatorContext mutator(int id) throws InlinePragma {
    return mutators[id];
  }

  /** @return The number of registered CollectorContext instances. */
  public final int collectorCount() throws InlinePragma {
    return collectorCount;
  }
   
  /** @return The number of registered MutatorContext instances. */
  public final int mutatorCount() {
    return mutatorCount;
  }

  /** Reset the mutator iterator */
  public void resetMutatorIterator() {
    mutatorCounter.reset();
  }
 
  /** 
   * Return the next <code>MutatorContext</code> in a
   * synchronized iteration of all mutators.
   *  
   * @return The next <code>MutatorContext</code> in a
   *  synchronized iteration of all mutators, or
   *  <code>null</code> when all mutators have been done.
   */
  public MutatorContext getNextMutator() {
    int id = mutatorCounter.increment();
    return id >= mutatorCount ? null : mutators[id];
  } 

  /**
   * Register a new CollectorContext instance.
   *
   * FIXME: Possible race in allocation of ids. Should be synchronized.
   *
   * @param collector The CollectorContext to register
   * @return The CollectorContext's unique identifier
   */
  public final int registerCollector(CollectorContext collector) throws InterruptiblePragma {
    collectors[collectorCount] = collector;
    return collectorCount++;
  }
  
  /**
   * Register a new MutatorContext instance.
   *
   * FIXME: Possible race in allocation of ids. Should be synchronized.
   *
   * @param mutator The MutatorContext to register
   * @return The MutatorContext's unique identifier
   */
    //wjw -- this needs to be called by VM when a new java thread is created
  public final int registerMutator(MutatorContext mutator) throws InterruptiblePragma {
    mutators[mutatorCount] = mutator;
    return mutatorCount++;
  } 
}
