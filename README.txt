INTEL CONTRIBUTION TO APACHE HARMONY
          May 2, 2006
======================================

This archive contains the contribution to the Apache 
Harmony project from Intel. The contribution consists 
of the following components: 

    - VM (VM Core)
    - GC
    - JIT
    - Bytecode Verifier 
    - Class Libraries (Kernel Classes only)
    - OS Layer

See http://wiki.apache.org/harmony for a definition of these components.

This donation can do the following with Harmony classes:

   - Run Eclipse* version 3.1.1: create, edit, compile, and launch Java* applications
   - Provide a self-hosting development environment: the JRE can build itself

The supported configurations are Windows* IA-32 and Linux* IA-32. 

0. QUICK START
--------------
This section gives brief instructions on how to build DRLVM on Windows* with
the standard configuration. For more detailed instructions, bundle content description, 
and other useful information, read further. 

1. Unzip DRLVM_src_*_*_Harmony.zip and Patches_for_Harmony_*.zip  in the same directory.

2. Set the following environment variables:
   ANT_HOME must point to the location of Apache Ant.  
   JAVA_HOME must point to the location of the Java* Runtime Environment. 

   NOTE: All paths must be absolute. 

3. Change the working directory to Harmony/build.

4. Run the following command:

        build.bat update -Dhttp.proxyHost=proxy -Dhttp.proxyPort=8080

   The class libraries and other sources are downloaded or checked out from the 
   Internet at this stage. Make sure that the SVN and Ant Internet settings are correct. 
   See steps 3.12.3 and 3.12.4 below for more information.

5. Run the following command:

        build.bat -DBUILD_CFG=release -DCXX=msvc

6. win_ia32_msvc_release/deploy/jre/bin/ij.exe -version
   This directory $OS_$ARCH_$CXX_$BUILD_CFG/deploy/jre contains DRLVM you have just built.

7. If building the DRLVM fails, read this README and follow building instructions to the point.

1. ARCHIVE CONTENTS
-------------------

The archive contains the source files, the building environment,
and the smoke tests source files for lightweight testing of the provided
implementation.

After extracting the archive, the following directories appear under
<EXTRACT_DIR>/Harmony, where <EXTRACT_DIR> is the location, to which
you have extracted this archive:

<EXTRACT_DIR>/Harmony/
       |
       +---build          - Files required to build the contribution
       |      
       \---vm             - VM source files
           |
           +- doc         - DRLVM Developer's Guide and Getting Started guide
           |
           +- em          - Execution Manager component responsible for dynamic optimization
           |
           +- gc          - Garbage Collector component responsible for allocation and
           |                reclamation of Java* objects in the heap
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
           +- vmcore      - Core component responsible for class loading and resolution,
           |                kernel classes, JNI and JVMTI support, stack support, threading
           |                support, exception handling, verifier, and a set of other services
           |                and utilities
           |
           +- vmi         - Component responsible for compatibility with the Harmony class
           |                libraries 
           |
           +- vmstart     - Partial implementation of the invocation API for starting
                            the VM as a dynamic library 

NOTE: There are two patches for Harmony CLASSLIB and Harmony Eclipse plug-in that
 distributed separately in file harmony_for_vm_patches.zip. 
 It should be unpacked in <EXTRACT_DIR>. New directory Harmony/build/patches 
 will be created. See harmony_for_vm_patches.zip README.txt for more details. 

2. TOOLS AND ENVIRONMENT VARIABLES REQUIRED FOR THE BUILD
-----------------------------------------------------------

To build the Java*, C/C++ and assembler source files of DRLVM, install the following software and tools:

+ Java* Runtime      - Apache Harmony Execution Environment
                       http://www-128.ibm.com/developerworks/java/jdk/harmony

+ Class libraries    - Harmony Class Libraries
                       https://svn.apache.org/repos/asf/incubator/harmony/enhanced/classlib/trunk

+ Apache Ant         - Apache Ant, version 1.6.5 or higher
                       http://ant.apache.org

+ C/C++ compiler     - on Windows*, use one of the following: 

                       + Microsoft* 32-bit C/C++ Compiler, version 7 or higher
                       + Windows* platform SDK
                       + Microsoft* Visual Studio .NET* 2003 or higher
                         http://www.microsoft.com/products
                         
                       + Intel C++ Compiler, version 9.0
                         http://www.intel.com/cd/software/products/asmo-na/eng/compilers/index.htm

+ Subversion         - Subversion, version 1.3.0 or higher
                       http://subversion.tigris.org/servlets/ProjectDocumentList?folderID=91

