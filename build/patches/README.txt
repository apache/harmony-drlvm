UPDATE TO HARMONY SOURCES FOR INTEL DRLVM CONTRIBUTION
          March 30, 2006
======================================

This archive contains patches for the Apache Harmony class libraries:
  - Small patch for the class library com.ibm.oti.vm.MsgHelp
    The setLocale() method has been accommodated to work with the null class loader.
  - Patch for java/net/URLClassLoader.java to correctly process whitespaces 
    in classpath.
  - Patch for the org.apache.harmony.eclipse.jdt.launching plugin 
    The new functionality enables Eclipse* to recognize Intel DRLVM. 

Extract the archive in the same directory as Intel DRLVM. 

1. ARCHIVE CONTENTS
-------------------

After extracting the archive, the following directories appear under
<DRLVM_EXTRACT_DIR>/Harmony, where <DRLVM_EXTRACT_DIR> is the location, 
to which you have extracted DRLVM and this archive:

<DRLVM_EXTRACT_DIR>/Harmony/build/patches
       |
       +---common
       |    |
       |    +- CLASSLIB...    - patched class library sources    
       |    |
       |    +- HYPLUGIN...  - patched Eclipse* plugin sources
       |    
       +- README              - This file
       |
       +- hyplugin.patch  
       |   
       +- luni-msghelp.patch 
       |
       +- url_classloader.patch

2. PATCHING HARMONY CLASSES
---------------------------

The DRLVM build patches Harmony sources automatically as the last step 
of the external resource downloading. See the README.txt file for the VM 
for more information about DRLVM building process.

*) Other brands and names are the property of their respective owners.
