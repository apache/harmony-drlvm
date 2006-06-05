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
 * @author Alexander V. Astapchuk
 * @version $Revision: 1.3.12.3.4.4 $
 */
 
/**
 * @file
 * @brief Definitions for statistics and measurement utilities.
 */

#if !defined(__STATS_H_INCLUDED__)
#define __STATS_H_INCLUDED__

#include "jdefs.h"
#include <string>


namespace Jitrino {
namespace Jet {

#if defined(JIT_TIMING)
#include <winsock2.h>
#include <windows.h>
#endif  // if defined(JIT_TIMING)



/**
 * @brief Class for quick and rough estimations of time spent in various
 *        parts of program.
 *
 * Normally, Timer instances are created as static and/or global variables.
 * They are automatically connected to a single linked list and may be 
 * printed out by a single #Timer::dump call.
 *
 * @note To use Timers, a macro JIT_TIMING must be defined. Otherwise, all 
 *       its methods are no-ops.
 *
 * So, the Timer-s usage may be left in the code without any conditional 
 * macros, and they do not contribute any runtime overhead until a macro 
 * JIT_TIMING defined. An overhead to Timer's #start() and #stop() calls is 
 * estimated in #init() and then used to correct the measured values.
 *
 * @note Only Windows implementation currently exists.
 */
class Timer {
public:
//#ifndef DOXYGEN_SHOULD_SKIP_THIS
#if !defined(JIT_TIMING)
    Timer(void) {};
    Timer(const char *) {};
    void startTimer(void) const {};
    void stopTimer(void) const {};
    void resetTimer(void) const {};
    void start(void) const {};
    void stop(void) const {};
    static void init(void) {};
    static void dump(void) {};
#else   // if !defined(JIT_TIMING)
//#endif  //ifndef DOXYGEN_SHOULD_SKIP_THIS
    /**
     * @brief A special-purpose constructor, normally used only during an 
     *        init() routine.
     * 
     * @note Does not register the instance in the Timers chain.
     */
    Timer(void) : totalTime(0), recursionCount(0), startTime(0)
    {
    }

    /**
     * @brief Creates named instance of Timer, inserts the instance in the 
     *        list of Timer-s.
     */
    Timer(const char *_name) : name(_name), totalTime(0), 
                               recursionCount(0), startTime(0)
    {
        if (!first) {
            first = this;
        }
        if (last) {
            last->next = this;
        }
        next = NULL;
        last = this;
    }
    

    /**
     * @brief Initializes the Timers framework, counting overhead spent to 
     *        start/stop pair invocation.
     */
    static void init(void);
    
    /**
     * @brief Prints out all Timer-s (their names and values) in the list.
     */
    static void dump(void);

    /**
     * @brief Returns current value  of underlying high-resolution timer.
     */
    static unsigned long long getCounter(void)
    {
        LARGE_INTEGER getCount;
        QueryPerformanceCounter(&getCount);
        return getCount.QuadPart;
    }

    /**
     * @brief Clears the Timer.
     */
    void resetTimer(void)
    {
        recursionCount = 0;
        totalTime = 0;
    }

    /**
     * @brief Alias for #start().
     */
    void startTimer(void)
    {
        start();
    }
    /**
     * @brief Starts the timer.
     */
    void start(void)
    {
        if (recursionCount++ == 0) {
            startTime = getCounter();
        }
    }

    /**
     * @brief Alias for #stop().
     */
    void stopTimer(void)
    {
        stop();
    }

    /**
     * @brief Stops the timer.
     */
    void stop(void)
    { 
        if (--recursionCount == 0) {
            const unsigned long long finishTime = getCounter();
            long long deltaT = (finishTime - startTime - correction);
            if (deltaT<0) {
                deltaT = 0;
            }
            totalTime = totalTime + deltaT;
        }
    }
    
    /**
     * @brief Returns a time interval measured by the Timer.
     */
    unsigned long long getTime(void) const
    {
        return totalTime;
    }

    /**
     * @brief Returns value of the Timer in seconds, with a given accuracy.
     *
     * I.e. is the measured time interval is '0.1s', then getSeconds(10) 
     * returns '1', and getSeconds(1000) returns '100'.
     */
    unsigned long long getSeconds(unsigned accuracy = 100000)
    {
        return (totalTime*accuracy)/freq;
    }

