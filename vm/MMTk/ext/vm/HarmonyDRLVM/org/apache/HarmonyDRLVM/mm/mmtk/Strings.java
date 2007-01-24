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
        int zz = (int)c[xx];
        if (zz == 0) break;
      }
      System.out.println("");
      for (int xx=0; xx < c.length; xx++) c[xx] = 0;
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
          int zz = (int)c[xx];
          if (zz == 0) break;
      }
      System.out.println("");
      for (int xx=0; xx < c.length; xx++) c[xx] = 0;
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
   * @param dstBegin the start offset in the destination array
   * @param dstEnd the index after the last character in the
   * destination to copy to
   * @return the number of characters copied.
   */
  public final int copyStringToChars(String src, char [] dst,
                                     int dstBegin, int dstEnd)
    throws LogicallyUninterruptiblePragma {
  
    int len = src.length();
    int span = 0;
    if (dstBegin + span <= dstEnd) 
    {
        span = len;
    } else 
    { 
        span = dstEnd - dstBegin;
    }
    for (int xx = 0; xx < span; xx++) 
    {
        Barriers.setArrayNoBarrierStatic(dst, dstBegin + xx, src.charAt(xx) );
    }
    return span;
  }
}
