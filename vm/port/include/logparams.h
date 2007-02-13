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
* @author Dmitry B. Yershov
*/  

#ifndef LOG_PARAMS_H
#define LOG_PARAMS_H

#include <iostream>
#include <vector>
#include "port_malloc.h"

#ifdef PLATFORM_NT
#define FMT64 "I64"
#else // !PLATFORM_NT
#define FMT64 "ll"
#endif // !PLATFORM_NT

using std::string;
using std::vector;

class LogParams {
private:
    vector<string> values;
    const char* def_messageId;
    const char* messageId;
    string result_string;
    int prefix, message_number, is_def_messageId_set;
public:

    LogParams(int pref, int mess_num) {
        prefix = pref;
        message_number = mess_num;
        is_def_messageId_set = 0;
        messageId = NULL;
    }

    ~LogParams() {
        STD_FREE((void*)def_messageId);
    }

    VMEXPORT const char* release();

    LogParams& operator<<(const char* message) {
        if (!is_def_messageId_set) {
            def_messageId = strdup(message);
            is_def_messageId_set = 1;
            return *this;
        } else {
            string logger_string;
            logger_string += message;
            values.push_back(logger_string);
            return *this;
        }
    }

    LogParams& operator<<(char* message) {
        if (!is_def_messageId_set) {
            def_messageId = strdup(message);
            is_def_messageId_set = 1;
            return *this;
        } else {
            string logger_string;
            logger_string += message;
            values.push_back(logger_string);
            return *this;
        }
    }

    LogParams& operator<<(volatile void* pointer) {
        string logger_string;
        char* buf = (char*)STD_MALLOC(21);
        sprintf(buf, "%p", pointer);
        logger_string += buf;
        STD_FREE(buf);
        values.push_back(logger_string);
        return *this;
    }

    LogParams& operator<<(const void* pointer) {
        string logger_string;
        char* buf = (char*)STD_MALLOC(21);
        sprintf(buf, "%p", pointer);
        logger_string += buf;
        STD_FREE(buf);
        values.push_back(logger_string);
        return *this;
    }

    LogParams& operator<<(void *pointer) {
        string logger_string;
        char* buf = (char*)STD_MALLOC(21);
        sprintf(buf, "%p", pointer);
        logger_string += buf;
        STD_FREE(buf);
        values.push_back(logger_string);
        return *this;
    }

    LogParams& operator<<(char c) {
        string logger_string;
        char* buf = (char*)STD_MALLOC(2*sizeof(char));
        sprintf(buf, "%c", c);
        logger_string += buf;
        STD_FREE(buf);
        values.push_back(logger_string);
        return *this;
    }

    LogParams& operator<<(int i) {
        string logger_string;
        char* buf = (char*)STD_MALLOC(21);
        sprintf(buf, "%d", i);
        logger_string += buf;
        STD_FREE(buf);
        values.push_back(logger_string);
        return *this;
    }

    LogParams& operator<<(long i) {
        string logger_string;
        char* buf = (char*)STD_MALLOC(50);
        sprintf(buf, "%ld", i);
        logger_string += buf;
        STD_FREE(buf);
        values.push_back(logger_string);
        return *this;
    }

    LogParams& operator<<(unsigned i) {
        string logger_string;
        char* buf = (char*)STD_MALLOC(50);
        sprintf(buf, "%u", i);
        logger_string += buf;
        STD_FREE(buf);
        values.push_back(logger_string);
        return *this;
    }

    LogParams& operator<<(unsigned long i) {
        string logger_string;
        char* buf = (char*)STD_MALLOC(100);
        sprintf(buf, "%lu", i);
        logger_string += buf;
        STD_FREE(buf);
        values.push_back(logger_string);
        return *this;
    }

    LogParams& operator<<(int64 i) {
        string logger_string;
        char* buf = (char*)STD_MALLOC(100);
        sprintf(buf, "%" FMT64 "d", i);
        logger_string += buf;
        STD_FREE(buf);
        values.push_back(logger_string);
        return *this;
    }

    LogParams& operator<<(uint64 i) {
        string logger_string;
        char* buf = (char*)STD_MALLOC(100);
        sprintf(buf, "%" FMT64 "u", i);
        logger_string += buf;
        STD_FREE(buf);
        values.push_back(logger_string);
        return *this;
    }

    LogParams& operator<<(double d) {
        string logger_string;
        char* buf = (char*)STD_MALLOC(100);
        sprintf(buf, "%lf", d);
        logger_string += buf;
        STD_FREE(buf);
        values.push_back(logger_string);
        return *this;
    }

    typedef std::ios_base& (*iomanip)(std::ios_base&);
    LogParams& operator<<(iomanip UNREF i) {
        //FIXME: NYI
        return *this;
    }

    typedef std::ostream& (*iomanip2)(std::ostream&);
    LogParams& operator<<(iomanip2 UNREF i) {
        //FIXME: NYI
        return *this;
    }
};

#endif //LOG_PARAMS_H
 
