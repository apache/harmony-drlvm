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

import org.mmtk.plan.Plan;
import org.mmtk.policy.ImmortalSpace;
import org.mmtk.policy.Space;
import org.mmtk.vm.VM;
import org.vmmagic.unboxed.*;
import org.vmmagic.pragma.*;
import org.mmtk.utility.Constants;

public class Memory extends org.mmtk.vm.Memory
  implements Constants, Uninterruptible {

    static protected int PHONY_JAVA_HEAP_SIZE = 1024 * 1024 * 450;
    static protected byte [] immortalPinnedScratchObject;  //wjw -- ugly hack, make it static so that it is always enumerated
    static protected int dangerousPointerToStartOfScratchArea;
    static protected int dangerousPointerToEndOfScratchArea;
    static 
    {
        immortalPinnedScratchObject = new byte[PHONY_JAVA_HEAP_SIZE];
        ObjectReference or = ObjectReference.fromObject(immortalPinnedScratchObject);
        Address addr = or.toAddress();
        addr = addr.plus(Space.BYTES_IN_CHUNK + 16);  // add 16 to skip over the object's header area
        dangerousPointerToStartOfScratchArea = addr.toWord().rshl(Space.LOG_BYTES_IN_CHUNK).lsh(Space.LOG_BYTES_IN_CHUNK).toInt();

        addr = or.toAddress();
        addr = addr.plus(PHONY_JAVA_HEAP_SIZE);
        dangerousPointerToEndOfScratchArea = addr.toInt();
        dangerousPointerToEndOfScratchArea = addr.toWord().rshl(Space.LOG_BYTES_IN_CHUNK).lsh(Space.LOG_BYTES_IN_CHUNK).toInt();
    }
 
    protected final Address getHeapStartConstant() // toss { return Address.fromInt(0x100); } //BOOT_IMAGE_DATA_START
    {
        Address heapStart = Address.fromInt(dangerousPointerToStartOfScratchArea);
        System.out.println("org.apache.HarmonyDRLVM.mm.mmtk.Memory.getHeapStartConstant() = " + Integer.toHexString(heapStart.toInt()) );
        return heapStart;
    }
    protected final Address getHeapEndConstant() // toss { return Address.fromInt(0xFFff0000); }  //MAXIMUM_MAPPABLE
    {
        Address heapEnd = Address.fromInt(dangerousPointerToEndOfScratchArea);
        System.out.println("org.apache.HarmonyDRLVM.mm.mmtk.Memory.getHeapEndConstant() = " + Integer.toHexString(heapEnd.toInt()) );
        return heapEnd;
    }
    protected final Address getAvailableStartConstant() 
    { 
        Address availableStart = Address.fromInt(dangerousPointerToStartOfScratchArea);
        System.out.println("org.apache.HarmonyDRLVM.mm.mmtk.Memory.getAvailableStartConstant() = " + Integer.toHexString(availableStart.toInt()) );
        return availableStart;
    }
    protected final Address getAvailableEndConstant() 
    {      
        Address availableEnd = Address.fromInt(dangerousPointerToEndOfScratchArea);
        System.out.println("org.apache.HarmonyDRLVM.mm.mmtk.Memory.getAvailableEndConstant() = " + Integer.toHexString(availableEnd.toInt() )); 
        return availableEnd;
    }
  protected final byte getLogBytesInAddressConstant() { return 2; }
  protected final byte getLogBytesInWordConstant() { return 2; }
  protected final byte getLogBytesInPageConstant() { return 12; }
  protected final byte getLogMinAlignmentConstant() { return 2;}
  protected final byte getMaxAlignmentShiftConstant() { return 3; } //wjw -- I dont have a clue
  protected final int getMaxBytesPaddingConstant() { return 8; } 
  protected final int getAlignmentValueConstant() { return 0x01020304;}
 
  private static int BOOT_SEGMENT_MB = (0x800000>>LOG_BYTES_IN_MBYTE);

    public final ImmortalSpace getVMSpace() throws InterruptiblePragma
    {
        ImmortalSpace bootSpace = new ImmortalSpace("boot", Plan.DEFAULT_POLL_FREQUENCY, 
            BOOT_SEGMENT_MB, false);
        System.out.println("org.apache.HarmonyDRLVM.mm.mmtk.Memory.getVMSpace(), bootSpace = " + bootSpace);
        return bootSpace;
    }

    public final void globalPrepareVMSpace() 
    {
        //System.out.println("org.apache.HarmonyDRLVM.mm.mmtk.Memory.globalPrepareVMSpace() needs fixing");
    }

    public final void collectorPrepareVMSpace() 
    {
        //System.out.println("org.apache.HarmonyDRLVM.mm.mmtk.Memory.collectorPrepareVMSpace() needs fixing");
    }

    public final void collectorReleaseVMSpace() 
    {
        //System.out.println("org.apache.HarmonyDRLVM.mm.mmtk.Memory.collectorReleaseVMSpace() needs fixing");
    }
    public final void globalReleaseVMSpace() 
    {
        //System.out.println("org.apache.HarmonyDRLVM.mm.mmtk.Memory.globalReleaseVMSpace() needs fixing");
    }
    public final void setHeapRange(int id, Address start, Address end) 
    {
        System.out.println("org.apache.HarmonyDRLVM.mm.mmtk.Memory.setHeapRange() id = " + id + 
            " start = " + Integer.toHexString(start.toInt()) + 
            " end = " + Integer.toHexString(end.toInt())                );
    }
 /**
   * Maps an area of virtual memory.
   *
   * @param start the address of the start of the area to be mapped
   * @param size the size, in bytes, of the area to be mapped
   * @return 0 if successful, otherwise the system errno
   */
	
  public final int dzmmap(Address start, int size) {
  //MERGEWJW  public final int mmap(Address start, int size) {
      //System.out.println("org.apache.HarmonyDRLVM.mm.mmtk.Memory.mmap() needs fixing");
  
    return 0;
  }
  
  /**
   * Protects access to an area of virtual memory.
   *
   * @param start the address of the start of the area to be mapped
   * @param size the size, in bytes, of the area to be mapped
   * @return <code>true</code> if successful, otherwise
   * <code>false</code>
   */
  public final boolean mprotect(Address start, int size) {
    System.out.println("org.apache.HarmonyDRLVM.mm.mmtk.Memory.mprotect() needs fixing, start = " + Integer.toHexString(start.toInt())
     +   " size = " + size);
    return false; 
  }

  /**
   * Allows access to an area of virtual memory.
   *
   * @param start the address of the start of the area to be mapped
   * @param size the size, in bytes, of the area to be mapped
   * @return <code>true</code> if successful, otherwise
   * <code>false</code>
   */
  public final boolean munprotect(Address start, int size) {
    System.out.println("org.apache.HarmonyDRLVM.mm.mmtk.Memory.mprotect() needs fixing");
    return false; 
  }

  /**
   * Zero a region of memory.
   * @param start Start of address range (inclusive)
   * @param len Length in bytes of range to zero
   * Returned: nothing
   */
  public final void zero(Address start, Extent len) {
      byte zeroByte = 0;
      int numberOfBytes = len.toInt();
      for(int xx=0; xx < numberOfBytes; xx++) 
      {
        start.store(zeroByte);
        start.plus(1);
      }
  }

  /**
   * Zero a range of pages of memory.
   * @param start Start of address range (must be a page address)
   * @param len Length in bytes of range (must be multiple of page size)
   */
  public final void zeroPages(Address start, int len) {
      int zeroInt = 0;
      for(int xx=0; xx < len; len+=4) 
      {
          start.store(zeroInt);
          start.plus(4);
      }
  }

  /**
   * Logs the contents of an address and the surrounding memory to the
   * error output.
   *
   * @param start the address of the memory to be dumped
   * @param beforeBytes the number of bytes before the address to be
   * included
   * @param afterBytes the number of bytes after the address to be
   * included
   */
  public final void dumpMemory(Address start, int beforeBytes,
                                int afterBytes) {
      Address low = start.minus(beforeBytes);
      Address hi = start.plus(afterBytes);
      System.out.println("org.apache.HarmonyDRLVM.mm.mmtk.Memory.dumpMemory() called -------------------------");
      while (low.NE(hi) ) 
      {
        byte b1 = low.loadByte();
        System.out.print(b1 + " ");
        low.plus(1);
      }
      System.out.println();
      System.out.println("--------------------------------------------dumpMemory finished");
  }

  /*
   * Utilities from the VM class
   */

  public final void sync() throws InlinePragma {
    //System.out.println("org.apache.HarmonyDRLVM.mm.mmtk.Memory.sync() was called"); 
  }

  public final void isync() throws InlinePragma {
    //System.out.println("org.apache.HarmonyDRLVM.mm.mmtk.Memory.isync() was called");
  }
}