+ libxml2 on Linux   - libxml2 package 

Additionally, the building system will download
the following software and libraries from the Internet:

+ HARMONY-39         - The beans, regex, and math contribution
                       http://issues.apache.org/jira/browse/HARMONY-39

+ HARMONY-88         - The jndi, logging, prefs and sql contribution
                       http://issues.apache.org/jira/browse/HARMONY-39

+ Xalan-Java         - Xalan-Java, version 2.7.0
                       http://xml.apache.org/xalan-j/ 

+ Eclipse*           - Eclipse* SDK, version 3.1.1
                       http://download.eclipse.org/eclipse/downloads/

+ Cpp Tasks          - Cpp Tasks collection, version 1.0 beta 3 or higher
                       http://ant-contrib.sourceforge.net/
                       http://sourceforge.net/project/showfiles.php?group_id=36177

+ Ant-Contrib        - Ant-Contrib collection of tasks, version 0.6 or higher
                       http://ant-contrib.sourceforge.net/
                       http://sourceforge.net/project/showfiles.php?group_id=36177

+ Zlib               - Zlib library, binaries, version 1.2.1 or higher
                       http://www.zlib.net

+ APR                - Apache Portable Runtime, version 1.2.6
                       http://apr.apache.org/download.cgi

+ APR-util           - APR-util version 1.2.6
                       http://apr.apache.org/download.cgi

+ APR-iconv          - APR-iconv version 1.1.1
                       http://apr.apache.org/download.cgi

+ Log4cxx            - latest log4cxx, SVN revision 365029
                       http://svn.apache.org/repos/asf/logging/log4cxx/trunk


NOTE: Your PATH environment variable must include the location of SVN tool set.
      It is recommended that SVN is run manually at least once prior to build
      since it may ask to accept the certificates from Apache site in an interactive
      manner. See 3.12.4 for more details. 
      
      Steps 3.12.3 and 3.12.4 describe how proxy settings could be configured.

Alternatively, you can download these resources prior to building DRLVM, unpack these,
and specify the resulting location by using the environment variables, as described
in step 3.12.1. 

3. BUILDING DRLVM
-----------------

The DRLVM build has been designed as a complete self-hosting environment. 
However, in order to bootstrap, prepare the Harmony JRE first, 
as described below:

3.1 Download and install the software mentioned in section 2.

3.2 Set up the Harmony execution environment as a bootstrap environment for building DRLVM:

    3.2.1 Build the Harmony Class Libraries by typing 'ant' in the make/ directory. See README.txt
          for the Harmony class libraries for more details and troubleshooting.

    3.2.2 Copy the contents of the deploy/jre directory of the class libraries into the deploy/jre
          directory of the Harmony execution environment. 

    3.2.3 Copy files xalan.jar, xercesImpl.jar, xml-apis.jar and serializer.jar from the Xalan-Java distribution 
          into the deploy/jre/lib/boot directory.

    3.2.4 Add the following line into deploy/jre/lib/boot/bootclasspath.properties file:
          bootclasspath.38=serializer.jar

    3.2.5 Download the Beans, Regex, and Math contribution from
          http://issues.apache.org/jira/browse/HARMONY-39.

    3.2.6 Build the contribution according to the instructions
          supplied with the package. 

    3.2.7 Copy the obtained regex.jar, math.jar and beans.jar into the deploy/jre/lib/boot directory. 

    3.2.8 On Linux*, you will need to setup the LD_LIBRARY_PATH to point to the deploy/jre/bin directory.


3.3 Set up the environment by following the instructions below specific for your operating system:
    
    On Windows*, do the following:
    
    set ANT_HOME=<Path to Ant 1.6.5>
    set JAVA_HOME=<Path to JRE 1.4>    
    set COMPILER_CFG_SCRIPT=<path to vcvars.bat or iclvars.bat>
    The default value: C:\Program Files\Intel\Compiler\C++\9.0\IA32\Bin\iclvars.bat>
              
    On Linux*, do the following:
    
    export JAVA_HOME=<Path to JRE 1.4>
    export ANT_HOME=<Path to Ant 1.6.5>
    export LD_LIBRARY_PATH=<Path to ICC libs>
    Example: export LD_LIBRARY_PATH="/opt/intel/cc/9.0/lib:$LD_LIBRARY_PATH"

    NOTE: All paths must be absolute. 

3.4 Change the working directory to <EXTRACT_DIR>/Harmony/build.

