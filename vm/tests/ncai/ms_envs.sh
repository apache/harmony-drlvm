#!/bin/bash
#
# @author: Valentin Al. Sitnick, Petr Ivanov
# @version: $Revision$
#

#
# Sets the Microsoft Visual Studio .NET 2003 environment.
#
# According to ${MVS_DIR}\Common7\Tools\vsvars32.bat the
# following list of variables must be set:
#
#     DevEnvDir
#     FrameworkDir
#     FrameworkSDKDir
#     FrameworkVersion
#     MSVCDir
#     VSINSTALLDIR
#     VCINSTALLDIR
#
#     INCLUDE
#     LIB
#     PATH
#

MVS_DIR_WIN="C:\Program Files\Microsoft Visual Studio .NET 2003"
MVS_DIR_UNIX="/cygdrive/c/Program Files/Microsoft Visual Studio .NET 2003"

export DevEnvDir="${MVS_DIR_WIN}\Common7\IDE"
export FrameworkDir="C:\WINNT\Microsoft.NET\Framework"
export FrameworkSDKDir="${MVS_DIR_WIN}\SDK\v1.1"
export FrameworkVersion="v1.1.4322"
export MSVCDir="${MVS_DIR_WIN}\VC7"
export VCINSTALLDIR="${MVS_DIR_WIN}"
export VSINSTALLDIR="${MVS_DIR_WIN}\Common7\IDE"

# the vsvars32.bat does not set this variable
#export VS71COMNTOOLS="${MVS_DIR_WIN}\Common7\Tools"

#
# Sets INCLUDE variable
#
if [ ! -n "${INCLUDE}" ] ; then
    INCLUDE="${MVS_DIR_WIN}\VC7\ATLMFC\INCLUDE"
else
    INCLUDE="${INCLUDE};${MVS_DIR_WIN}\VC7\ATLMFC\INCLUDE"
fi
echo -n "Start... "
INCLUDE="${INCLUDE};${MVS_DIR_WIN}\VC7\INCLUDE"
INCLUDE="${INCLUDE};${MVS_DIR_WIN}\VC7\PlatformSDK\include\prerelease"
INCLUDE="${INCLUDE};${MVS_DIR_WIN}\VC7\PlatformSDK\include"
INCLUDE="${INCLUDE};${MVS_DIR_WIN}\SDK\v1.1\include"
export INCLUDE

#
# Sets LIB variable
#
if [ ! -n "${LIB}" ] ; then
    LIB="${MVS_DIR_WIN}\VC7\ATLMFC\LIB"
else
    LIB="${LIB};${MVS_DIR_WIN}\VC7\ATLMFC\LIB"
fi
LIB="${LIB};${MVS_DIR_WIN}\VC7\ATLMFC\LIB"
LIB="${LIB};${MVS_DIR_WIN}\VC7\LIB"
LIB="${LIB};${MVS_DIR_WIN}\VC7\PlatformSDK\lib\prerelease"
LIB="${LIB};${MVS_DIR_WIN}\VC7\PlatformSDK\lib"
LIB="${LIB};${MVS_DIR_WIN}\SDK\v1.1\lib"
export LIB

#
# Sets PATH variable
#
PATH="${PATH}:${MVS_DIR_UNIX}/Common7/IDE"
PATH="${PATH}:${MVS_DIR_UNIX}/VC7/BIN"
PATH="${PATH}:${MVS_DIR_UNIX}/Common7/Tools"
PATH="${PATH}:${MVS_DIR_UNIX}/Common7/Tools/bin/prerelease"
PATH="${PATH}:${MVS_DIR_UNIX}/Common7/Tools/bin"
PATH="${PATH}:${MVS_DIR_UNIX}/SDK/v1.1/bin"

echo "done..."

