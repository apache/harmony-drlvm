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
/**
 * Few simple tests for magics Wb4J support in Jitrino.JET.
 *
 * @author Alexander V. Astapchuk
 * @version $Revision$
 */
import org.vmmagic.unboxed.*;
import org.apache.HarmonyDRLVM.mm.mmtk.*;  ///org.apache.HarmonyDRLVM.mm.mmtk
import org.mmtk.vm.*;
import org.mmtk.plan.nogc.*;
import org.mmtk.plan.Plan;
import org.mmtk.plan.CollectorContext;
import org.mmtk.utility.heap.LazyMmapper;
import org.mmtk.plan.MutatorContext;
import org.mmtk.plan.marksweep.MS;
import org.mmtk.utility.options.*;
import org.mmtk.utility.Log;
import org.mmtk.utility.heap.*;

public class test 
{
	public static void main(String[] args) {
        VM vm_init = new VM();
        MS pl2 = (MS)SelectedPlan.get();
        // toss -- moved to SelectedPlan org.apache.HarmonyDRLVM.mm.mmtk.ActivePlan ap = new org.apache.HarmonyDRLVM.mm.mmtk.ActivePlan();
        Plan pl = SelectedPlan.ap.global();

        pl2.boot();
        pl.fullyBooted();
        org.apache.HarmonyDRLVM.mm.mmtk.SynchronizedCounter.boot();
        LazyMmapper.boot(Address.zero(), 0 );
        HeapGrowthManager.boot(Extent.fromInt(1024*1024*1), Extent.fromInt(1024*1024*1) ); //see Memory.java

        MutatorContext mc = SelectedPlan.ap.mutator();
        CollectorContext cc = SelectedPlan.ap.collector();

        Address addr = null;
        //Address addr = mc.alloc(64, 0, 0, 0, 0);
        //System.out.println("addr = " + Integer.toHexString(addr.toInt() ) );
        
        //NoGC nogc = new NoGC();
        //NoGCMutator mc = (NoGCMutator)VM.activePlan.mutator(0);    //getNextMutator();
        //System.out.println("test NoGC, mc = " + mc);
        //NoGCMutator nm = new NoGCMutator();

        System.out.println("top of test wjw------");
        test0();
        test1();
        test2();
        testWB();
        //System.out.println("verbose = " + org.mmtk.utility.options.Options.verbose.getValue() );

        int [] ia = new int[10];
        ObjectReference or = ObjectReference.fromObject(ia);
        Address addr2 = or.toAddress();
        int vtblPtr = addr2.loadInt();
        Address addr3 = addr2.plus(8);
        int length = addr3.loadInt();
        if (length != 10) System.out.println("test.java -- bad length -------------------" + length);
        
        for (int kk = 0;  kk < 10; kk++)
        {
            addr = mc.alloc(52, 0, 0, Plan.ALLOC_DEFAULT, 0);
            //addr = mc.alloc(1024, 0, 0, Plan.ALLOC_LOS, 0);

            addr.store(vtblPtr);
            Address addrLen = addr.plus(8);
            addrLen.store(length);
            ObjectReference or2 = addr.toObjectReference();

            Object obj2 = or2.toObject();

            //System.out.println(obj2 + " " + obj2.hashCode() );
        }
        Address addrElementZero = addr.plus(12);
        addrElementZero.store(addr);  // stuff the _address_ of the array into an element to test enumeration
        org.apache.HarmonyDRLVM.mm.mmtk.Scanning.addressOfTestRoot = addrElementZero.toInt();
        System.out.println("addressOfTestRoot = " + Integer.toHexString(addrElementZero.toInt() ) );
        System.out.println("the root itself is = " + Integer.toHexString(addr.toInt()) );

        or = addr.toObjectReference();
        ia = (int []) (or.toObject() );

        for (int kk = 0;  kk < 1024*1024*1024; kk++)
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
            System.out.println("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ test loop iteration = " + kk);
            if ( (ia[3] != kk) | (ia[4] != kk) | (ia[5] != kk) | (ia[6] != kk) | (ia[7] != kk) | (ia[8] != kk)  )
                VM.assertions._assert(false);
        }

        //for (long ii = 0; ii < 0x00FFffFFffFFffFFL; ii++) // for it to run out of memory, see if error msg is correct
        {
            //addr = nm.alloc(64, 8, 0, NoGC.ALLOC_DEFAULT, 0);
            //System.out.println("test NoGC alloc --- " + Integer.toHexString(addr.toInt()) );
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
		if (len != arr.length) {
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
		if (len != arr.length) {
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
		if (arr.get(0) != ref) {
			throw new Error("Invalid ref read");
		}
		try {
			// Even though these are magics, can't allow to 
			// write outside of array.
			arr.set(10, ref);
			// must not be here
			throw new Error("Bounds check was not performed");
		}
		catch(ArrayIndexOutOfBoundsException e) {
			// ok
		}
		System.out.println("test2 - ok");
	}

	static Object wbFldStatic = null;
	Object  wbFldInstance = null;
	static void testWB()
	{
		Object obj = new Object();
		test t = new test();
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
//// from DRLVM gc_header.h
//// 
}