3.5 Download the libraries required for the build, specifically:
    
     On Windows*:         |  On Linux*:
     ---------------------+--------------------
     build.bat update     |  build.sh update
 

3.6 Run the build by starting:
    
     On Windows*:         |  On Linux*:
     ---------------------+--------------------
     build.bat            |  build.sh
                                         

3.7 You can switch between the different compilers and modes by using 
    the environment variables, as follows:
   
                            On Windows*:             |  On Linux*:
                            -------------------------+----------------------------
    MSVC compiler     :     set CXX=msvc           # |  N/A     
    Intel(R) compiler :     set CXX=icl              |  export CXX=icc 
    GCC compiler      :     N/A                      |  export CXX=gcc           # 
    Release           :     set BUILD_CFG=release  # |  export BUILD_CFG=release #
    Debug             :     set BUILD_CFG=debug      |  export BUILD_CFG=debug   
   
      # - default values
    
    NOTE: All paths must be absolute. 
      
3.8 Locate the resulting binary image. 
   
    In case of a successful build, the operating JRE is placed at:
   
    ./build/${OS}_ia32_${CXX}_${BUILD_CFG}/deploy/jre/
   
    Where OS=(win|lnx) is determined automatically, 
    CXX and BUILD_CFG are the values you have set.

3.9 Run your application. 

    To run an application called Hello, go to the directory 
    ./build/${OS}_ia32_${CXX}_${BUILD_CFG}/deploy/jre/bin and type:
         
    On Windows*:        |  On Linux*:
    --------------------+--------------------
    ij.exe Hello        | LD_LIBRARY_PATH=$PWD; export LD_LIBRARY_PATH
                        | ./ij Hello

    On Linux*, the convenience script ij.sh configures LD_LIBRARY_PATH 
    automatically. If you have generated the executable with the Intel(R) C++ compiler,
    add path to ICC libraries to LD_LIBRARY_PATH too, for example: 

        LD_LIBRARY_PATH="$PWD:/opt/intel/cc/9.0/lib:$LD_LIBRARY_PATH"
        export LD_LIBRARY_PATH
        ./ij Hello

    To run Eclipse*, set the ECLIPSE_HOME variable to point to the Eclipse* 
    installation directory and type:

    On Windows*:        |  On Linux*:
    --------------------+--------------------
    eclipse.bat         |  eclipse.sh

    The build produces a set of .jar files, native libraries, and
    support files that constitute the executable JRE. These files are in
    the following directory tree structure:

    ./build/${OS}_ia32_${CXX}_${BUILD_CFG}
        |
        \---deploy
              |
              \---jre
                   |
                   +---bin
                   |    |
                   |    +--- ij.exe      (if Windows*)
                   |    +--- *.dll       (if Windows*)
                   |    +--- *.pdb       (if Windows*)
                   |    +--- ij.bat      (if Windows*)
                   |    +--- eclipse.bat (if Windows*)
                   |    |
                   |    +--- ij          (if Linux*)
                   |    +--- *.so        (if Linux*)
                   |    +--- *.a         (if Linux*)
                   |    +--- ij.sh       (if Linux*)
                   |    +--- eclipse.sh  (if Linux*)
                   |    |
                   |    \--- Hello.class
                   |
                   +---doc
                   |    |
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


3.10 Build the selected components. 
    
    You can specify the components in the COMPONENTS Java* system property
    in a space-separated list. For example:      
      
    For Windows* users:
        build.bat -DCOMPONENTS="vm.jitrino vm.kernel_classes"
     
    For Linux* users:
        sh build.sh -DCOMPONENTS="vm.jitrino vm.kernel_classes"
        
    You can build any component indicated in the file
    ./build/make/deploy.xml.
   
3.11 Clean up the previously built code by executing one of the following:
   
     On  Windows*:     |  On Linux*:
     ------------------+----------------
     build.bat clean   |  build.sh clean 

