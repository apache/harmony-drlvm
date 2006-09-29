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

import org.vmmagic.unboxed.*;
import org.apache.HarmonyDRLVM.mm.mmtk.*;
import org.mmtk.vm.*;
import org.mmtk.plan.Plan;
import org.mmtk.plan.CollectorContext;
import org.mmtk.utility.heap.LazyMmapper;
import org.mmtk.plan.MutatorContext;
////////////import org.mmtk.plan.marksweep.MS;
//////////import org.mmtk.plan.nogc.*;
import org.mmtk.plan.semispace.SS;
import org.mmtk.utility.options.*;
import org.mmtk.utility.Log;
import org.mmtk.utility.heap.*;

public class TestSemiSpace 
{
    static MutatorContext mc;
    static CollectorContext cc;
    public static void main(String[] args) 
    {
        VM vm_init = new VM();
        //NoGC pl2 = (NoGC)SelectedPlan.get();
        //MS pl2 = (MS)SelectedPlan.get();
        SS pl2 = (SS)SelectedPlan.get();
        Plan pl = SelectedPlan.ap.global();

        pl2.boot();
        pl.fullyBooted();
        org.apache.HarmonyDRLVM.mm.mmtk.SynchronizedCounter.boot();
        LazyMmapper.boot(Address.zero(), 0 );
        HeapGrowthManager.boot(Extent.fromInt(1024*1024*1), Extent.fromInt(1024*1024*1) ); //set the java heap very small to force collections

        TestSemiSpace.mc = SelectedPlan.ap.mutator();
        TestSemiSpace.cc = SelectedPlan.ap.collector();

        //System.out.println("TestSemiSpace SS.copySpace0.getName() = " + SS.copySpace0.getName() );
        //System.out.println("TestSemiSpace SS.copySpace1.getName() = " + SS.copySpace1.getName() );


        int xx = Integer.decode(args[0]);
        switch (xx)
        {
            case 1:
                test0();
                test1();
                test2();
                testWB();             
                break;
            case 2:
                singleRootTest();
                break;
            case 3:
                linkListOfLiveObjectsTest();
                break;
            case 4:
                linkListOfLiveObjectsTest2();
                break;
            default:
                System.out.println("******************** do nothing, invalid test number  ***********************");
                break;
        }
    }

    static final int arrayLenOffset = 8;
    static void test0()
    {
        Object [] arr = new Object[13];
        ObjectReference ref = ObjectReference.fromObject(arr);
        Address addr = ref.toAddress();
        Address lenAddr = addr.plus(arrayLenOffset);
        int len = lenAddr.loadInt();
        if (len != arr.length) 
        {
            throw new Error("length does not match");
        }
        System.out.println("test0 - ok");
    }
    static void test1()
    {
        Object [] arr = new Object[17];
        ObjectReference ref = ObjectReference.fromObject(arr);
        Address addr = ref.toAddress();
        Offset off = Offset.fromIntSignExtend(arrayLenOffset);
        int len = addr.loadInt(off);
        if (len != arr.length) 
        {
            throw new Error("length does not match");
        }
        System.out.println("test1 - ok");
    }

    static void test2()
    {
        Object obj = new Object();
        ObjectReference ref = ObjectReference.fromObject(obj);
        ObjectReferenceArray arr = ObjectReferenceArray.create(1);
        arr.set(0, ref);
        if (arr.get(0) != ref) 
        {
            throw new Error("Invalid ref read");
        }
        try 
        {
            // Even though these are magics, can't allow to 
            // write outside of array.
            arr.set(10, ref);
            // must not be here
            throw new Error("Bounds check was not performed");
        }
        catch(ArrayIndexOutOfBoundsException e) 
        {
            // ok
        }
        System.out.println("test2 - ok");
    }

