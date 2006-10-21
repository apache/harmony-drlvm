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
import org.mmtk.plan.nogc.*;
import org.mmtk.plan.Plan;
import org.mmtk.plan.CollectorContext;
// toss import org.mmtk.utility.heap.Mmapper;
import org.mmtk.plan.MutatorContext;
import org.mmtk.plan.marksweep.MS;
import org.mmtk.utility.options.*;
import org.mmtk.utility.Log;
import org.mmtk.utility.heap.*;

public class TestMarkSweep 
{
    static MutatorContext mc;
    static CollectorContext cc;
    public static void main(String[] args) 
    {
        VM vm_init = new VM();
        //NoGC pl2 = (NoGC)SelectedPlan.get();
        MS pl2 = (MS)SelectedPlan.get();
        Plan pl = SelectedPlan.ap.global();

        pl2.boot();
        pl.fullyBooted();
        org.apache.HarmonyDRLVM.mm.mmtk.SynchronizedCounter.boot();
        // toss Mmapper.boot(Address.zero(), 0 );
        HeapGrowthManager.boot(Extent.fromInt(1024*1024*1), Extent.fromInt(1024*1024*1) ); //set the java heap very small to force collections

        TestMarkSweep.mc = SelectedPlan.ap.mutator();
        TestMarkSweep.cc = SelectedPlan.ap.collector();


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
            case 5:
                linkListOfArraysTest();
                break;
            default:
                System.out.println("do nothing");
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
        TestMarkSweep t = new TestMarkSweep();
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
        Address addr = null;
        int [] ia = new int[10];
        
        ObjectReference or = ObjectReference.fromObject(ia);
        Address addr2 = or.toAddress();
        int vtblPtr = addr2.loadInt();
        Address addr3 = addr2.plus(8);
        int length = addr3.loadInt();
        if (length != 10) System.out.println("TestMarkSweep.java -- bad length -------------------" + length);
        Address addrElementZero = null;

        for (int kk = 10;  kk < 1000000; kk++)
        {
            addr = TestMarkSweep.mc.alloc(52, 0, 0, Plan.ALLOC_DEFAULT, 0);

            addr.store(vtblPtr);
            Address addrLen = addr.plus(8);
            addrLen.store(length);
            ObjectReference or2 = addr.toObjectReference();

            Object obj2 = or2.toObject();

            if ( (kk % 200000) == 0) System.out.println("TestMarkSweep.singleRootTest(), obj2 = " + obj2 + " " + obj2.hashCode() );
        }
        addrElementZero = addr.plus(12);
        addrElementZero.store(addr);  // stuff the _address_ of the array into an element to test enumeration
 
        org.apache.HarmonyDRLVM.mm.mmtk.Scanning.addressOfTestRoot = addrElementZero.toInt();
        System.out.println("addressOfTestRoot = " + Integer.toHexString(addrElementZero.toInt() ) );
        System.out.println("the root itself is = " + Integer.toHexString(addr.toInt()) );

        or = addr.toObjectReference();
         
        ia = (int []) (or.toObject() );

        for (int kk = 0;  kk < 5; kk++)
        {
            ia[3] = ia[4] = ia[5] = ia[6] = ia[7] = ia[8] = kk;
            for (int jj = 0; jj < 800000; jj++) 
            {
                addr = mc.alloc(52, 0, 0, Plan.ALLOC_DEFAULT, 0);
                addr.store(vtblPtr);
                Address addrLen = addr.plus(8);
                addrLen.store(length);
                ObjectReference or2 = addr.toObjectReference();
                Object obj2 = or2.toObject();
            }
            System.out.println("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ single root test, loop = " + kk + " OK");
            if ( (ia[3] != kk) | (ia[4] != kk) | (ia[5] != kk) | (ia[6] != kk) | (ia[7] != kk) | (ia[8] != kk)  )
                VM.assertions._assert(false);
        }
    }
    static void linkListOfLiveObjectsTest()
    {
        Green gr = new Green();
        ObjectReference orx = ObjectReference.fromObject(gr);
        Address addrx = orx.toAddress();
        int vtblPtrGreen = addrx.loadInt();

        Address addrBaseObject = TestMarkSweep.mc.alloc(52, 0, 0, Plan.ALLOC_DEFAULT, 0);

        addrBaseObject.store(vtblPtrGreen);
 
        Address addrGreenFieldZero = addrBaseObject.plus(8);
        addrGreenFieldZero.store(addrBaseObject);  // stuff the _address_ of the array into field zero to test enumeration
        org.apache.HarmonyDRLVM.mm.mmtk.Scanning.addressOfTestRoot = addrGreenFieldZero.toInt();

        System.out.println("addressOfTestRoot = " + Integer.toHexString(addrGreenFieldZero.toInt() ) );
        System.out.println("the root itself is = " + Integer.toHexString(addrBaseObject.toInt()) );

        ObjectReference orBaseObject = addrBaseObject.toObjectReference();         
        Green oldGreen = (Green)(orBaseObject.toObject() );  //everything from base object should always be reachable

        for (int kk = 0;  kk < 5000; kk++)  //create 5K *52 = 250KB of live objects
        {
            Address addrNext = mc.alloc(52, 0, 0, Plan.ALLOC_DEFAULT, 0);
            addrNext.store(vtblPtrGreen);
            ObjectReference or2 = addrNext.toObjectReference();
            Green newGreen = (Green)or2.toObject();
            oldGreen.f1 = newGreen;
            oldGreen.f6 = kk;
            oldGreen = newGreen;
        }

        oldGreen = null;  // don't let gcv4 see these ref ptrs!
        System.out.println("linkListOfLiveObjectsTest, allocated 250KB worth of live objects");

        for (int zz = 0; zz < 10; zz++) //allocate a bunch of objects, throw them away to force collections
        {
            for (int kk = 0;  kk < 20000; kk++)
            {
                Address addr4 = mc.alloc(52, 0, 0, Plan.ALLOC_DEFAULT, 0);
                addr4.store(vtblPtrGreen);
            }
            //System.out.println("linkListOfLiveObjectsTest, allocated and discarded " + zz + "x100KB");
        }
        //check if the 50KB of linked objects are still intact       

        oldGreen = (Green)(orBaseObject.toObject() );  //everything from base object should always be reachable
        for (int rr = 0; rr < 1000; rr++) 
        {
            if (oldGreen.f6 != rr) System.out.println("oldGreen.f6 = " + oldGreen.f6 + " rr = " + rr); //VM.assertions._assert(false);
            oldGreen = oldGreen.f1; 
        }
        System.out.println("linkListOfLiveObjectsTest, the live objects are still intact  OK");
    }
 
