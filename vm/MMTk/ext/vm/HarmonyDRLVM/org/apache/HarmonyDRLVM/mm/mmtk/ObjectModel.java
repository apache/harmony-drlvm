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

import org.mmtk.utility.scan.MMType;
import org.mmtk.utility.Constants;
import org.vmmagic.unboxed.*;
import org.vmmagic.pragma.*;
import org.mmtk.vm.*;
import org.mmtk.policy.Space;
import org.mmtk.plan.MutatorContext;
import org.mmtk.utility.alloc.Allocator;
import org.mmtk.plan.Plan;
import org.mmtk.plan.CollectorContext;

public final class ObjectModel extends org.mmtk.vm.ObjectModel implements Constants, Uninterruptible {
  /**
   * Copy an object using a plan's allocCopy to get space and install
   * the forwarding pointer.  On entry, <code>from</code> must have
   * been reserved for copying by the caller.  This method calls the
   * plan's <code>getStatusForCopy()</code> method to establish a new
   * status word for the copied object and <code>postCopy()</code> to
   * allow the plan to perform any post copy actions.
   *
   * @param from the address of the object to be copied
   * @return the address of the new object
   */
  public ObjectReference copy(ObjectReference from, int allocator)
    throws InlinePragma {
      int objSize = 0;     
      Class clsObj = from.getClass();
      boolean isArr = clsObj.isArray();
      if (isArr) 
      {
          String clsName = clsObj.toString();
          VM.assertions._assert(clsName.charAt(6) == '[');
          int elementSize = 0;
          if (clsName.charAt(7) == 'B') elementSize = 1;
          if (clsName.charAt(7) == 'C') elementSize = 1;
          if (clsName.charAt(7) == 'D') elementSize = 8;
          if (clsName.charAt(7) == 'F') elementSize = 4;
          if (clsName.charAt(7) == 'I') elementSize = 4;
          if (clsName.charAt(7) == 'J') elementSize = 8;
          if (clsName.charAt(7) == 'L') elementSize = 4;
          if (clsName.charAt(7) == 'S') elementSize = 2;
          if (clsName.charAt(7) == 'Z') elementSize = 1;
          if (clsName.charAt(7) == '[') elementSize = 4;
          VM.assertions._assert(elementSize != 0);
          VM.assertions._assert( (clsName.charAt(7) == 'I') || (clsName.charAt(7) == 'L') ); //need to align the byte array properly
          Address addrObj = from.toAddress();
          int arrayLenOffset = 8;
          Address lenAddr = addrObj.plus(arrayLenOffset);
          int len = lenAddr.loadInt();
          int sizeOfArrayHeader = 12;  //4 bytes for vtablePtr, 4 for lock bits, 4 for array length
          objSize = sizeOfArrayHeader + len * elementSize;      
      }
      else 
      {
          Address addrFrom = from.toAddress();
          int vtblPtr = addrFrom.loadInt();
          Address addrVPtr = Address.fromInt(vtblPtr);
          int vPtr = addrVPtr.loadInt();
          Address addrVPtr_0 = Address.fromInt(vPtr);
          int slot_0 = addrVPtr_0.loadInt();
          int slot_24 = addrVPtr_0.plus(24).loadInt();
          //System.out.println("slot_24 = " + Integer.toHexString(slot_24) );
          objSize = slot_24;
      }
      //System.out.println("ObjectModel.copy(), allocator = " + allocator);
      //VM.assertions._assert(allocator == 0);
      CollectorContext cc = SelectedPlan.ap.collector();
      Address addrTo = cc.allocCopy(from, objSize,
          0, /*int align,*/  0, /*int offset,*/ allocator) ;
      Address addrFrom = from.toAddress();
            //System.out.println("ObjectModel.copy(), objSize = " + objSize + " addrFrom = " +
                //Integer.toHexString(addrFrom.toInt()) + " addrTo = " + Integer.toHexString(addrTo.toInt()) );
      Address addrCursor = addrTo;
      for (int xx = 0; xx < objSize; xx++) 
      {
            byte bb = addrFrom.loadByte();
                        //System.out.print("_" + Integer.toHexString((int)bb));
            addrCursor.store(bb);
            addrCursor = addrCursor.plus(1);
            addrFrom = addrFrom.plus(1);
      }
                        //System.out.println("");
      // mask off the GC bits (both forwarding and mark bits)
      Address addrGCBits = addrTo.plus(4);
      int yy = addrGCBits.loadInt();
      // the following does not work because of "private" token -- Word ww = org.mmtk.policy.CopySpace.GC_FORWARDED;
      yy = yy & 0xffFFffFc;  //ugly! but where is MMTk #defines for these bits?
      addrGCBits.store(yy);
      return addrTo.toObjectReference();
  }

   