    static Object wbFldStatic = null;
    Object  wbFldInstance = null;
    static void testWB()
    {
        Object obj = new Object();
        TestSemiSpace t = new TestSemiSpace();
        Object[] array = new Object[10];
        //
        wbFldStatic = obj;
        //
        t.wbFldInstance = obj;
        //
        array[3] = obj;
        //
        System.out.println("testWB - seems ok");
    }
    static void singleRootTest()
    {        
        int [] ia = new int[10];
        
        ObjectReference orIa = ObjectReference.fromObject(ia);
        Address addrIa = orIa.toAddress();
        int vtblPtr = addrIa.loadInt();
        Address addrLenIa = addrIa.plus(8);
        int length = addrLenIa.loadInt();
        if (length != 10) System.out.println("TestSemiSpace.java -- bad length -------------------" + length);

        Address addrImmortalObj = TestSemiSpace.mc.alloc(52, 0, 0, Plan.ALLOC_IMMORTAL, 0);
        addrImmortalObj.store(vtblPtr);
        Address addrLenImmortal = addrImmortalObj.plus(8);
        addrLenImmortal.store(length);
  
        Address addrImmortalObjElementZero = addrImmortalObj.plus(12); // stuff the _address_ of the root into element zero of the immortal array

        Address addrLiveObj = mc.alloc(52, 0, 0, Plan.ALLOC_DEFAULT, 0);
        addrLiveObj.store(vtblPtr);
        Address addrLenLive = addrLiveObj.plus(8);
        addrLenLive.store(length);

        addrImmortalObjElementZero.store(addrLiveObj);  // stuff the _address_ of the array into an element to test enumeration
        org.apache.HarmonyDRLVM.mm.mmtk.Scanning.addressOfTestRoot = addrImmortalObjElementZero.toInt();

        System.out.println("addressOfTestRoot = " + Integer.toHexString(addrImmortalObjElementZero.toInt() ) );
        System.out.println("the root itself is = " + Integer.toHexString(addrLiveObj.toInt()) );

        for (int kk = 0;  kk < 5; kk++)
        {
            addrLiveObj = addrImmortalObjElementZero.loadAddress();
            Address addrLiveElement3 = addrLiveObj.plus(12 + 3*4);
            addrLiveElement3.store(kk);

            for (int jj = 0; jj < 800000; jj++) 
            {
                Address addrDeadObj = mc.alloc(52, 0, 0, Plan.ALLOC_DEFAULT, 0);
                addrDeadObj.store(vtblPtr);
                Address addrLenDeadObj = addrDeadObj.plus(8);
                addrLenDeadObj.store(length);
            }

            addrLiveObj = addrImmortalObjElementZero.loadAddress();
            addrLiveElement3 = addrLiveObj.plus(12 + 3*4);
            int xx = addrLiveElement3.loadInt();
            if ( xx != kk  )
                System.out.println("kk = " + kk + " element 3 = " + xx  );
            System.out.println("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ single root test, loop = " + kk + " OK");
        }
    }
    static void linkListOfLiveObjectsTest()
    {
        Green gr = new Green();
        ObjectReference orGr = ObjectReference.fromObject(gr);
        Address addrGr = orGr.toAddress();
        int vtblPtrGreen = addrGr.loadInt();

        Address addrImmortalObj = TestSemiSpace.mc.alloc(52, 0, 0, Plan.ALLOC_IMMORTAL, 0);
        addrImmortalObj.store(vtblPtrGreen);
  
        Address addrImmortalObjFieldZero = addrImmortalObj.plus(8); // stuff the _address_ of the root into element zero of the immortal array

        Address addrLiveObj = mc.alloc(52, 0, 0, Plan.ALLOC_DEFAULT, 0);
        addrLiveObj.store(vtblPtrGreen);

        addrImmortalObjFieldZero.store(addrLiveObj);  // stuff the _address_ of the array into an element to test enumeration
        org.apache.HarmonyDRLVM.mm.mmtk.Scanning.addressOfTestRoot = addrImmortalObjFieldZero.toInt();

        System.out.println("addressOfTestRoot = " + Integer.toHexString(addrImmortalObjFieldZero.toInt() ) );
        System.out.println("the root itself is = " + Integer.toHexString(addrLiveObj.toInt()) );

        Object [] oa = new Object[10];
        ObjectReference orOa = ObjectReference.fromObject(oa);
        Address addrOa = orOa.toAddress();
        int vtblPtrOa = addrOa.loadInt();
        Address addrLenOa = addrOa.plus(8);
        int lengthOa = addrLenOa.loadInt();
        if (lengthOa != 10) System.out.println("TestSemiSpace.java -- bad length -------------------" + lengthOa);
        Address addrLiveArray = mc.alloc(52, 0, 0, Plan.ALLOC_DEFAULT, 0);
     
        addrLiveArray.store(vtblPtrOa);
        Address addrLiveArrayLen = addrLiveArray.plus(8);
        addrLiveArrayLen.store(lengthOa);

        ObjectReference orGreen = addrLiveObj.toObjectReference();
        Green oldGreen = (Green)orGreen.toObject();
        oldGreen.f8 = (Object [])addrLiveArray.toObjectReference().toObject();

        Address addrLeafGreen = mc.alloc(52, 0, 0, Plan.ALLOC_DEFAULT, 0);
        addrLeafGreen.store(vtblPtrGreen); 
        oldGreen.f8[3] = addrLeafGreen.toObjectReference().toObject();

        for (int kk = 0;  kk < 500; kk++)  //create 5K *52 = 250KB of live objects
        {
            Address addrNext = mc.alloc(52, 0, 0, Plan.ALLOC_DEFAULT, 0);
            addrNext.store(vtblPtrGreen);
            ObjectReference or2 = addrNext.toObjectReference();
            Green newGreen = (Green)or2.toObject();
            oldGreen.f1 = newGreen;
            oldGreen.f6 = kk;
            oldGreen = newGreen;
        }

        for (int jj = 0; jj < 150000; jj++) // generate lots of dead objects, cause lots of GCs
        {
            Address addrDeadObj = mc.alloc(52, 0, 0, Plan.ALLOC_DEFAULT, 0);
            addrDeadObj.store(vtblPtrGreen);
        }

        addrLiveObj = addrImmortalObjFieldZero.loadAddress();  //reload the lead object
        orGreen = addrLiveObj.toObjectReference();
        Green greenCursor = (Green)orGreen.toObject();   //everything from base object should always be reachable

        System.out.println("greenCursor.f8[3] = " + greenCursor.f8[3]);
        System.out.println("greenCursor.f8[4] = " + greenCursor.f8[4]);

        for (int rr = 0; rr < 500; rr++) 
        {
            if (greenCursor.f6 != rr) System.out.println("oldGreen.f6 = " + oldGreen.f6 + " rr = " + rr); //VM.assertions._assert(false);
            greenCursor = greenCursor.f1; 
        }
        System.out.println("greenCursor = " + greenCursor);
        System.out.println("linkListOfLiveObjectsTest, the live objects are still intact  OK");
    }
 