    static void linkListOfLiveObjectsTest2()
    {
        Green gr = new Green();
        ObjectReference or = ObjectReference.fromObject(gr);
        Address addr2 = or.toAddress();
        int vtblPtr = addr2.loadInt();

        Address addr = TestMarkSweep.mc.alloc(52, 0, 0, Plan.ALLOC_DEFAULT, 0);

        addr.store(vtblPtr);
 
        Address addrFieldZero = addr.plus(8);
        addrFieldZero.store(addr);  // stuff the _address_ of the array into field zero to test enumeration
        org.apache.HarmonyDRLVM.mm.mmtk.Scanning.addressOfTestRoot = addrFieldZero.toInt();

        //System.out.println("addressOfTestRoot = " + Integer.toHexString(addrFieldZero.toInt() ) );
        //System.out.println("the root itself is = " + Integer.toHexString(addr.toInt()) );

        or = addr.toObjectReference();         
        Green baseGreen = (Green)(or.toObject() );  //everything from baseGreen should always be reachable
        Green oldGreen = baseGreen;  

        System.out.println("linkListOfLiveObjectsTest2, this test should fail real close to the size of the java heap");

        for (int zz = 0; zz < 10; zz++)
        {
            for (int kk = 0;  kk < 2000; kk++)
            {
                addr = mc.alloc(52, 0, 0, Plan.ALLOC_DEFAULT, 0);
                addr.store(vtblPtr);
                ObjectReference or2 = addr.toObjectReference();
                Green newGreen = (Green)or2.toObject();
                oldGreen.f1 = newGreen;
                oldGreen = newGreen;
            }
            System.out.println("linkListOfLiveObjectsTest2, allocated and retaining " + zz + "x100KB");
        }
    }