  /**
   * Copy an object to be pointer to by the to address. This is required 
   * for delayed-copy collectors such as compacting collectors. During the 
   * collection, MMTk reserves a region in the heap for an object as per
   * requirements found from ObjectModel and then asks ObjectModel to 
   * determine what the object's reference will be post-copy.
   * 
   * @param from the address of the object to be copied
   * @param to The target location.
   * @param region The start (or an address less than) the region that was reserved for this object.
   * @return Address The address past the end of the copied object
   */
  public Address copyTo(ObjectReference from, ObjectReference to, Address region)
    throws InlinePragma {

      //System.out.println("wjw org.apache.HarmonyDRLVM.mm.mmtk.ObjectModel.copyTo() was called");
      VM.assertions._assert(false);
      return Address.fromInt(0);
  }

  /**
   * Return the reference that an object will be referred to after it is copied
   * to the specified region. Used in delayed-copy collectors such as compacting
   * collectors.
   *
   * @param from The object to be copied.
   * @param to The region to be copied to.
   * @return The resulting reference.
   */
  public ObjectReference getReferenceWhenCopiedTo(ObjectReference from, Address to) {
    //System.out.println("wjw org.apache.HarmonyDRLVM.mm.mmtk.ObjectModel.getReferenceWhenCopiedTo() was called");
    VM.assertions._assert(false);
    return from;  // keep the compiler happy -
  }
  
  /**
   * Gets a pointer to the address just past the end of the object.
   * 
   * @param object The object.
   */
  public Address getObjectEndAddress(ObjectReference object) {
    //System.out.println("wjw org.apache.HarmonyDRLVM.mm.mmtk.ObjectModel.getObjectEndAddress() was called");
    VM.assertions._assert(false);
    return Address.fromInt(0);  // keep the compiler happy
  }
  
  /**
   * Return the size required to copy an object
   *
   * @param object The object whose size is to be queried
   * @return The size required to copy <code>obj</code>
   */
  public int getSizeWhenCopied(ObjectReference object) {
    //need to use drlvm's get_object_size_bytes()
    //System.out.println("wjw org.apache.HarmonyDRLVM.mm.mmtk.ObjectModel.getSizeWhenCopied() was called");
    VM.assertions._assert(false);
    return 0;  
  }
  
  /**
   * Return the alignment requirement for a copy of this object
   *
   * @param object The object whose size is to be queried
   * @return The alignment required for a copy of <code>obj</code>
   */
  public int getAlignWhenCopied(ObjectReference object) {

    //System.out.println("wjw org.apache.HarmonyDRLVM.mm.mmtk.ObjectModel.getAlignWhenCopied() was called");
    VM.assertions._assert(false);
    return 0;
  }
  
  /**
   * Return the alignment offset requirements for a copy of this object
   *
   * @param object The object whose size is to be queried
   * @return The alignment offset required for a copy of <code>obj</code>
   */
  public int getAlignOffsetWhenCopied(ObjectReference object) {
      //System.out.println("wjw org.apache.HarmonyDRLVM.mm.mmtk.ObjectModel.getAlignOffsetWhenCopied() was called");
      VM.assertions._assert(false);
      return 0;
  }
  
  /**
   * Return the size used by an object
   *
   * @param object The object whose size is to be queried
   * @return The size of <code>obj</code>
   */
  public int getCurrentSize(ObjectReference object) {
      //System.out.println("wjw org.apache.HarmonyDRLVM.mm.mmtk.ObjectModel.getCurrentSize() was called");
      VM.assertions._assert(false);
      return 0;
  }

  /**
   * Return the next object in the heap under contiguous allocation
   */
  public ObjectReference getNextObject(ObjectReference object) {
    //System.out.println("wjw org.apache.HarmonyDRLVM.mm.mmtk.ObjectModel.getNextObject() was called");
    VM.assertions._assert(false);
    return ObjectReference.fromObject(null);
  }

