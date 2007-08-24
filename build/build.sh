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

# ================================================
# Check external resources / software installation
# ================================================

if [ -x "$ANT_HOME/bin/ant" ]; then
    # running locally installed ant
    ANT_COMMAND="$ANT_HOME/bin/ant --noconfig"
elif [ -x "`which ant 2>/dev/null`" ]; then
    # running pre-installed ant from GNU/Linux distribution
    ANT_COMMAND="`which ant`"
fi


if [ ! -x $JAVA_HOME/bin/java ] ; then
    echo "* Neither $JAVA_HOME/bin/java not found."
    echo "* Make sure you have J2SDK or DRLVM installed on your computer and that"
    echo "* JAVA_HOME environment variable points out to its installation dir"
    ERROR
elif [ ! -x "${ANT_COMMAND%% *}" ]; then
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
CLASSPATH=$CLASSPATH:`pwd`/make/tmp/junit.jar
CLASSPATH=$CLASSPATH:`pwd`/make/tmp/xalan.jar
CLASSPATH=`pwd`/make/tmp/cpptasks/patched.classes:$CLASSPATH
CLASSPATH=`pwd`/make/tmp/ant-contrib.jar:$CLASSPATH
export CLASSPATH

$ANT_COMMAND -f ./make/build.xml "$@"  || ERROR