    static void linkListOfArraysTest()
    {
        Green gr = new Green();
        ObjectReference orx = ObjectReference.fromObject(gr);
        Address addrx = orx.toAddress();
        int vtblPtrGreen = addrx.loadInt();

        Address addrBaseObject = TestMarkSweep.mc.alloc(52, 0, 0, Plan.ALLOC_DEFAULT, 0);

        addrBaseObject.store(vtblPtrGreen);
 
        Address addrGreenFieldZero = addrBaseObject.plus(8);
        addrGreenFieldZero.store(addrBaseObject);  // stuff the _address_ of the array into field zero to test enumeration
        org.apache.HarmonyDRLVM.mm.mmtk.Scanning.addressOfTestRoot = addrGreenFieldZero.toInt();

        System.out.println("addressOfTestRoot = " + Integer.toHexString(addrGreenFieldZero.toInt() ) );
        System.out.println("the root itself is = " + Integer.toHexString(addrBaseObject.toInt()) );

        ObjectReference orBaseObject = addrBaseObject.toObjectReference();         
        Green oldGreen = (Green)(orBaseObject.toObject() );  //everything from base object should always be reachable

        for (int kk = 0;  kk < 5000; kk++)  //create 5K *52 = 250KB of live objects
        {
            Address addrNext = mc.alloc(52, 0, 0, Plan.ALLOC_DEFAULT, 0);
            addrNext.store(vtblPtrGreen);
            ObjectReference or2 = addrNext.toObjectReference();
            Green newGreen = (Green)or2.toObject();
            oldGreen.f1 = newGreen;
            oldGreen.f6 = kk;
            oldGreen = newGreen;
        }

        Object oaq [] = new Object[10];        
        ObjectReference orq = ObjectReference.fromObject(oaq);
        Address addrq = orq.toAddress();
        oaq = null;  //aggressively null out unused objects (to keep gcv4 from finding them!)
        orq = null;

        //allocate one array of objects and link onto the first green
        int vtblPtrOA = addrq.loadInt();
        Address addrLen = addrq.plus(8);
        int length = addrLen.loadInt();
        if (length != 10) System.out.println("test.java -- bad length -------------------" + length);

        Address addrOA = TestMarkSweep.mc.alloc(52, 0, 0, Plan.ALLOC_DEFAULT, 0);
        addrOA.store(vtblPtrOA);
        Address addrOALen = addrOA.plus(8);
        addrOALen.store(length);

        oldGreen = (Green)(orBaseObject.toObject() );
        oldGreen.f8 = (Object []) addrOA.toObjectReference().toObject();
        System.out.println("linkListOfLiveObjectsTest, allocated 250KB worth of live objects");
        //stuff an object in the array at index 5 (or 12 + 5*4 offset from ref ptr)
        Address addrElement5 = addrOA.plus(32);
        addrElement5.store(addrBaseObject.toInt() );

        oldGreen = null;  // don't let gcv4 see these ref ptrs!

        for (int zz = 0; zz < 10; zz++) //allocate a bunch of objects, throw them away to force collections
        {
            for (int kk = 0;  kk < 30000; kk++)
            {
                Address addr4 = mc.alloc(52, 0, 0, Plan.ALLOC_DEFAULT, 0);
                addr4.store(vtblPtrGreen);
            }
            //System.out.println("linkListOfLiveObjectsTest, allocated and discarded " + zz + "x100KB");
        }
        //check if the 50KB of linked objects are still intact       

        oldGreen = (Green)(orBaseObject.toObject() );  //everything from base object should always be reachable
               System.out.println("oldGreen.f8 = " + oldGreen.f8);
               System.out.println("oldGreen.f8[5] = " + oldGreen.f8[5]);
        for (int rr = 0; rr < 1000; rr++) 
        {
            if (oldGreen.f6 != rr) System.out.println("oldGreen.f6 = " + oldGreen.f6 + " rr = " + rr); //VM.assertions._assert(false);
            oldGreen = oldGreen.f1; 
        }
        System.out.println("linkListOfArraysTest() is OK");
    }
}
