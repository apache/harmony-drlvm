/*
 *  Licensed to the Apache Software Foundation (ASF) under one or more
 *  contributor license agreements.  See the NOTICE file distributed with
 *  this work for additional information regarding copyright ownership.
 *  The ASF licenses this file to You under the Apache License, Version 2.0
 *  (the "License"); you may not use this file except in compliance with
 *  the License.  You may obtain a copy of the License at
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
        start = apr_time_now();
    }

    Timer(const char *str, const char *_category) {
        action = str;
        category = _category;
        finished = false;
        start = apr_time_now();
    }

    void finish() {
        finished = true;
        apr_time_t end = apr_time_now();
        INFO2(category, action << " " << (end - start + 500) / 1000 << " ms");
    }

    ~Timer() {
        if (!finished) finish();
    }

    apr_time_t dt() { return apr_time_now() - start; }
};

inline void timer_init() {}
inline void timer_calibrate(apr_time_t time_from_start) {}

#else
#  define TIME(action_name, action_params) action_name action_params
#  define timer_init()
#  define timer_calibrate()
#endif
