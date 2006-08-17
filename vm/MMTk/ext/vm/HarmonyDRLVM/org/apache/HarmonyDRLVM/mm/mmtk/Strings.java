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

import org.vmmagic.unboxed.*;
import org.vmmagic.pragma.*;

public final class Strings extends org.mmtk.vm.Strings implements Uninterruptible {
  /**
   * Log a message.
   *
   * @param c character array with message starting at index 0
   * @param len number of characters in message
   */
  public final void write(char [] c, int len) {
      for (int xx=0; xx < c.length; xx++) 
      {
        System.out.print(c[xx]);    
      }
      System.out.println("");
  }

  /**
   * Log a thread identifier and a message.
   *
   * @param c character array with message starting at index 0
   * @param len number of characters in message
   */
  public final void writeThreadId(char [] c, int len) {
      for (int xx=0; xx < c.length; xx++) 
      {
          System.out.print(c[xx]);    
      }
      System.out.println("");    
  }

  /**
   * Copies characters from the string into the character array.
   * Thread switching is disabled during this method's execution.
   * <p>
   * <b>TODO:</b> There are special memory management semantics here that
   * someone should document.
   *
   * @param src the source string
   * @param dst the destination array
   * @param dstBegin the start offset in the desination array
   * @param dstEnd the index after the last character in the
   * destination to copy to
   * @return the number of characters copied.
   */
  public final int copyStringToChars(String src, char [] dst,
                                     int dstBegin, int dstEnd)
    throws LogicallyUninterruptiblePragma {
      /*
    if (VM.runningVM)
      VM_Processor.getCurrentProcessor().disableThreadSwitching();
    int len = src.length();
    int n = (dstBegin + len <= dstEnd) ? len : (dstEnd - dstBegin);
    for (int i = 0; i < n; i++) 
      Barriers.setArrayNoBarrierStatic(dst, dstBegin + i, src.charAt(i));
    if (VM.runningVM)
      VM_Processor.getCurrentProcessor().enableThreadSwitching();
    return n;
    */
      System.out.println("org.apache.HarmonyDRLVM.mm.mmtk.Strings.copyStringToChars()");
      return 0;
  }
}