    /**
     * @brief Returns name of the timer.
     */
    const char * getName(void) const
    {
        return name;
    }

    /**
     * @brief Returns next timer in the list, or NULL if this is last timer.
     */
    Timer * getNext(void) const
    {
        return next;
    }

    /**
     * @brief Returns very first Timer in a list of Timers.
     */
    static Timer * getFirst(void)
    {
        return first;
    }

private:
    /**
     * @brief Frequency (ticks-per-second) of the underlying OS 
     *        high-resolution timer.
     */
    static unsigned long long   freq;

    /**
     * @brief Estimated overhead of start/stop calls, including calls to 
     *        OS' API functions.
     */
    static unsigned long long   correction;

    // actually, a pointer to a head is much enough for enumeration but in
    // order to preserve the natural order of the Timers definitions 2 
    // pointers are used

    /**
     * @brief The 'first' field point onto the head of the list.
     *
     * The Timers are linked together in a single-linked list for easy 
     * enumeration and printing.
     */
    static Timer * first;

    /**
     * @brief The 'last' field point to the tail of the list for fast 
     *        insertion.
     */
    static Timer * last;

    /**
     * @brief Name of the Timer.
     */
    const char * name;

    /**
     * @brief Start value.
     */
    unsigned long long startTime;

    /**
     * @brief Total time measured by this timer.
     */
    unsigned long long totalTime;

    /**
     * @brief Depth of recursion (repetitive calls of #start()/#stop()).
     */
    int recursionCount;

    /**
     * @brief Pointer to a next timer.
     */
    Timer * next;
#ifndef DOXYGEN_SHOULD_SKIP_THIS
#endif      // if !defined(JIT_TIMING)
#endif  //ifndef DOXYGEN_SHOULD_SKIP_THIS
};


/**
 * @brief A simple holder only to visually designate timers through the 
 *        sources.
 */
class Timers {
public:
    /**
     * @brief Time spent in VM's resolve_xx methods.
     */
    static Timer    vmResolve;
    /**
     * @brief Total lifetime - time between JIT_Init() and JIT_Deinit() calls.
     */
    static Timer    totalExec;
    /**
     * @brief Summarized time of compilation.
     */
    static Timer    compTotal;
    /**
     * @brief Time spent for initialization routines.
     */
    static Timer    compInit;
    /**
     * @brief Time spent for Compiler::bbs_mark_all().
     */
    static Timer    compMarkBBs;
    /**
     * @brief Time spent for Compiler::bbs_gen_code().
     */
    static Timer    compCodeGen;
    /**
     * @brief TIme spent for Compiler::bbs_layout_code().
     */
    static Timer    compCodeLayout;
    /**
     * @brief Time spent for code patching.
     */
    static Timer    compCodePatch;
    /**
     * @brief Time spent Compiler::bbs_ehandlers_set().
     */
    static Timer    compEhandlers;
};

/**
 * @brief Class for simple statistics collection and dump.
 *
 * All of its operations are noops until a macro JIT_STATS defined, so 
 * the operations may be left in code without any runtime overhead.
 */
class Stats {
private:
    // only statics
    Stats();
    Stats(const Stats&);
    Stats& operator=(const Stats&);
public:
    /**
     * @brief An empty type, whose all methods and operators are no-ops.
     *
     * Used to replace any other type when no JIT_STATS macro defined.
     */
    class EmptyType {
    public:
        // various ctors
        EmptyType(void) {};
        EmptyType(const char *) {};
        EmptyType(unsigned) {};
        EmptyType(float) {};
        EmptyType(double) {};
        EmptyType(const EmptyType&) {};
        //
        EmptyType& operator=(const EmptyType&)  { return *this; }
        EmptyType& operator=(unsigned)          { return *this; }
        EmptyType& operator=(const char*)       { return *this; }
        //
        bool operator==(const EmptyType&)       { return false; }
        bool operator!=(const EmptyType&)       { return false; }
        bool operator<=(const EmptyType&)       { return false; }
        bool operator>=(const EmptyType&)       { return false; }
        EmptyType& operator++(void)             { return *this; }
        EmptyType& operator++(int)              { return *this; }
        EmptyType& operator+=(const EmptyType&) { return *this; }
        operator unsigned()                     { return 0;     }
    };
#ifdef JIT_STATS
    /**
     * @brief A macro used to declare a variable of type \c typ, but 
     *        replaces its type to #EmptyType when no JIT_STATS defined.
     */
    #define STATS_ITEM(typ) typ
    /**
     * @brief Prints out collected statistics.
     */
    static void dump(void);
#else
    #define STATS_ITEM(typ) Stats::EmptyType
    static void dump(void) {}
#endif
    /**
     * @brief String filter for collected statistics.
     * 
     * Introduces a filter (trough the strstr(fully-qualified-name, filter)
     * to collect statistics about.
     */
    static const char *                 g_name_filter;
    
