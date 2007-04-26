

                 HOW TO ADD A NEW COMPONENT TO THE BUILD
                 =======================================

I. INTRODUCTION    
---------------
    
    The build provided with DRLVM has the following structure:
    
    make/
      |
      +--- setup.xml
      |        first entry point: downloads, prepares and checks the necessary 
      |        external resources
      |
      +--- get_antcontrib.xml
      |        used from setup.xml: gets ant-contrib from an external location 
      |        and prepares it to be used by Ant
      |
      +--- build.xml
      |        second entry point: main build file
      |
      +--- deploy.xml
      |        describes the content of the product
      |
      +--- build_component.xml
      |        build 'core': generates and executes build file for
      |        each component
      |
      +--- build_component.xsl
      |        xslt used for pre-processing of component descriptors
      |
      +--- selector.xsl
      |        xslt used for attribute-based filtering of XML content
      |
      +--- ant/
      |        stub for the Ant Splash task that
      |        eliminates the dependency on Swing
      |
      +--- components/
      |        contains descriptors for components
      |
      +--- targets/
               contains target templates and shared settings for
               components
    
    Each component in the product is described with an Ant-based XML descriptor. 
    To add a new component, you need to create a descriptor for it.
    The component is defined as follows:

      - A component is a more or less distinct part of product.

      - A component can contain several types of source:
        C++ sources, C sources, assembler sources, Java* sources, smoke 
        and unit tests.

      - A component can be processed in several ways in the build:
        produce native output (shared or static library or executable)
        and/or Java* classes in .jar file, produce and run smoke and/or unit tests. 
    
    The build dynamically produces a build_${component}.xml file for
    each specific component by taking its descriptor, performing the
    attribute-based filtering on it, and then applying target templates
    and settings on it.
    
    
II. HOW TO DEVELOP A COMPONENT DESCRIPTOR
-----------------------------------------

    Each descriptor is placed in the components/ directory and 
    follows the structure described below:

     1. Component build descriptor must be formed in the project with
        the corresponding name and nested target with a fixed name. 
        Example with the name init: 

        <project name="component_name" >
            <target name="init">
                ...
            </target>
        </project>

     2. Inside the mentioned project and target tags, the
        descriptor must follow the following structure:

        - Native part:
            - Common description:
                - The property "libname" represents the native output
                  (shared/static library or executable) file name,
                  specified without the extention, for example, 

                  <property name="libname" value="lang" />

                - The property "outtype" represents the native output type
                  ("shared", "static" or "executable"). 
                  Example: 
                  <property name="outtype" value="shared" />

            - C compilation related description:
              The compiler definition with id="c.compiler" represents
              the C-related resources: the source path file set, the directory
              set of include files, compiler arguments, and define
              sets. Example: 

              <compiler id="c.compiler" extends="comm.cpp.compiler">
                  <includepath>
                      <dirset dir="${src}" >
                          <include name="src\api" />
                      </dirset>
                  </includepath>
                  <fileset dir="${src}">
                      <include name="src/vm/*.c" />
                  </fileset>
                  <compilerarg value="/WX" />
                  <defineset define="DEBUG" />
              </compiler>

            - C++ compilation related description:
              The compiler definition with id="cpp.compiler" represents
              the C++-related resources: the source path file set, the directory
              set of include files, compiler arguments, and define
              sets. Example: 

              <compiler id="cpp.compiler" extends="common.cpp.compiler">
                  <includepath>
                      <dirset dir="${src}" >
                          <include name="src/api" />
                      </dirset>
                  </includepath>
                  <fileset dir="${src}">
                      <include name="src/vm/*.cpp" />
                  </fileset>
                  <compilerarg value="/WX" />
                  <defineset define="DEBUG" />
              </compiler>

            - Assembler compilation description:
              The description represents the assembler source file set
              and compiler arguments. Example:

              <fileset id="asm.fileset" dir="${src}">
                  <include name="vmcore/src/thread/atomic.asm" />
              </fileset>
              <property name="asm.args" value="-Wa" />

            - Linker-related description:
              The description represents the library set and linker arguments. 
              NOTE: You need not include object files produced in the native part;
              they will be passed to the linker automatically. 
              Example: 

              <linker id="linker" extends="common.linker">
                  <linkerarg value="--export-dynamic" />
                  <linkerarg value="/NODEFAULTLIB:libcmt.lib" />
                  <syslibset libs="advapi32,userenv,ws2_32,vmcore,odbc32,mswsock" />
                  <libset libs="${vm.vmcore.lib}" dir="${vm.vmcore.libdir}" />
              </linker>

        - Java* part:
            - The property "jarname" represents the output .jar file name
              with compiled Java* classes with no file extention specified. 
              Example: 

              <property name="jarname" value="kernel.jar" />

            - The property "srcjarname" represents the output .jar file name
              with Java* source files. 
              Example: 

              <property name="srcjarname" value="kernel-src.jar" />

            - Java* source files location. 
              Example: 

              <property name="java.source.dir"
                  location="${build.vm.home}/vmcore/src/kernel_classes/javasrc"/>

            - Java* source files pattern: files to pass to javac
              Example: 

              <patternset id="java.source.pattern"  includes="**/*.java"/>

            - Java* class files pattern: files to pack into the .jar file
              Example: 

              <patternset id="java.classes.pattern"  includes="**/*.class"/>

            - Java* class path used as the boot class path in javac
              Example: 

              <path id="java.class.path">
                  <fileset dir="${classlib.luni.jardir}" includes="*.jar" />
                  <fileset dir="${classlib.security.jardir}" includes="*.jar" />
              </path>

            - Java* unit tests file set description, which represents:
                  - Java* unit tests source
                  - Pattern for source file set
              Example: 

              <path id="unit.test.javasrc">
                   <pathelement location="${build.VM.home}/tests/unit" />
               </path>
              <patternset id="unit.test.java.pattern">
                  <include name="**/*.java" />
              </patternset>

            - Java* smoke tests file set description, which represents:
                  - Java* smoke tests source
                  - Pattern for source file set
                  - Pattern for classes file set
              Example: 

              <property name="smoke.test.javasrc" location="${build.VM.home}/tests/smoke" />
              <patternset id="smoke.test.java.pattern">
                  <include name="**/*.java"/>
                  <exclude name="**/Logger.java"/>
              </patternset>        
              <patternset id="smoke.test.pattern">
                  <include name="**/*.class"/>
                  <exclude name="**/*$*.class"/>
              </patternset>        

