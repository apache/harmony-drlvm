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
 * @author Vera Volynets
 * @version $Revision: 1.6.22.3 $
 */  
 
package perf;

import java.util.ArrayList;

/** This is a simple program that allocates objects of 30kb and 60kb.
 * This test should stress gc_v4 (mark-compact) algorithm
 * because off "corner" sizes of allocated objects(30kb and 60kb). Set heap size 128Mb. 
 * Garbage is interleaved with small amount of live objects. Live objects are saved in massive.
 *
 * @keyword perf gc  
 */
public class StressCornerSize_a {
    static class CornerObject {
        byte array[];
	CornerObject next;
	CornerObject(int size)
	{
	    array=new byte[size*1024];
	}
    }

    public static void main(String[] args) {
      	long itime=System.currentTimeMillis();
	ArrayList al = new ArrayList();
        int iterations = 30000;
	CornerObject corner;
		
	for (int i = 0; i < iterations; i++) {
            if((i % 2) == 0) {
	        corner = new CornerObject(45);
	    }else{
	        corner = new CornerObject(25);
	    }
	    if((i != 0) && ((i % 16) == 0)) {
	        al.add(corner);
	    }
	}
	System.out.println("The test run: "+(System.currentTimeMillis()-itime)+" ms\n");
	System.out.println ("PASSED");
    }	
}

