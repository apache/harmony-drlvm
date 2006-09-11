INTEL CONTRIBUTION TO APACHE HARMONY
          August 17, 2006
======================================
 
This archive contains the contribution to the Apache Harmony project from
Intel. The contribution contains a new garbage collector to be used with DRLVM.

The garbage collector, GC V4.1 is a single-threaded
stop-the-world adaptive copying / slide compacting collector with dynamic
algorithm switching. 

GC V4.1 was developed by Ivan Volosyuk and is roughly one third the size of GC
V4. It addresses specific performance problems of GC V4.  For example, there
are fewer:
 * full heap passes (1 pass in the common case)
 * atomic exchanges during GC
 * fewer bit shifts.

GC V4.1 has basic implementations of:
 * weak references
 * finalizible objects
 * object pinning

The design of GC V4.1 is similar to: "P. Sansom. Dual-Mode Garbage Collection.
In Third Int. Workshop on the Parallel Implementation of Functional Languages,
pages 238 -- 310, University  Southampton, U.K., September 1991."  This design
was chosen for expedient development.

Basic GC terminology and concepts in this collector are from:
"Paul R. Wilson. Uniprocessor garbage collection techniques. In Proc of
International Workshop on Memory Management in the Springer-Verlag Lecture
Notes in Computer Science series., St. Malo, France, September 1992"

GC V4.1 is a drop-in replacement for GC V4. In limited testing, GC V4.1
delivers better performance than GC V4 on UP and SMP hardware.

GC V4 functions that were reused include spin lock implementation,
parameter parsing code, class preparation, fast list.

0. QUICK START
--------------
In the <DRLVM> root directory, replace the gc/ directory with the content
of the archive by executing the following commands:

  cd incubator/harmony/enhanced/drlvm/trunk/vm
  rm -rf gc
  unzip gcv4.1.zip

Build and use DRLVM as usual.

1. ARCHIVE CONTENTS
-------------------
Archive contains the GC V4.1 source files. 

Supported architectures: Linux*/IA-32, Windows*/IA-32.

The layout of archive is following:
  <gcv4.1.zip>
   src/*.cpp, *.h  - C++ sources of GC V4.1


2. TOOLS AND ENVIRONMENT VARIABLES REQUIRED FOR THE BUILD
---------------------------------------------------------
This collector requires the Harmony class libraries and DRLVM. 
Please, follow the usual build process for DRLVM to build this module as a part
of DRLVM. Tools and environment variable are exactly the same as for DRLVM.

3. BUILDING GC V4.1
------------------
Build GC V4.1 as a part of DRLVM by doing the following:
  
  3.1 Replace GC V4 with GC V4.1.
      Remove the gc/ directory from <DRLVM>/vm/gc. Unpack the archive into 
      the <DRLVM>/vm directory. The gc/ directory appears there.

  3.2 Follow the usual DRLVM build process. Consult <DRLVM>/README.txt for
      details.

4. RUNNING GC V4.1
-----------------
You can use the new GC V4.1 collector with DRLVM just as GC V4.

5. BUILDING AND RUNNING TESTS
-----------------------------
All existing tests for DRLVM should work OK.

Note: 
    The test gc.LOS from DRLVM smoke tests is known to have issues with GC.
    The test verifies that the space available for large objects is not less
    then that for small objects. GC V4.1 does not differentiate between small
    and large objects but contains an optimization for the out-of-heap
    condition: OutOfMemoryError can be thrown even when some space is still
    available on the heap, which causes the test to fail. Exclude gc.LOS
    from the test run until it is fixed.

6. KNOWN ISSUES
---------------
Detailed descriptions of the algorithms used is missing.
Synchronization on simultaneous pinning and hashcode creation is not yet
implemented. Per object recursion counter is limited by 7 iterations.
At present, the system will abort on pin counter overflow.

7. TODO
-------
Try additional benchmarks, investigate performance problems.

8. DISCLAIMER AND LEGAL INFORMATION
-----------------------------------
*) Other brands and names are the property of their respective owners.