III. ATTRIBUTE-BASED FILTERING
------------------------------

    To support the multiple platforms, operating systems, compilers and 
    so on, the build supports an attribute-based filtering mechanism. 
    Use the tags <select> to identify all elements (or sets of elements) 
    in the descriptor file that can applied only to a specific operating system, 
    platform architecture, debug or release mode or a native compiler.
    The following attributes are supported
    for <select> tags:
            
         Attr  |  Meaning           |  Supported values
         ------+--------------------+-----------------         
         os    |  operating system  |  win, lnx
         arch  |  architecture      |  ia32, ipf, em64t
         cxx   |  native compiler   |  msvc, icl 
         cfg   |  building mode     |  release, debug
            
    The example below illustrates using different source files
    for different operating systems and platform architectures. 
            
    <compiler id="c.compiler" extends="comm.cpp.compiler">
        <fileset dir="${src}">
            <select os="win" arch="ia32">
                <compilerarg name="src/WINDOWS/IA-32/file.cpp" />
            </select>
            <select os="win" arch="ipf">
                <compilerarg name="src/WINDOWS/IPF/file.cpp" />
            </select>
            <select os="lnx" arch="ia32">
                <compilerarg name="src/LINUX/IA-32/file.cpp" />
            </select>
            <select os="win" arch="ia32,ipf">
                <compilerarg name="src/WINDOWS/IA-32_or_IPF/file2.cpp" />
            </select>
            ...
        </fileset>
        ...
    </compiler>

    Having no value for a specific attribute means that the contents of <select> tags
    is applicable for any value of the given attribute.
    

