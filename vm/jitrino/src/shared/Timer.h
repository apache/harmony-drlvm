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
 * @author Intel, Mikhail Y. Fursov
 * @version $Revision: 1.13.16.1.4.4 $
 *
 */

#ifndef _TIMER_H_
#define _TIMER_H_

#ifndef PLATFORM_POSIX
// must be here, before <windows.h> to avoid compile-time errors
// with winsock.h - 
// http://www.microsoft.com/msdownload/platformsdk/sdkupdate/2600.2180.7/contents.htm
#include <winsock2.h>
#include <windows.h>
#define TIMERS_IMPLEMENTED 1
#endif
#include "Jitrino.h"
#include <ostream>


namespace Jitrino {

class Timer;


class Timer {
    const char * name;        // Name for the event counter
    int64 startTime;
    int64 totalTime;
    int64 correction;
    double calibrationTime;
    double timerFrequency;
    bool isOn;
    Timer *next;
    int recursionCount;
public:
    Timer(const char *_name, Timer *next0) : name(_name), isOn(false), next(next0),
                                             recursionCount(0) {
        //calibrateTimer();
        resetTimer();
    };

    void setFrequency() {
#ifdef TIMERS_IMPLEMENTED 
#ifdef USE_THREAD_TIMER
        timerFrequency = 10000000;
#else
        LARGE_INTEGER getFrequency;
        QueryPerformanceFrequency(&getFrequency);
        timerFrequency = (double) getFrequency.QuadPart;
#endif
#endif
    }

    static int64 getCounter() {
#ifdef TIMERS_IMPLEMENTED
#ifdef USE_THREAD_TIMER
        FILETIME creation;
        FILETIME exit;
        FILETIME system;
        FILETIME user;
        GetThreadTimes(GetCurrentThread(), &creation, &exit, &system, &user);
        return (((int64) user.dwHighDateTime) << 32) + user.dwLowDateTime;
#else
        LARGE_INTEGER getCount;
        QueryPerformanceCounter(&getCount);
        return getCount.QuadPart;
#endif
#else
	return 0;
#endif
    }

    void calibrateTimer() {
        totalTime = 0;
        correction = 0;
        startTimer();
        stopTimer();
        correction = totalTime;
    }

    //
    // Clear the timer.
    //
    void resetTimer() {
        setFrequency();
        isOn = false;
        recursionCount = 0;
        calibrateTimer();
        totalTime = 0;
    }
    
    //
    // Start the timer.  Continue timing from where we were.
    //
    void startTimer() {
        if (recursionCount++ == 0) {
            startTime = getCounter();
            isOn = true;
        }
    }
    //
    // Stop the timer.
    //
    void stopTimer() {
        if (--recursionCount == 0) {
            int64 finishTime = getCounter();
            int64 deltaT = (finishTime - startTime - correction);
            totalTime = totalTime + deltaT;
            isOn = false;
        }
    }

    int64 getTime() {
        return totalTime;
    }

    double getSeconds() {
        return (((double)totalTime) / timerFrequency);
    }

    double getFrequency() {
        return timerFrequency;
    }

    //
    // Print data on the event
    //
	void print(::std::ostream & os) {
        os << "Timer: " << name;
        os << "\t";
        os << getSeconds() << " seconds";
        os << ::std::endl;
    }

    Timer *getNext() {
        return next;
    }
};

class PhaseTimer {
    Timer* t;
public:
    PhaseTimer(Timer* &tin, const char *name) : t(tin) {
        if (!t) {
            tin = Jitrino::addTimer(name);
            t = tin;
        }
        if (Jitrino::flags.time)
            t->startTimer();
        else
            t = 0;
    }
    ~PhaseTimer() {
        if (t)
            t->stopTimer();
    }
};

class PhaseInvertedTimer {
    Timer* t;
public:
    PhaseInvertedTimer(Timer* &tin) : t(tin) {
        if (t)
            t->stopTimer();
    }
    ~PhaseInvertedTimer() {
        if (t)
            t->startTimer();
    }
};

} //namespace Jitrino 

#endif // _TIMER_H_
