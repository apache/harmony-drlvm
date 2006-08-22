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
import org.mmtk.vm.*;

public class Barriers extends org.mmtk.vm.Barriers implements Uninterruptible {
  /**
   * Perform the actual write of the write barrier.
   *
   * @param ref The object that has the reference field
   * @param slot The slot that holds the reference
   * @param target The value that the slot will be updated to
   * @param offset The offset from the ref (metaDataA)
   * @param locationMetadata An index of the FieldReference (metaDataB)
   * @param mode The context in which the write is occuring
   */
  public final void performWriteInBarrier(ObjectReference ref, Address slot, 
                                           ObjectReference target, Offset offset, 
                                           int locationMetadata, int mode) 
    throws InlinePragma {
    System.out.println("org.apache.HarmonyDRLVM.mm.mmtk.Barriers -- performWriteInBarrier was called" );
    //VM.assertions._assert(false);

    //wjw turn this on when we need it -----> Object obj = ref.toObject();
    //wjw    VM_Magic.setObjectAtOffset(obj, offset, target.toObject(), locationMetadata);  
  }

  /**
   * Atomically write a reference field of an object or array and return 
   * the old value of the reference field.
   * 
   * @param ref The object that has the reference field
   * @param slot The slot that holds the reference
   * @param target The value that the slot will be updated to
   * @param offset The offset from the ref (metaDataA)
   * @param locationMetadata An index of the FieldReference (metaDataB)
   * @param mode The context in which the write is occuring
   * @return The value that was replaced by the write.
   */
  public final ObjectReference performWriteInBarrierAtomic(
                                           ObjectReference ref, Address slot,
                                           ObjectReference target, Offset offset,
                                           int locationMetadata, int mode)
    throws InlinePragma { 
     /*  wjw -- turn this on when we get to write barrier debug                         
    Object obj = ref.toObject();
    Object newObject = target.toObject();
    Object oldObject;
    do {
      oldObject = VM_Magic.prepareObject(obj, offset);
    } while (!VM_Magic.attemptObject(obj, offset, oldObject, newObject));
    return ObjectReference.fromObject(oldObject); 
    */
    System.out.println("org.apache.HarmonyDRLVM.mm.mmtk.Barriers -- performWriteInBarrierAtomic was called" );
    //VM.assertions._assert(false);
    return ref;  // keep the compiler happy
  }

  /**
   * Sets an element of a char array without invoking any write
   * barrier.  This method is called by the Log method, as it will be
   * used during garbage collection and needs to manipulate character
   * arrays without causing a write barrier operation.
   *
   * @param dst the destination array
   * @param index the index of the element to set
   * @param value the new value for the element
   */
  public final void setArrayNoBarrier(char [] dst, int index, char value) {
    setArrayNoBarrierStatic(dst, index, value);
  }
  private static boolean oneShot = false;
  public static final void setArrayNoBarrierStatic(char [] dst, int index, char value) {
      if (oneShot == false) 
      {
          System.out.println("org.apache.HarmonyDRLVM.mm.mmtk.Barriers.setArrayNoBarrier() -- needs fixing" );
          oneShot = true;
      }
      dst[index] = value;
      //VM.assertions._assert(false);
  }

  /**
   * Gets an element of a char array without invoking any read barrier
   * or performing bounds check.
   *
   * @param src the source array
   * @param index the natural array index of the element to get
   * @return the new value of element
   */
  public final char getArrayNoBarrier(char [] src, int index) {
    return getArrayNoBarrierStatic(src, index);
  }
  
  private static boolean oneShot2 = false;
  public static final char getArrayNoBarrierStatic(char [] src, int index) {
      if (oneShot2 == false ) 
      {
          System.out.println("org.apache.HarmonyDRLVM.mm.mmtk.Barriers.getArrayNoBarrier()" );
          //VM.assertions._assert(false);
          oneShot2 = true;
      }
      return src[index];
  }

  /**
   * Gets an element of a byte array without invoking any read barrier
   * or bounds check.
   *
   * @param src the source array
   * @param index the natural array index of the element to get
   * @return the new value of element
   */
  public final byte getArrayNoBarrier(byte [] src, int index) {
    return getArrayNoBarrierStatic(src, index);
  }
  public static final byte getArrayNoBarrierStatic(byte [] src, int index) {
      System.out.println("org.apache.HarmonyDRLVM.mm.mmtk.Barriers.getArrayNoBarrier()" );
      return src[index];
  }

  /**
   * Gets an element of an int array without invoking any read barrier
   * or performing bounds checks.
   *
   * @param src the source array
   * @param index the natural array index of the element to get
   * @return the new value of element
   */
  public final int getArrayNoBarrier(int [] src, int index) {
      System.out.println("org.apache.HarmonyDRLVM.mm.mmtk.Barriers.getArrayNoBarrier()" );
      //VM.assertions._assert(false);
      return src[index];
  }

  /**
   * Gets an element of an Object array without invoking any read
   * barrier or performing bounds checks.
   *
   * @param src the source array
   * @param index the natural array index of the element to get
   * @return the new value of element
   */
  public final Object getArrayNoBarrier(Object [] src, int index) {
      System.out.println("org.apache.HarmonyDRLVM.mm.mmtk.Barriers.getArrayNoBarrier()" );
      //VM.assertions._assert(false);
      return src[index];
  }
  

  /**
   * Gets an element of an array of byte arrays without causing the potential
   * thread switch point that array accesses normally cause.
   *
   * @param src the source array
   * @param index the index of the element to get
   * @return the new value of element
   */
  public final byte[] getArrayNoBarrier(byte[][] src, int index) {
    return getArrayNoBarrierStatic(src, index);
  }
  public static final byte[] getArrayNoBarrierStatic(byte[][] src, int index) {
      System.out.println("org.apache.HarmonyDRLVM.mm.mmtk.Barriers.getArrayNoBarrierStatic()" );
      //VM.assertions._assert(false);
      return src[index];
  }
}