IV. A COMPONENT DESCRIPTOR EXAMPLE
----------------------------------

    This section provides the component descriptor sample that illustrates
    adding a new descriptor.

    <!-- All components descriptors are Ant projects with the name of
         the component name. -->
    <project name="vm.vmcore">
        <!-- The target "init" provides all static information that is required
             to build the component. The target can depend on common targets 
             (see the common targets definition in the build/targets directory). 
             Via targets, components can inherit common features. --> 

        <!-- In the "init" target, you can include the following properties 
             shared across components: 
                $libname  - the name of the library
                $src      - the root of the source tree
                $includes - the include directory (if the components
                            exports some include files)
                $libdir   - the directory where the produced library
                            is generated (if it deffered of the
                            default one)
                $jarname  - the name of produced jar file (if it
                            exists)
                $jardir   - the directory where the produced library
                            is generated, all the property can be set
                            in the common target and in the descriptor
                            all the properties of the other components
                            can be obtained for example
                            ${classlib.luni.jarname} is the jarname of
                            classlib.luni component
          --> 

    <target name="init" depends="common_vm">
        <!-- ${target}.depends property defines the components that the
             component requires to execute the target. If the target
             does not depend on other componets, the property can 
             be not set. 
             For example, the component vm.mcore depends on
             extra.apr, extra.aprutil, extra.log4cxx, extra.zlib,
             vm.encoder, vm.port for building, but does not depend on
             them to run tests or to generate documents. -->

        <property name="build.depends" value="extra.apr,
                                              extra.aprutil,
                                              extra.log4cxx,
                                              extra.zlib,
                                              vm.encoder,
                                              vm.port" />

        <!-- the name and the type of the produced library: 
             on Windows* vmcore.dll, on Linux* vmcore.so -->
        <property name="libname" value="vmcore" />
        <property name="outtype" value="shared" />

        <!-- the root of the source code location -->
        <property name="src" location="${build.vm.home}" />

        <!-- compiler settings to compile cpp sources, see
             cpptasks documentation -->
        <compiler id="cpp.compiler" extends="common.cpp.compiler">
            <fileset dir="${build.vm.home}/vmcore/src">
                <include name="class_support/*.cpp" />
                <include name="exception/*.cpp" />
                <include name="init/*.cpp" />
                <include name="gc/*.cpp" />
                <include name="interpreter/*.cpp" />
                <include name="jit/*.cpp" />
                <include name="jni/*.cpp" />
                <include name="jvmti/*.cpp" />
                <include name="object/*.cpp" />
                <include name="reflection/*.cpp" />
                <include name="stack/*.cpp" />
                <include name="thread/*.cpp" />
                <include name="util/*.cpp" />
                <include name="verifier/*.cpp" />
            </fileset>
            <!-- omitted some fileset from original
                  vmcore.xml for obviousness -->

            <includepath>
                <pathelement location="${extra.apr.includes}" />
                <pathelement location="${extra.log4cxx.includes}" />
                <select os="win">
                    <pathelement location="${extra.zlib.includes}" />
                </select>
            </includepath>

            <!-- omitted some includepath from original
                 vmcore.xml; for obviousness -->
            <select os="win" cfg="release" cxx="icl">
                <compilerarg value="/Qip" />
            </select>
            <defineset define="BUILDING_VM,GC_V4,USE_DLL_JIT,APR_DECLARE_STATIC" />
        </compiler>

        <!-- assembler description section -->
        <select os="lnx" arch="ia32">
            <fileset id="asm.fileset"
                     dir="${build.vm.home}/vmcore">
                <include name="src/util/ia32/base/*.asm" />
            </fileset>
        </select>

        <select os="lnx" arch="ipf">
            <fileset id="asm.fileset"
                 dir="${build.vm.home}/vmcore">
                <include name="src/util/ipf/base/*.asm" />
            </fileset>
        </select>

        <!-- linker settings -->
        <linker id="linker" extends="common.linker">
            <libset libs="${vm.port.lib}"
                    dir="${vm.port.libdir}" />

            <libset libs="${vm.encoder.lib}"
                    dir="${vm.encoder.libdir}" />

            <libset libs="${extra.log4cxx.lib}"
                    dir="${extra.log4cxx.libdir}" />

            <libset libs="${extra.aprutil.lib}"
                    dir="${extra.aprutil.libdir}" />

            <libset libs="${extra.apr.lib}"
                    dir="${extra.apr.libdir}" />

            <select os="win">
                <libset libs="${extra.zlib.lib}"
                        dir="${extra.zlib.libdir}" />
            </select>

            <select os="win" cfg="debug">
                <linkerarg value="/NODEFAULTLIB:libcmt.lib" />
            </select>

            <select os="lnx">
                <linkerarg value="--export-dynamic" />
                <linkerarg value="-lz" />
                <linkerarg value="-lxml2" />
                <linkerarg value="-lm" />
                <linkerarg value="-ldl" />
                <linkerarg value="-lpthread" />
                <linkerarg value="-lstdc++" />
            </select>
        </linker>
    </target>
    </project>