3.12 Update the network resources as required. 
   
     The build uses external resources from a network, such as APR or Harmony classes.
     These are downloaded and stored locally during the first launch of the build. 

     3.12.1 Change the resource location settings by editing the property file:
      
            On Windows*:
            build\make\win.properties
      
            On Linux*:
            build/make/lnx.properties
      
            Each network resource location is configured in the property file, as follows:
      
            remote.<resource name>.archive=<Resource URL>
      
            You can override this setting through the system environment, as follows:
      
            <resource name>_HOME=<extracted archive location>
          
            Example:
            The build searches for the Eclipse* archive in its default location on the web:
       
            remote.ECLIPSE.archieve=http://eclipse.objectweb.org/downloads/drops/R-3.1.1-200509290840/eclipse-SDK-3.1.1-win32            .zip
      
            You can specify the Eclipse* installation directory by setting the ECLIPSE_HOME 
            environment variable.  
      
     3.12.2 Updating the local version of external resources
   
            To download a fresh copy of the external resources, type the following:
      
            On Windows*:        |  On Linux*:
            --------------------+-----------------
            build.bat update    |  build.sh update
     
            External resources are stored in the directory:
      
                build/pre-copied
     
     3.12.3 Configuring Ant proxy settings
     
            The proxy server can be configured in one of the following ways:
            
            a) Using the system properties http.proxyHost and http.proxyPort, for example:
            
                build.bat -Dhttp.proxyHost=proxy -Dhttp.proxyPort=8080
            
            b) By specifying values for http.proxyHost and http.proxyPort in win.properties and
            lnx.properties on Windows* and Linux* correspondingly.
            
     3.12.4 Configuring SVN

            To configure the SVN proxy, make sure that the file
            ~/.subversion/servers on Linux* or the file
            C:\Documents and Settings\<USER>\Application Data\Subversion\servers
            on Windows* contain the following lines:

            [global]
            http-proxy-host = proxy
            http-proxy-port = 8080

            To check the connection and obtain svn.apache.org SSL certificate run:
            
                svn co https://svn.apache.org/repos/asf/incubator/harmony/enhanced/classlib/tags 
            
            Select Accept (P)ermanently to save the certificate. This enables the script build.sh 
            to run SVN in the non-interactive mode.
            
            See http://www.apache.org/dev/version-control.html for more info.
     
3.13 Clean up the local copy of the external resources by executing the following:
      
     On Windows*:             |  On Linux*:
     -------------------------+-----------------------
     build.bat clean.update   |  build.sh clean.update
              
          NOTE: Because  you have executed the target 'update' previously, 
          this ./build.sh  takes the locally stored resources for building.  

3.14 Troubleshoot the DRLVM build if required. Possible errors include:

     
     + Command line is over the maximum length with no source file specified.

       This message indicates that the cpptasks.jar file was not patched correctly.
       The initial cpptasks bundle has a limitation on the command line length, see ant-contrib 
       bug #1402730 on sourceforge.net for details. To overcome this limitation, 
       build.xml patches cpptasks.jar before using it for native sources compilation.
       
       Solution: follow the instructions below.

        a) Check that you are using Ant version 1.6.5 or higher.
        b) Remove directories make/tmp/ and pre-copied/.
        c) Retry updating the build.


     + The property C:\work\vm-harmony\vm\tests\smoke\classloader\LogLoader.java.keywords
       is not defined. The property common.depends.on.targets is not defined.

       These or similar messages may indicate that you have an older version
       of ant-contrib installed in the $ANT_HOME/lib directory.

       To ensure that it uses correct version of ant-contrib,
       the build system downloads and uses its own private copy of
       ant-contrib.jar, in make/tmp directory.

       Solution: upgrade your $ANT_HOME/lib/ant-contrib.jar or
       delete it and let the build system download a correct version for you.


4. RUNNING DRLVM WITH EXTERNAL CLASS LIBRARIES
----------------------------------------------


To run DRLVM with third-party external class libraries, do the following:

+ Check that these class libraries comply with the kernel classes interface 
  and the VMI interfaces, see the description in the directory
  <EXTRACT_DIR>/Harmony/vm/vmcore/src/kernel_classes. 

+ Add external native dynamic libraries (*.dll or *.so files) to the system path or copy 
  the files into the <EXTRACT_DIR>/deploy/jre/bin directory. 

+ Add external library class directories or .jar files into the -Xbootclasspath option. 

Example:
ij -Xbootclasspath/p:c:\external_library\lib\classes.jar MyApp


5. BUILDING AND RUNNING TESTS
-----------------------------

To build and run the DRLVM tests, execute the following command:

          On Windows*:             |  On Linux*:
          -------------------------+-----------------------
          build.bat test           |  build.sh test


The test classes are placed in
<EXTRACT_DIR>/Harmony/make/%BUILD_CFG%_Windows_ia32_%NATIVE_COMPILER%/semis/vm/test/classes.
The test results are placed in
<EXTRACT_DIR>/Harmony/make/%BUILD_CFG%_Windows_ia32_%NATIVE_COMPILER%/semis/vm/test/report.


6. BUILDING DRLVM AND RUNNING TESTS IN A SELF-HOSTING ENVIRONMENT
------------------------------------------------------------------

