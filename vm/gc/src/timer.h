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
 * @author Ivan Volosyuk
 */

#define STATISTICS
#ifdef STATISTICS
#  define TIME(action_name, action_params) { Timer timer_##action_name(#action_name); \
                                           action_name action_params;               \
                                           timer_##action_name.finish(); }

#ifdef _WIN32
static __declspec(naked) __int64
ticks(void) {
    __asm       {
        rdtsc
        ret
    }
}
#else
static int64
ticks(void) {
    int64 val;
    __asm__ __volatile__ ("rdtsc" : "=A" (val));
    return val;
}
#endif

extern int64 timer_start;
extern int64 timer_dt;

class Timer {
    const char *action;
    const char *category;
    apr_time_t start;
    bool finished;
    public:
    
    Timer(const char *str) {
        action = str;
        finished = false;
        category = "gc.time";
        start = ticks();
    }

    Timer(const char *str, const char *_category) {
        action = str;
        category = _category;
        finished = false;
        start = ticks();
    }

    void finish() {
        finished = true;
        apr_time_t end = ticks();
        INFO2(category, action << " " << (end - start) / timer_dt / 1000 << " ms");
    }

    ~Timer() {
        if (!finished) finish();
    }

    apr_time_t dt() { return ticks() - start; }
};

inline void timer_init() {
    timer_start = ticks();
}

inline void timer_calibrate(apr_time_t time_from_start) {
    int64 ticks_from_start = ticks() - timer_start;
    int64 dt = ticks_from_start / time_from_start;
    timer_dt = dt;
    
}

#else
#  define TIME(action_name, action_params) action_name action_params
#  define timer_init()
#  define timer_calibrate()
#endif