  /**
   * Return an object reference from knowledge of the low order word
   */
  public ObjectReference getObjectFromStartAddress(Address start) {
    //System.out.println("wjw org.apache.HarmonyDRLVM.mm.mmtk.ObjectModel.getObjectFromStartAddress() was called");
	  ObjectReference or = start.toObjectReference();
	  Object obj = or.toObject();
	  return or;
  }
  
  /**
   * Get the type descriptor for an object.
   *
   * @param ref address of the object
   * @return byte array with the type descriptor
   */
  public byte [] getTypeDescriptor(ObjectReference ref) {
    //System.out.println("wjw org.apache.HarmonyDRLVM.mm.mmtk.ObjectModel.getTypeDescriptor() was called");
    VM.assertions._assert(false);
    return new byte[10]; 
  }

  public int getArrayLength(ObjectReference object) 
    throws InlinePragma {
     
      Class clsObj = object.getClass();
      boolean isArr = clsObj.isArray();
      if (isArr == false) System.out.println("getArrayLength() -- isArr = false");
      VM.assertions._assert(isArr == true);     
      Address addr2 = object.toAddress();
      int vtblPtr = addr2.loadInt();
      Address addr3 = addr2.plus(8);
      int length = addr3.loadInt();
      if (length != 10) System.out.println("getArrayLength() -- length = " + length);
      VM.assertions._assert(length == 10);
    return length;

  }
  /**
   * Tests a bit available for memory manager use in an object.
   *
   * @param object the address of the object
   * @param idx the index of the bit
   */
  public boolean testAvailableBit(ObjectReference object, int idx) {
    //System.out.println("wjw org.apache.HarmonyDRLVM.mm.mmtk.ObjectModel.testAvailableBit() was called");
    VM.assertions._assert(false);
    return false;
  }

  /**
   * Sets a bit available for memory manager use in an object.
   *
   * @param object the address of the object
   * @param idx the index of the bit
   * @param flag <code>true</code> to set the bit to 1,
   * <code>false</code> to set it to 0
   */
  public void setAvailableBit(ObjectReference object, int idx,
                                     boolean flag) {
    //System.out.println("wjw org.apache.HarmonyDRLVM.mm.mmtk.ObjectModel.setAvailableBit() was called");
    VM.assertions._assert(false);
    return;
  }

  /**
   * Attempts to set the bits available for memory manager use in an
   * object.  The attempt will only be successful if the current value
   * of the bits matches <code>oldVal</code>.  The comparison with the
   * current value and setting are atomic with respect to other
   * allocators.
   *
   * @param object the address of the object
   * @param oldVal the required current value of the bits
   * @param newVal the desired new value of the bits
   * @return <code>true</code> if the bits were set,
   * <code>false</code> otherwise
   */
  public boolean attemptAvailableBits(ObjectReference object,
                                             Word oldVal, Word newVal) {
     //System.out.print("wjw org.apache.HarmonyDRLVM.mm.mmtk.ObjectModel.attemptAvailableBits() ");
     //System.out.println("object = " + Integer.toHexString(object.toAddress().toInt()) +
     //   " oldVal = " + Integer.toHexString(oldVal.toInt() ) + " newVal = " + Integer.toHexString(newVal.toInt() ) );
    
      Address addr = object.toAddress();
      addr = addr.plus(4);  // wjw -- 
      int xx = addr.loadInt();
      VM.assertions._assert(xx == oldVal.toInt());  // wjw temporary single thread hack, need real atomic ops
      addr.store(newVal.toInt());
          //VM.assertions._assert(false); 
    return true;
  }

  /**
   * Gets the value of bits available for memory manager use in an
   * object, in preparation for setting those bits.
   *
   * @param object the address of the object
   * @return the value of the bits
   */
  public Word prepareAvailableBits(ObjectReference object) {
    //System.out.print("org.apache.HarmonyDRLVM.mm.mmtk.ObjectModel.prepareAvailableBits(), object = " +
    //    Integer.toHexString(object.toAddress().toInt()) );

    Address addr = object.toAddress();
    addr = addr.plus(4);  // wjw -- 
    int xx = addr.loadInt();
     // System.out.println(" return val = " + Integer.toHexString(xx) );
    //VM.assertions._assert(false);
    return Word.fromInt(xx);
  }

