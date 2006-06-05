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
 * @author Sergey L. Ivashin
 * @version $Revision: 1.1.22.3 $
 *
 */

#include "XTimer.h"

#ifdef _WIN32
#include <windows.h>
#define TIMERS_IMPLEMENTED
//#define USE_THREAD_TIMER
#endif


namespace Jitrino 
{

static bool   enabled = false;
static int64  correction;
static double frequency;


void XTimer::initialize (bool on) 
{
#ifdef TIMERS_IMPLEMENTED

	if (!enabled && on)
	{
		enabled = true;

#ifdef USE_THREAD_TIMER
		frequency = 10000000;
#else
		LARGE_INTEGER freq;
		QueryPerformanceFrequency(&freq);
		frequency = (double)freq.QuadPart;
#endif

		XTimer xt;
		correction = 0;
		xt.start(); xt.stop();
		xt.start(); xt.stop();
		xt.start(); xt.stop();
		xt.start(); xt.stop();
		correction = xt.totalTime/4;
	}

	enabled = on;

#endif //#ifdef TIMERS_IMPLEMENTED
}


static int64 getCounter() 
{
#ifdef TIMERS_IMPLEMENTED

#ifdef USE_THREAD_TIMER
    FILETIME creation;
    FILETIME exit;
    FILETIME system;
    FILETIME user;
    GetThreadTimes(GetCurrentThread(), &creation, &exit, &system, &user);
    return (((int64)user.dwHighDateTime) << 32) + user.dwLowDateTime;
#else
    LARGE_INTEGER count;
    QueryPerformanceCounter(&count);
    return count.QuadPart;
#endif

#else //#ifdef TIMERS_IMPLEMENTED

    return 0;

#endif //#ifdef TIMERS_IMPLEMENTED
}


//
// Clear the timer.
//
void XTimer::reset () 
{
    totalTime = 0;
    state = 0;
}
    

//
// Start the timer. Continue timing from where we were.
//
void XTimer::start () 
{
	if (enabled)
	    if (state++ == 0)
		  startTime = getCounter();
}


//
// Stop the timer.
//
void XTimer::stop () 
{
	if (enabled)
	    if (--state == 0) 
	        totalTime += getCounter() - startTime - correction;
}


double XTimer::getSeconds() const
{
	if (enabled)
	    return (((double)totalTime) / frequency);
	else
		return 0;
}


/*
double XTimer::getFrequency()
{
    return frequency;
}
*/


} //namespace Jitrino 
