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
import org.apache.HarmonyDRLVM.mm.mmtk.*;
import org.mmtk.vm.*;

public class test {
	public static void main(String[] args) {
        VM vm_init = new VM();
        System.out.println("top of test wjw------");
		test0();
		test1();
		test2();
		testWB();
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

}