You can build the DRLVM using the DRLVM itself and Harmony class libraries 
as a hosting environment for running Ant and the Eclipse* compiler. For that, 
do the following:

+ Copy the previously built JRE from build/${OS}_ia32_${CXX}_${BUILD_CFG}/deploy/jre
  to any directory, for example, <HARMONY_JRE>.

+ Set the JAVA_HOME environment variable to point to the <HARMONY_JRE> directory.

+ Run the build by starting:
    
     On Windows*:         |  On Linux*:
     ---------------------+--------------------
     build.bat            |  build.sh

+ Test the build as described in section 5 above.  


7. KNOWN ISSUES
---------------

Building in a self-hosting environment:

    In certain cases, the following error might be produced: 
    
        java.lang.InternalError: Error -1 getting next zip entry
    
    The error might appear during compilation of the kernel classes in a Harmony self-hosting environment
    due to the problems with the zip support module. A simple workaround is to restart
    the build or build the kernel classes separately by using the following command:
    
        build.bat -DCOMPONENTS=vm.kernel_classes

General restrictions:

    The number of threads must be lower than 800. 

    Java* heap memory must not be greater than 1.3 GB. 

    Code must not enter the same monitor more than 256 times. 

    The real object hash code has only 6 bits. 

    Currently, the debug scenario cannot run because JPDA is missing. 
    Note that the supplied documentation might not reflect the current situation. 

Bytecode verifier restrictions:

    The subroutine check is not supported.

    Class structure verification is not supported.

Partially implemented features: 

    Stack overflow is not supported in the JIT-enabled mode.
    
    JNI invocation API is only partially implemented.
    
    JVMTI has limited support in the JIT-enabled mode. The system
    supports the following events: 
        COMPILED_METHOD_LOAD
        DYNAMIC_CODE_GENERATED
        EXCEPTION
    The EXCEPTION event has not been tested for adequate operation yet.

    JVMTI implementation does not provide full argument validity checks.
    Specifically, the VM might crash when passing invalid Java* objects or JNI
    identifiers as arguments to JVMTI API functions.
    
    JVMTI API function groups for local variable access and stack inspection 
    in the JIT-enabled mode have not been tested for adequate operation. 

    Class unloading is not supported. 

    Tracing of method calls and instructions is not supported.
    The following methods have no effect:
        Runtime.traceInstructions(bool) 
        Runtime.traceMethodCalls(bool) 
        
    Java* assertions are not supported. 
    
    The java.lang.ref.SoftReference class implementation does not follow
    the intent of the specification and is currently equivalent to the
    java.lang.ref.WeakReference class. 

Jitrino restrictions:
    
    Parallel compilation and calls for resolution during a compilation
    session may lead to re-entering Jitrino from different or the same thread. 
    In our experience, Jitrino works normally in such cases. However, 
    certain Jitrino parts, such as timers and certain logging system 
    features are not designed to be re-enterable. This restriction is not 
    applicable to normal runs in the default mode with logging and timers 
    disabled.

    Only a few optimization passes are currently enabled in the high-level optimizer, 
    namely: code lowering, lazy exceptions, and the static profile estimator. 
    Guarded devirtualization and inlining are currently enabled in the bytecode translator.
    Other optimization passes are poorly tested and are disabled by default. 

8. TODO
-------

Provide complete J2SE 1.5.0 support.

Provide Itanium* and EM64T* support.

Improve performance on multi-core processors.

Increase modularity. For example, make the threading functionality
a separate module.

Implement multi-VM support and VM Local storage.

Complete remaining JVMTI functions for full debugging.

Migrate to common interfaces for JIT and Interpreter.

Improve GC performance. Add concurrent and generational GC
functionality.

Implement floating point support based solely on x87. In Jitrino,
floating-point arithmetic depends on SSE and SSE2 scalar
instructions. Consequently, Jitrino will not work on platforms
not supporting SSE2, such as Pentium 3.

Enable client-oriented and server-oriented optimization modes 
in the high-level optimizer. Currently, you can enable the client-oriented 
optimization mode by using the -Xjit client command-line option. 
However, this mode might be unstable on heavy workloads.

Code re-factoring of JIT bytecode translator is necessary. The
implementation of the prepass phase in the bytecode translator
is not optimal due to heavy bug fixing. 

Implement correctly all items pointed in the section Partially
implemented features in the section KNOWN ISSUES of this file.


9. DISCLAIMER AND LEGAL INFORMATION
------------------------------------


*) Other brands and names are the property of their respective owners.
