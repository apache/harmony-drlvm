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
* @author Dmitry B. Yershov
* @version $Revision: 1.1.2.1.4.3 $
*/  

#ifndef LOGGER_STRING_H
#define LOGGER_STRING_H

#include <iostream>
#include "port_malloc.h"

using std::string;

class LoggerString {
private:
    string logger_string;
public:
    const char* release() {
        return (const char*)logger_string.c_str();
    }

    LoggerString& operator<<(const char* message) {
        logger_string += message;
        return *this;
    }

    LoggerString& operator<<(char* message) {
        logger_string += message;
        return *this;
    }

    LoggerString& operator<<(volatile void* pointer) {
        char* buf = (char*)STD_MALLOC(21);
        sprintf(buf, "%p", pointer);
        logger_string += buf;
        STD_FREE(buf);
        return *this;
    }

    LoggerString& operator<<(const void* pointer) {
        char* buf = (char*)STD_MALLOC(21);
        sprintf(buf, "%p", pointer);
        logger_string += buf;
        STD_FREE(buf);
        return *this;
    }

    LoggerString& operator<<(void *pointer) {
        char* buf = (char*)STD_MALLOC(21);
        sprintf(buf, "%p", pointer);
        logger_string += buf;
        STD_FREE(buf);
        return *this;
    }

    LoggerString& operator<<(char c) {
        char* buf = (char*)STD_MALLOC(2*sizeof(char));
        sprintf(buf, "%c", c);
        logger_string += buf;
        STD_FREE(buf);
        return *this;
    }

    LoggerString& operator<<(int i) {
        char* buf = (char*)STD_MALLOC(21);
        sprintf(buf, "%d", i);
        logger_string += buf;
        STD_FREE(buf);
        return *this;
    }

    LoggerString& operator<<(long i) {
        char* buf = (char*)STD_MALLOC(50);
        sprintf(buf, "%ld", i);
        logger_string += buf;
        STD_FREE(buf);
        return *this;
    }

    LoggerString& operator<<(unsigned i) {
        char* buf = (char*)STD_MALLOC(50);
        sprintf(buf, "%u", i);
        logger_string += buf;
        STD_FREE(buf);
        return *this;
    }

    LoggerString& operator<<(unsigned long i) {
        char* buf = (char*)STD_MALLOC(100);
        sprintf(buf, "%lu", i);
        logger_string += buf;
        STD_FREE(buf);
        return *this;
    }

    LoggerString& operator<<(int64 i) {
        char* buf = (char*)STD_MALLOC(100);
        sprintf(buf, "%" FMT64 "d", i);
        logger_string += buf;
        STD_FREE(buf);
        return *this;
    }

    LoggerString& operator<<(uint64 i) {
        char* buf = (char*)STD_MALLOC(100);
        sprintf(buf, "%" FMT64 "u", i);
        logger_string += buf;
        STD_FREE(buf);
        return *this;
    }

    LoggerString& operator<<(double d) {
        char* buf = (char*)STD_MALLOC(100);
        sprintf(buf, "%lf", d);
        logger_string += buf;
        STD_FREE(buf);
        return *this;
    }

    typedef std::ios_base& (*iomanip)(std::ios_base&);
    LoggerString& operator<<(iomanip UNREF i) {
        //FIXME: NYI
        return *this;
    }

    typedef std::ostream& (*iomanip2)(std::ostream&);
    LoggerString& operator<<(iomanip2 UNREF i) {
        //FIXME: NYI
        return *this;
    }
};

#endif //LOGGER_STRING_H
 
