#! /bin/bash
#
# @author: Valentin Al. Sitnick, Petr Ivanov.
# @version: $Revision$
#

system=`uname`

USE_INTERPRETER="-Dvm.use_interpreter=0"

FIND=/usr/bin/find

#detect architecture
TMPARCH=`uname -m |sed -e 's/i.86/i386/'`
if [ "$TMPARCH" = "i386" ]; then
    TST_ARCH="ia32"
else
    echo "Architectures other than IA32 are not supported yet"
    exit
fi


#
# Select compiler for native libs creating
# - 'gcc' for GNU Compiler (default for Linux)
# - 'icc' for Intel Compiler (optional for Linux)
# - 'cl' Microsoft Compiler (default for Windows)
# - 'icl' for Intel Compiler (optional for Windows)

if [ $system = "Linux" ]; then

    PREFERRED_COMPILER=gcc
    ALT_COMPILER=icc

    C_COMPILER=$PREFERRED_COMPILER

    if [ -z "$VTSSUITE_ROOT" ] ; then
        VTSSUITE_ROOT="$PWD"
    fi

    if [ -z "$REF_JAVA_HOME" ] ; then
        REF_JAVA_HOME="$JAVA_HOME"
    fi

    if [ -z "$TST_JAVA_HOME" ] ; then
        if [ ! -d "../../../build/lnx_${TST_ARCH}_${PREFERRED_COMPILER}_debug/deploy/jdk" ] ; then
            TST_JAVA_HOME="../../../build/lnx_${TST_ARCH}_${ALT_COMPILER}_debug/deploy/jdk"
        else
            TST_JAVA_HOME="../../../build/lnx_${TST_ARCH}_${PREFERRED_COMPILER}_debug/deploy/jdk"
        fi
    fi

    TST_JAVA="$TST_JAVA_HOME/jre/bin/java"
    TST_JAVA_INCLUDE="$TST_JAVA_HOME/include"


    export LD_LIBRARY_PATH="$VTSSUITE_ROOT/bin/lib"

else # for Windows

    PREFERRED_COMPILER=cl
    ALT_COMPILER=icl

    C_COMPILER=$PREFERRED_COMPILER

    PREFERRED_COMPILER=`echo -n $PREFERRED_COMPILER |sed -e 's/^cl/msvc/'`
    ALT_COMPILER=`echo -n $ALT_COMPILER |sed -e 's/^cl/msvc/'`

    if [ -z "$VTSSUITE_ROOT" ] ; then
        VTSSUITE_ROOT="$PWD"
    fi

    if [ -z "$REF_JAVA_HOME" ] ; then
        REF_JAVA_HOME="$JAVA_HOME"
    fi

    if [ -z "$TST_JAVA_HOME" ] ; then
        if [ ! -d "../../../build/win_${TST_ARCH}_${PREFERRED_COMPILER}_debug/deploy/jdk" ] ; then
            TST_JAVA_HOME="../../../build/win_${TST_ARCH}_${ALT_COMPILER}_debug/deploy/jdk"
        else
            TST_JAVA_HOME="../../../build/win_${TST_ARCH}_${PREFERRED_COMPILER}_debug/deploy/jdk"
        fi
    fi

    TST_JAVA="$TST_JAVA_HOME/jre/bin/java.exe"
    TST_JAVA_INCLUDE="$TST_JAVA_HOME/include"

    export PATH=$PATH:$VTSSUITE_ROOT/bin/lib
fi

source parse_args.sh

