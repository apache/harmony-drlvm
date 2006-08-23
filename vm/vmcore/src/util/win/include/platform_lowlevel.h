/*
 *  Copyright 2005-2006 The Apache Software Foundation or its licensors, as applicable.
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */
/** 
 * @author Intel, Evgueni Brevnov
 * @version $Revision: 1.1.2.1.4.3 $
 */  


#ifndef _platform_lowlevel_H_
#define _platform_lowlevel_H_

//MVM

#if _MSC_VER >= 1300 || __INTEL_COMPILER
// workaround for the
// http://www.microsoft.com/msdownload/platformsdk/sdkupdate/2600.2180.7/contents.htm
#include <winsock2.h>
#endif

#include <windows.h>

#if _MSC_VER < 1300
#include <winsock2.h>
#endif

#include <crtdbg.h>

#include "platform.h"

inline void disable_assert_dialogs() {
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDOUT);
    _CrtSetReportMode(_CRT_ERROR,  _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ERROR,  _CRTDBG_FILE_STDOUT);
    _CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_WARN, _CRTDBG_FILE_STDOUT);
    _set_error_mode(_OUT_TO_STDERR);
}

inline void debug_break() {
    _CrtDbgBreak();
}

inline DWORD IJGetLastError(VOID)
{
    return GetLastError();
}

struct timespec {
    long tv_sec;
    long tv_nsec;
};

#endif // _platform_lowlevel_H_