  /**
   * Sets the bits available for memory manager use in an object.
   *
   * @param object the address of the object
   * @param val the new value of the bits
   */
  public void writeAvailableBitsWord(ObjectReference object, Word val) {
    //System.out.println("org.apache.HarmonyDRLVM.mm.mmtk.ObjectModel.attemptAvailableBits(), object ="
    //    + Integer.toHexString(object.toAddress().toInt()) +
    //    " val = " + Integer.toHexString(val.toInt())   );
    Address addrRefPtr = object.toAddress();
    addrRefPtr = addrRefPtr.plus(4);  // wjw --
    addrRefPtr.store(val.toInt() );
    return;
  }

  /**
   * Read the bits available for memory manager use in an object.
   *
   * @param object the address of the object
   * @return the value of the bits
   */
  public Word readAvailableBitsWord(ObjectReference object) {
      //System.out.println("org.apache.HarmonyDRLVM.mm.mmtk.ObjectModel.readAvailableBitsWord(), object ="
        //  + Integer.toHexString(object.toAddress().toInt()) );
      Address addrRefPtr = object.toAddress();
      addrRefPtr = addrRefPtr.plus(4);  // wjw --
      Word wor = addrRefPtr.loadWord();
      //System.out.println("org.apache.HarmonyDRLVM.mm.mmtk.ObjectModel.readAvailableBitsWord(), wor =" + Integer.toHexString(wor.toInt()) );
      return wor;
  }

  /**
   * Gets the offset of the memory management header from the object
   * reference address.  XXX The object model / memory manager
   * interface should be improved so that the memory manager does not
   * need to know this.
   *
   * @return the offset, relative the object reference address
   */
  /* AJG: Should this be a variable rather than method? */
  public Offset GC_HEADER_OFFSET() {
    //System.out.println("wjw org.apache.HarmonyDRLVM.mm.mmtk.ObjectModel.GC_HEADER_OFFSET() was called");
    VM.assertions._assert(false);
    return Offset.fromInt(0);
  }

  /**
   * Returns the lowest address of the storage associated with an object.
   *
   * @param object the reference address of the object
   * @return the lowest address of the object
   */
  public Address objectStartRef(ObjectReference object)
    throws InlinePragma {
        //System.out.println("wjw org.apache.HarmonyDRLVM.mm.mmtk.ObjectModel.objectStartRef() was called");

    VM.assertions._assert(false);
    return Address.fromInt(0);
  }

  /**
   * Returns an address guaranteed to be inside the storage associated
   * with and object.
   *
   * @param object the reference address of the object
   * @return an address inside the object
   */
    private static int tripCount = 0;

  public Address refToAddress(ObjectReference object) {
      Address addr = object.toAddress();
      int xx = addr.toInt();
      String ss = Integer.toHexString(xx);
    //System.out.println("wjw org.apache.HarmonyDRLVM.mm.mmtk.ObjectModel.refToAddress() was called " + ss);
    //if (tripCount == 1) VM.assertions._assert(false);
      tripCount++;
    return addr.plus(4);  //wjw -- I don't know if what the ref ptr points at "qualifies", hopefully the lock bits does...
  }

  /**
   * Checks if a reference of the given type in another object is
   * inherently acyclic.  The type is given as a TIB.
   *
   * @return <code>true</code> if a reference of the type is
   * inherently acyclic
   */
  public boolean isAcyclic(ObjectReference typeRef) 
  {
      //System.out.println("wjw org.apache.HarmonyDRLVM.mm.mmtk.ObjectModel.isAcyclic() was called");
      VM.assertions._assert(false);
      return false;
  }
    
    private static Class [] clsArray = new Class[10];
    private static MMType [] mmtArray = new MMType[10];

    private static MMType getObjTypeLookup(Class clsObj)
    {
        for (int xx = 0; xx < 10; xx++) 
        {
            if (clsArray[xx] == clsObj) 
            {
                return mmtArray[xx];
            }
        }
        return null;
    }

