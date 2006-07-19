#!/bin/sh
#    Copyright 2005-2006 The Apache Software Foundation or its licensors, as applicable.
#  
#    Licensed under the Apache License, Version 2.0 (the "License");
#    you may not use this file except in compliance with the License.
#    You may obtain a copy of the License at
#  
#       http://www.apache.org/licenses/LICENSE-2.0
#  
#    Unless required by applicable law or agreed to in writing, software
#    distributed under the License is distributed on an "AS IS" BASIS,
#    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#    See the License for the specific language governing permissions and
#    limitations under the License.

# @author: Sergey V. Dmitriev
# @version: $Revision: 1.10.2.10 $

# This file is a Linux command line interface to the Harmony build. 
# It checks the installed software, required tools and external resources
# and then executes Ant.
# The locations of all external resources and installed software must be
# defined in defs.bat file or in appropriate environment variables.
# See defs.bat for variable descriptions.

# Prints a common guiding information and exits from script

ERROR() {
    echo "*"
    echo "* Please, refer to README.txt for details."
    exit 1
}

export MACHINE_ARCH=`uname -m`

EXTERNAL_DIR=/nfs/site/proj/drl/share/binaries/externals/clean

# ================================================
# Set default environment variables
# ================================================


# Apache Ant 1.6.2 or higher (can be obtained at http://ant.apache.org)
if [ -z "$ANT_HOME" ]; then
    ANT_HOME=$EXTERNAL_DIR/common/apache-ant-1.6.5-bin/apache-ant-1.6.5
fi


# ================================================
# Check external resources / software installation
# ================================================

if [ ! -x $JAVA_HOME/bin/java ] && [ ! -x $JAVA_HOME/bin/ij ]; then
    echo "* Neigher $JAVA_HOME/bin/java nor $JAVA_HOME/bin/ij found."
    echo "* Make sure you have J2SDK or DRLVM installed on your computer and that"
    echo "* JAVA_HOME environment variable points out to its installation dir, e.g."
    echo "* export JAVA_HOME=/usr/local/jdk_1.4.2"
    ERROR
elif [ ! -x $ANT_HOME/bin/ant ]; then
    echo "* File $ANT_HOME/bin/ant not found."
    echo "* Make sure you have Ant 1.6.5 or above installed from"
    echo "* http://ant.apache.org/bindownload.cgi and that ANT_HOME environment"
    echo "* variable points out to the Ant installation dir, e.g. "
    echo "* export ANT_HOME=/usr/local/ant_1.6.5"
    ERROR
fi

export ANT_HOME=`(cd $ANT_HOME;pwd)`
export CXX
export BUILD_CFG
CLASSPATH=`pwd`/make/tmp/cpptasks.jar
CLASSPATH=$CLASSPATH:`pwd`/make/tmp/org.eclipse.jdt.core_3.1.1.jar;
CLASSPATH=$CLASSPATH:`pwd`/make/tmp/jdtCompilerAdapter.jar
CLASSPATH=$CLASSPATH:`pwd`/make/tmp/junit.jar
CLASSPATH=$CLASSPATH:`pwd`/make/tmp/xalan.jar
CLASSPATH=`pwd`/make/tmp/cpptasks/patched.classes:$CLASSPATH
CLASSPATH=`pwd`/make/tmp/ant-contrib.jar:$CLASSPATH
export CLASSPATH

ANT_COMMAND="$ANT_HOME/bin/ant --noconfig"

# it is necessarily to compile 'vm.jitrino' in release mode

$ANT_COMMAND -f ./make/build.xml -Dvm.jitrino.cfg=release "$@"  || ERROR

