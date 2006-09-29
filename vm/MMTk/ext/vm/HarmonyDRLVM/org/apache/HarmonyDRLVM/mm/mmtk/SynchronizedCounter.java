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

package org.apache.HarmonyDRLVM.mm.mmtk;

import org.vmmagic.pragma.*;
import org.vmmagic.unboxed.Offset;
import org.mmtk.vm.*;

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
  }

  private int count = 0;

  public int reset() {
    int oldValue = count;  // unsynchronized access for now, single thread operation only
    count = 0;
    //System.out.println("org.apache.HarmonyDRLVM.mm.mmtk.SynchronizedCounter.reset() oldValue = " + oldValue);
      //return 0;

 
      return oldValue;
  }

  // Returns the value before the add
  //
  public int increment() {
    int oldValue = count;
    count++;
      //if (count == 1)
            //System.out.println("org.apache.HarmonyDRLVM.mm.mmtk.SynchronizedCounter.increment() oldValue = " + oldValue);
    return oldValue;
  }

  public int peek () {
    //System.out.println("org.apache.HarmonyDRLVM.mm.mmtk.SynchronizedCounter.peek() count = " + count);
    return count;
  }

}
