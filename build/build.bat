@ECHO OFF

rem    Copyright 2005-2006 The Apache Software Foundation or its licensors, as applicable.
rem  
rem    Licensed under the Apache License, Version 2.0 (the "License");
rem    you may not use this file except in compliance with the License.
rem    You may obtain a copy of the License at
rem  
rem       http://www.apache.org/licenses/LICENSE-2.0
rem  
rem    Unless required by applicable law or agreed to in writing, software
rem    distributed under the License is distributed on an "AS IS" BASIS,
rem    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
rem    See the License for the specific language governing permissions and
rem    limitations under the License.

REM
REM @author: Sergey V. Dmitriev
REM @version: $Revision: 1.9.2.12 $
REM

REM This file is a Windows command line interface to the Harmony build. 
REM It checks the installed software, required tools and external resources
REM and then executes Ant.
REM The locations for all external resources are defined in make/win.properties file.
REM They can be altered via appropriate environment variables. 
REM See win.properties for variable descriptions.

SETLOCAL

REM ================================================
REM  Check environment variables.
REM 
REM  For the quick start, build needs the following 
REM  variables to be set:
REM  JAVA_HOME = <Path to 1.4-compatible JRE>
REM  ANT_HOME = <Path to Apache Ant 1.6.5>
REM  COMPILER_CFG_SCRIPT = <Whatever script that is configuring environment for C/C++ compiler>
REM ================================================


REM Script for configuring C/C++ compiler, Intel C compiler by default.

IF DEFINED COMPILER_CFG_SCRIPT GOTO CONFIG

IF "%CXX%" == "msvc" (
    IF EXIST "C:\Program Files\Microsoft Platform SDK\SetEnv.Cmd" (
        SET COMPILER_CFG_SCRIPT=C:\Program Files\Microsoft Platform SDK\SetEnv.Cmd
    ) ELSE IF EXIST "C:\Program Files\Microsoft SDK\SetEnv.bat" (
        SET COMPILER_CFG_SCRIPT=C:\Program Files\Microsoft SDK\SetEnv.bat
    ) ELSE IF EXIST "C:\Program Files\Microsoft Visual Studio .NET 2003\Common7\Tools\vsvars32.bat" (
        SET COMPILER_CFG_SCRIPT=C:\Program Files\Microsoft Visual Studio .NET 2003\Common7\Tools\vsvars32.bat
    ) ELSE IF EXIST "c:\Program Files\Microsoft Visual Studio\VC98\Bin\VCVARS32.BAT" (
        SET COMPILER_CFG_SCRIPT=C:\Program Files\Microsoft Visual Studio\VC98\Bin\VCVARS32.BAT
    )

    IF "%BUILD_CFG%" == "release" (
        SET COMPILER_CFG_ARG=/RETAIL
    ) ELSE (
        SET COMPILER_CFG_ARG=/DEBUG
    )
            
) ELSE (
    IF NOT DEFINED VS71COMNTOOLS (
        IF EXIST "C:\Program Files\Microsoft Visual Studio .NET 2003\Common7\Tools" (
            SET VS71COMNTOOLS=C:\Program Files\Microsoft Visual Studio .NET 2003\Common7\Tools\
        )
    )

    IF EXIST "C:\Program Files\Intel\Compiler\C++\9.0\IA32\Bin\iclvars.bat" (
        SET COMPILER_CFG_SCRIPT=C:\Program Files\Intel\Compiler\C++\9.0\IA32\Bin\iclvars.bat
    ) 
) 

IF NOT DEFINED COMPILER_CFG_SCRIPT (
    ECHO error: Cannot guess the location of compiler configuration script
    ECHO Please set COMPILER_CFG_SCRIPT and/or CXX
    GOTO ERROR
)

:CONFIG
ECHO ON
CALL "%COMPILER_CFG_SCRIPT%" %COMPILER_CFG_ARG%
@ECHO OFF

IF NOT %ERRORLEVEL% == 0 (
    ECHO *
    ECHO * Failed to call C compiler configuration script:
    ECHO * %COMPILER_CFG_SCRIPT%
    ECHO *
    GOTO ERROR
 )

REM ================================================
REM Check JAVA_HOME & ANT_HOME
REM ================================================

IF NOT EXIST "%JAVA_HOME%\bin\java.exe" (
    IF NOT EXIST "%JAVA_HOME%\bin\java.exe" (
        ECHO * Neigher "%JAVA_HOME%\bin\java.exe" nor "%JAVA_HOME%\bin\java.exe" found.
        ECHO * Make sure you have Harmony JRE or DRLVM installed on your computer and that
        ECHO * JAVA_HOME environment variable points out to its installation dir, e.g.
        ECHO * SET JAVA_HOME=c:\jre
        GOTO ERROR
    )
)

IF NOT EXIST "%ANT_HOME%\bin\ant.bat" (
    ECHO * File %ANT_HOME%\bin\ant.bat not found.
    ECHO * Make sure you have Ant 1.6.5 or above installed from
    ECHO * http://ant.apache.org/bindownload.cgi and the ANT_HOME environment
    ECHO * variable points to the Ant installation dir, e.g.
    ECHO * SET ANT_HOME=c:\ant_1.6.5
    GOTO ERROR
)


SET CLASSPATH=

REM ===================
REM Executing Ant build
REM ===================

SET CLASSPATH=%CD%\make\tmp\cpptasks.jar
SET CLASSPATH=%CLASSPATH%;.\make\tmp\junit.jar
SET CLASSPATH=%CLASSPATH%;.\make\tmp\xalan.jar

SET CLASSPATH=%CD%\make\tmp\cpptasks\patched.classes;%CLASSPATH%
SET CLASSPATH=.\make\tmp\ant-contrib.jar;%CLASSPATH%

SET ANT_COMMAND=%ANT_HOME%\bin\ant.bat

REM  Note: vm.jitrino is always complied in release mode, otherwise it makes VM debug too slow
CALL "%ANT_COMMAND%" -f make/build.xml -Dvm.jitrino.cfg=release %*

GOTO THEEND

:ERROR
ECHO *
ECHO * Please, refer to README.txt for details.
ECHO *
EXIT /B 1

:THEEND