    private static void getObjectTypeCacheInsert(Class clsObj, MMType mmt)
    {
        int xx;
        for (xx = 0; xx < 10; xx++)
        {
            if (clsArray[xx] == clsObj) return;
            if (clsArray[xx] == null) break;
        }
        VM.assertions._assert(xx<10);
        VM.assertions._assert(clsArray[xx] == null);
        VM.assertions._assert(mmtArray[xx] == null);
        clsArray[xx] = clsObj;
        mmtArray[xx] = mmt;
        //System.out.println("getObjTypeCacheInsert(), inserting  " + clsObj);
    }
  /**
   * Return the type object for a give object
   *
   * @param object The object whose type is required
   * @return The type object for <code>object</code>
   */
  public MMType getObjectType(ObjectReference object) 
    throws InlinePragma {
                    Object obj = object.toObject();
                    //System.out.println("wjw org.apache.HarmonyDRLVM.mm.mmtk.ObjectModel.getObjectType() was called " + obj);
                    //VM.assertions._assert(false);
      boolean isDelegated = false;  //scanning is *not* delegated to the VM
      
      Class clsObj = object.getClass();
      MMType mmtc = getObjTypeLookup(clsObj);
      if (mmtc != null) return mmtc;

      boolean isArr = clsObj.isArray();
      boolean isRefArray = false;
      if (isArr) 
      {
          String clsName = clsObj.toString();
          //System.out.println("getObjectType() 3:" + clsName  + "_6:" + clsName.charAt(6) +
          //    "_7:" + clsName.charAt(7) );
          VM.assertions._assert(clsName.charAt(6) == '[');
          if (clsName.charAt(7) == '[') isRefArray = true;
          if (clsName.charAt(7) == 'L') isRefArray = true;
      }
      boolean isAcyclic = false;  //wjw -- I don't have a clue what this is...

      Space spx = Space.getSpaceForObject(object);
      MutatorContext mc = SelectedPlan.ap.mutator();
      Allocator alc = mc.getAllocatorFromSpace(spx);  //but alloc is an "int, not an "Allocator".  Go figure...
  
      int [] offs = new int [0];
      /////888888888888888888888888888888
      if (isArr == false)   //build the correct offset table
          {
              ObjectReference or = ObjectReference.fromObject(object);
              Address addr2 = or.toAddress();
              int vtblPtr = addr2.loadInt();
              Address addrVPtr = Address.fromInt(vtblPtr);
              int vPtr = addrVPtr.loadInt();
              //System.out.println("AddrVptr = " + Integer.toHexString(addrVPtr.toInt()) );
              //System.out.println("vPtr = " + Integer.toHexString(vPtr) );
              Address addrVPtr_0 = Address.fromInt(vPtr);
              int vPtr_0 = addrVPtr_0.loadInt();
              //System.out.println("vPtr_0 = " + Integer.toHexString(vPtr_0) );
              int vPtr_2c = addrVPtr_0.plus(0x2c).loadInt();
              //System.out.println("vPtr_2c = " + Integer.toHexString(vPtr_2c) );
              Address addrElementZero = Address.fromInt(vPtr_2c);
              int xx = 0;
              for (xx = 0; xx < 12; xx++ ) 
              {
                  int element = addrElementZero.plus(xx*4).loadInt();
                  //System.out.println("element [" + xx + "] = " + element );
                  if (element == 0) break;
              }
              if (xx > 10) VM.assertions._assert(false); // we ran off in the woods
              if (xx > 0) 
              {
                  offs = new int[xx];
                  for (int yy = 0; yy < xx; yy++) 
                  {
                      int element = addrElementZero.plus(yy*4).loadInt();
                      offs[yy] = element;            
                  }
                  for (int rr = 0; rr < offs.length; rr++) 
                  {
                      //System.out.println("offs[" + rr + "] = " + offs[rr] );
                  } 
              }
          }
      ////////88888888888888888888888
      //System.out.println("offs length = " + offs.length + " isRefArray = " + isRefArray);
      MMType mmt = new MMType(isDelegated, isRefArray, isAcyclic, Plan.ALLOC_DEFAULT, offs);
      getObjectTypeCacheInsert(clsObj, mmt);
      return mmt;
  }

	public Offset getArrayBaseOffset()
	{
		//// toss VM.assertions._assert(false);
		Offset ofs = Offset.fromInt(12);
		return ofs;
	}

	static Offset arrayBaseOffsetTrapdoor(ObjectModel o)
	{
		return o.getArrayBaseOffset();
	}
}