    static void linkListOfLiveObjectsTest2()
    {
        System.out.println("linkListOfLiveObjectsTest2, this test should fail real close to 1/2 the size of the java heap");
        Green gr = new Green();
        ObjectReference orGr = ObjectReference.fromObject(gr);
        Address addrGr = orGr.toAddress();
        int vtblPtrGreen = addrGr.loadInt();

        Address addrImmortalObj = TestSemiSpace.mc.alloc(52, 0, 0, Plan.ALLOC_IMMORTAL, 0);
        addrImmortalObj.store(vtblPtrGreen);
  
        Address addrImmortalObjFieldZero = addrImmortalObj.plus(8); // stuff the _address_ of the root into element zero of the immortal array

        Address addrLiveObj = mc.alloc(52, 0, 0, Plan.ALLOC_DEFAULT, 0);
        addrLiveObj.store(vtblPtrGreen);

        addrImmortalObjFieldZero.store(addrLiveObj);  // stuff the _address_ of the array into an element to test enumeration
        org.apache.HarmonyDRLVM.mm.mmtk.Scanning.addressOfTestRoot = addrImmortalObjFieldZero.toInt();

        System.out.println("addressOfTestRoot = " + Integer.toHexString(addrImmortalObjFieldZero.toInt() ) );
        System.out.println("the root itself is = " + Integer.toHexString(addrLiveObj.toInt()) );

        ObjectReference orGreen = addrLiveObj.toObjectReference();
        Green oldGreen = (Green)orGreen.toObject();

        for (int kk = 0;  kk < 50000000; kk++)  //intentionally grab the entire heap
        {
            Address addrNext = mc.alloc(52, 0, 0, Plan.ALLOC_DEFAULT, 0);
            addrNext.store(vtblPtrGreen);
            ObjectReference or2 = addrNext.toObjectReference();
            Green newGreen = (Green)or2.toObject();
            oldGreen.f1 = newGreen;
            oldGreen.f6 = kk;
            oldGreen = newGreen;
        }
    }

}