    static STATS_ITEM(unsigned)         opcodesSeen[OPCODE_COUNT];
    /// how many methods compiled
    static STATS_ITEM(unsigned)         methodsCompiled;
    static STATS_ITEM(unsigned)         methodsCompiledSeveralTimes;
    /// Methods without catch handlers
    static STATS_ITEM(unsigned)         methodsWOCatchHandlers;
    //
    
    /// How many null checks are eliminated
    static STATS_ITEM(unsigned)         npesEliminated;
    static STATS_ITEM(unsigned)         npesPerformed;
    
        
#define DEF_MIN_MAX_VALUE(what)    \
    STATS_ITEM(unsigned)        Stats::what##_total = (unsigned)0;    \
    STATS_ITEM(unsigned)        Stats::what##_min = (unsigned)0;      \
    STATS_ITEM(::std::string)   Stats::what##_min_name; \
    STATS_ITEM(unsigned)        Stats::what##_max = (unsigned)0;      \
    STATS_ITEM(::std::string)   Stats::what##_max_name;

#define DECL_MIN_MAX_VALUE(what)    \
    static STATS_ITEM(unsigned)         what##_total;    \
    static STATS_ITEM(unsigned)         what##_min;      \
    static STATS_ITEM(::std::string)    what##_min_name; \
    static STATS_ITEM(unsigned)         what##_max;      \
    static STATS_ITEM(::std::string)    what##_max_name;

#ifdef JIT_STATS
    #define STATS_SET_NAME_FILER(nam) Stats::g_name_filter = nam
    #define STATS_INC(what, howmany)    \
        if ( (NULL == Stats::g_name_filter) || \
            (NULL != strstr(m_fname, Stats::g_name_filter))) { \
            what += howmany; \
        }
        
    #define STATS_MEASURE_MIN_MAX_VALUE( what, value, nam ) \
    { \
        if ( (NULL == Stats::g_name_filter) || \
             (NULL != strstr(m_fname, Stats::g_name_filter)) ) { \
        Stats::what##_total += value; \
        if (Stats::what##_max < value) { \
            Stats::what##_max = value; \
            Stats::what##_max_name = nam; \
        }; \
        static bool what##_done = false; \
        if (!what##_done && Stats::what##_min > value) { \
            what##_done = true; \
            Stats::what##_min = value; \
            Stats::what##_min_name = nam; \
        }; \
        }\
    }
#else   // JIT_STATS
    #define STATS_SET_NAME_FILER(nam)
    #define STATS_INC(what, howmany)    
    #define STATS_MEASURE_MIN_MAX_VALUE( what, value, nam )
#endif  // JIT_STATS
    
    // byte code size
    DECL_MIN_MAX_VALUE( bc_size );
    // native code size
    DECL_MIN_MAX_VALUE( code_size );
    // native code size / byte code size ratio
    DECL_MIN_MAX_VALUE( native_per_bc_ratio );
    // stack depth
    DECL_MIN_MAX_VALUE( jstack );
    // number of local slots
    DECL_MIN_MAX_VALUE( locals );
    // number of basic blocks
    DECL_MIN_MAX_VALUE( bbs );
    // size of the basic blocks
    DECL_MIN_MAX_VALUE( bb_size );
    //
    DECL_MIN_MAX_VALUE( patchItemsToBcSizeRatioX1000 );
};


}
} // ~namespace Jitrino::Jet


#endif  // ~__STATS_H_INCLUDED__

