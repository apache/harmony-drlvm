======================================
         Apache Harmony DRLVM
======================================

DRLVM is one of the virtual machines of the Apache Harmony 
project. It contains: 
 
    - VM (VM Core)
    - GC
    - JIT
    - Bytecode Verifier 
    - Class Libraries (Kernel Classes only)
    - OS Layer

See http://wiki.apache.org/harmony for a definition of these components.

At the time of original initial donation it could do the following with
Harmony classes:

    - Run Eclipse, version 3.1.1: create, edit, compile, and launch Java* applications
    - Provide a self-hosting development environment: the JRE can build itself
 
The supported configurations are Windows* IA-32 and Linux* IA-32. 

 
0. QUICK START
--------------

For prerequisites and instructions on how to build DRLVM, refer to 
Getting Started for Contributors 
[http://harmony.apache.org/quickhelp_contributors.html]. 

  
1. DRLVM SOURCE TREE CONTENTS
-----------------------------
 
This source tree consists of the source files, the building environment,
and the smoke tests source files for lightweight testing of the
DRLVM.

The structure is as follows: 

 <EXTRACT_DIR>/trunk/
        |
        +---build          - Files required to build the contribution
        |      
        \---vm             - VM source files
            |
            +- MMTk        - Garbage collector written in Java that is well recognized in 
            |                the JVM research community
            |
            +- doc         - DRLVM Developer's Guide and Getting Started guide
            |
            +- em          - Execution Manager component responsible for dynamic optimization
            |
            +- gc_cc       - Stop-the-world adaptive copying/slide compacting garbage
            |                collector with dynamic algorithm switching
            |
            +- gc_gen      - Generational garbage collector
            |
            +- gcv4        - Mark-compact garbage collector
            |
            +- include     - Set of header files containing external specification
            |                and inter-component interfaces
            |
            +- interpreter - Interpreter component
            |
            +- jitrino     - Just-in-time Compiler component
            |
            +- launcher    - Main function responsible for starting the VM as an application
            |
            +- port        - OS and platform porting layer component that, together with
            |                APR, provides an unified interface to low-level system routines 
            |                across different platforms
            |
            +- tests       - Tests source files
            |
            +- thread      - Thread manager (TM) library aimed to provide threading capabilities 
            |                for Java virtual machines
            |
            +- vmcore      - Core component responsible for class loading and resolution,
            |                kernel classes, JNI and JVMTI support, stack support, threading
            |                support, exception handling, verifier, and a set of other services
            |                and utilities
            |
            +- vmi         - Component responsible for compatibility with the Harmony class
            |                libraries 
            |
            \- vmstart     - Partial implementation of the invocation API for starting
                             the VM as a dynamic library  
 
 
2. TOOLS AND ENVIRONMENT VARIABLES REQUIRED FOR THE BUILD
---------------------------------------------------------

For detailed information on tools and environment variabled required for the build, refer to  
to Getting Started for Contributors
[http://harmony.apache.org/quickhelp_contributors.html].


3. BUILDING VM
--------------

The current section contains the description of the resulting 
source tree after the build and gives details on supplementary 
tasks related to the build. 

For detailed instructions on how to build DRLVM, 
refer to Getting Started for Contributors
[http://harmony.apache.org/quickhelp_contributors.html].

The build produces a set of .jar files, native libraries, and
support files that constitute the executable JRE. These files 
are placed in the following directory tree structure:

./build/${OS}_ia32_${CXX}_${BUILD_CFG}
    |
    \---deploy
          |
          \---jre
               |
               +---bin
               |    |
               |    +--- java.exe      (if Windows*)
               |    +--- *.dll         (if Windows*)
               |    +--- *.pdb         (if Windows*)
               |    +--- java.bat      (if Windows*)
               |    +--- eclipse.bat   (if Windows*)
               |    |
               |    +--- java          (if Linux*)
               |    +--- *.so          (if Linux*)
               |    +--- *.a           (if Linux*)
               |    +--- java.sh       (if Linux*)
               |    +--- eclipse.sh    (if Linux*)
               |    |
               |    \--- Hello.class
               |
               +---doc
               |    |
               |    +--- DeveloperGuide.htm
               |    +--- GettingStarted.htm
               |    +--- drl.css
               |    \--- images
               |          |
               |          \--- *.gif
               |
               +---lib
               |    |
               |    +--- org.apache.harmony.eclipse.jdt.launching_1.0.0.jar
               |    +--- boot
               |          |
               |          +--- archive.jar
               |          +--- bcprov-jdk14-129.jar
               |          +--- beans.jar
               |          +--- bootclasspath.properties
               |          +--- crypto.jar
               |          +--- icu4j_3_4.jar
               |          +--- icu4jni-3.4.jar
               |          +--- kernel.jar
               |          +--- logging.jar
               |          +--- luni.jar
               |          +--- math.jar
               |          +--- nio.jar
               |          +--- nio_char.jar
               |          +--- regex.jar
               |          +--- resolver.jar
               |          +--- security.jar
               |          +--- serializer.jar
               |          +--- sql.jar
               |          +--- text.jar
               |          +--- x_net.jar
               |          +--- xalan.jar
               |          +--- xercesImpl.jar
               |          +--- xml-apis.jar
               | 
               +---include
               |    |
               |    +--- jni.h
               |    +--- jni_types.h
               |    +--- jvmti.h
               |    \--- jvmti_types.h
               |
               \--- README.txt

              
You can now run DRLVM on the command line or under Eclipse*. For details on 
how to run the VM under Eclipse*, see Getting Started with DRLVM, 
[http://harmony.apache.org/subcomponents/drlvm/getting_started.html].  

 3.1 Build the selected components. 
     
      You can build any component indicated in the file
     ./build/make/deploy.xml.
    
 3.2 Clean up the previously built code by executing one of the following:
    
      On  Windows*:     | On Linux*:
      ------------------+---------------
      build.bat clean   | build.sh clean 
 
 3.3 Update the network resources as required. 
    
      The build uses external resources from a network, such as APR or Harmony classes.
      These are downloaded and stored locally during the first launch of the build. 

      3.3.1 Change the resource location settings by editing the property file:
       
             On  Windows*:            | On Linux*:
            --------------------------+--------------------------
            build\make\win.properties | build/make/lnx.properties              

            Example:
            The build searches for the Eclipse* archive in its default location on the web:
            remote.ECLIPSE.archieve=http://eclipse.objectweb.org/downloads/drops/R-3.1.1-200509290840/eclipse-SDK-3.1.1-win32.zip
            You can specify the Eclipse* installation directory by setting the ECLIPSE_HOME 
            environment variable.  
       
      3.3.2 Update the local version of external resources.
    
            To download a fresh copy of the external resources, type the following:

      
            On Windows*:        | On Linux*:
            --------------------+----------------
            build.bat update    | build.sh update
     
            External resources are stored in the directory:
      
            build/pre-copied
      
      
 3.4 Clean up the local copy of the external resources by executing the following:
      
     On Windows*:             | On Linux*:
     -------------------------+----------------------
     build.bat clean.update   | build.sh clean.update
              
     NOTE: Because you have executed the target 'update' previously, 
     this ./build.sh  takes the locally stored resources for building. 

4. RUNNING DRLVM WITH EXTERNAL CLASS LIBRARIES
----------------------------------------------
 
To run DRLVM with third-party external class libraries, do the following:

1. Check that these class libraries comply with the kernel classes interface 
   and the VMI interfaces, see the description in the directory
   <EXTRACT_DIR>/Harmony/vm/vmcore/src/kernel_classes. 

2. Add external native dynamic libraries (*.dll or *.so files) to the system path or copy 
   the files into the <EXTRACT_DIR>/deploy/jre/bin directory. 

3. Add external library class directories or .jar files into the -Xbootclasspath option. 

   Example:
   ij -Xbootclasspath/p:c:\external_library\lib\classes.jar MyApp
 

5. BUILDING AND RUNNING TESTS
-----------------------------

To build and run the DRLVM tests, execute the following command:

   On Windows*:    |  On Linux*:
   ----------------+----------------
   build.bat test  |  build.sh test


The test classes are placed in
<EXTRACT_DIR>/Harmony/make/%BUILD_CFG%_Windows_ia32_%NATIVE_COMPILER%/semis/vm/test/classes.
The test results are placed in
<EXTRACT_DIR>/Harmony/make/%BUILD_CFG%_Windows_ia32_%NATIVE_COMPILER%/semis/vm/test/report.


6. TROUBLESHOOTING
-------------------
 
For build troubleshooting information, refer to the Wiki page: 
http://wiki.apache.org/harmony/DrlvmBuildTroubleshooting


7. TODO
--------

For information on TODO issues, refer to the Wiki page:
http://wiki.apache.org/harmony/TODO_List_for_DRLVM  
