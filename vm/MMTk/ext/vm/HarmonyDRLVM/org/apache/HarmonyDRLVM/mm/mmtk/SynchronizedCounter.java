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

import org.vmmagic.pragma.*;
import org.vmmagic.unboxed.Offset;

/**
 * A counter that supports atomic increment and reset.
 *
 * $Id: SynchronizedCounter.java,v 1.2 2005/07/20 14:32:01 dframpton-oss Exp $
 *
 * @author Perry Cheng
 */
public final class SynchronizedCounter extends org.mmtk.vm.SynchronizedCounter implements Uninterruptible {

  private static Offset offset = Offset.max();

  public static void boot() {
    //offset = VM_Entrypoints.synchronizedCounterField.getOffset();
      System.out.println("org.apache.HarmonyDRLVM.mm.mmtk.SynchronizedCounter.boot()");
  }

  private int count = 0;

  public int reset() {
    //    int offset = VM_Interface.synchronizedCounterOffset;
    /* int oldValue = count;
    int actualOldValue = VM_Synchronization.fetchAndAdd(this, offset, -oldValue);
    if (actualOldValue != oldValue) {
      VM.sysWriteln("oldValue = ", oldValue);
      VM.sysWriteln("actualOldValue = ", actualOldValue);
      VM.sysFail("Concurrent use of SynchronizedCounter.reset");
    }
    return oldValue;
    */
    System.out.println("org.apache.HarmonyDRLVM.mm.mmtk.SynchronizedCounter.reset()");
      return 0;
  }

  // Returns the value before the add
  //
  public int increment() {
    //if (VM.VerifyAssertions) VM._assert(!offset.isMax());
    //return VM_Synchronization.fetchAndAdd(this, offset, 1);
    System.out.println("org.apache.HarmonyDRLVM.mm.mmtk.SynchronizedCounter.increment()");
    return 0;
  }

  public int peek () {
    //return count;
    System.out.println("org.apache.HarmonyDRLVM.mm.mmtk.SynchronizedCounter.peek()");
      return 0;
  }

}
