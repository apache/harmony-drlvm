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
* @author Alexey V. Varlamov, Dmitry B. Yershov
* @version $Revision: 1.1.2.1.4.3 $
*/  

// disable warning #4250 over /WX option in appach native interface for MS compiler
#if defined(_MSC_VER) && !defined (__INTEL_COMPILER) /* Microsoft C Compiler ONLY */
#pragma warning( push )
#pragma warning (disable:4250) //Two or more members have the same name. The one in class2 is inherited because it is a base class for the other classes that contained this member.
#endif

#include <stdarg.h>
#include "port_malloc.h"
#include "logger.h"
#include "logparams.h"
#include <log4cxx/logger.h>
#include <log4cxx/logmanager.h>
#include <log4cxx/level.h>
#include <log4cxx/consoleappender.h>
#include <log4cxx/fileappender.h>
#include <log4cxx/patternlayout.h>
#include <log4cxx/spi/filter.h>
#include <log4cxx/spi/location/locationinfo.h>
#include <log4cxx/filter/stringmatchfilter.h>
#include <log4cxx/helpers/transcoder.h>
#include <log4cxx/propertyconfigurator.h>
#include <apr_env.h>
#include "hyport.h"

#if defined(_MSC_VER) && !defined (__INTEL_COMPILER)
#pragma warning( pop )
#endif

#ifdef PLATFORM_NT
#define vsnprintf _vsnprintf
#endif


using namespace log4cxx;
using std::string;

static LevelPtr trace_levelPtr;
static LoggingLevel max_level = INFO;

static LogSite unusedLogSite = {UNKNOWN, 0};
static LogSite *lastLogSite = &unusedLogSite;

static void clear_cached_sites() {
    //FIXME thread unsafe
    LogSite *site = lastLogSite;
    while(site != &unusedLogSite) {
        site->state = UNKNOWN;
        site = site->next;
    }
}

void* portlib_for_logger = NULL;

const char* LogParams::release() {

    if (portlib_for_logger) {
        messageId = (char*) ((HyPortLibrary*)portlib_for_logger)->nls_lookup_message ((HyPortLibrary*)portlib_for_logger,
        HYNLS_DO_NOT_PRINT_MESSAGE_TAG | HYNLS_DO_NOT_APPEND_NEWLINE ,
        prefix, message_number, def_messageId);
    } else {
        messageId = def_messageId;
    }
    if (portlib_for_logger) {
        messageId = ((HyPortLibrary *)portlib_for_logger)->buf_write_text((struct HyPortLibrary *)portlib_for_logger, (const char *)messageId, (IDATA) strlen(messageId));
    }
    int i = 0;
    while(messageId[i] != '\0') {
        if (messageId[i] == '{' && messageId[i + 1] >= '0' &&
            messageId[i + 1] <= '9' && messageId[i + 2] == '}') {
                int arg = messageId[i + 1] - '0';
                result_string += values[arg];
                i += 3;
            } else {
                result_string += messageId[i];
                i++;
            }
    }
    if (portlib_for_logger) {
        ((HyPortLibrary *)portlib_for_logger)->mem_free_memory ((struct HyPortLibrary *)portlib_for_logger, (void*)messageId);
    }
    return (const char*)result_string.c_str();
}

static LevelPtr get_log4cxx_level(LoggingLevel level) {
    switch(level) {
        case DIE:
            return Level::getFatal();
        case WARN:
            return Level::getWarn();
        case INFO:
            return Level::getInfo();
        case LOG:
            return Level::getDebug();
        case TRACE:
            return trace_levelPtr;
        default:
            return Level::getWarn();
    }
}

static LoggerPtr get_logger(const char* category) {
    if (strcmp(category, "root") == 0) {
        return Logger::getRootLogger();
    } else {
        return Logger::getLogger(category);
    }
}

int set_locale(char* logger_locale) {
    char* lang = strdup(logger_locale);
    char* region = NULL;
    char* variant = NULL;
    if (portlib_for_logger) {
        region = strchr(lang, '_');
        if (region == NULL) {
            ((HyPortLibrary *)portlib_for_logger)->nls_set_locale((HyPortLibrary *)portlib_for_logger, lang, "", "");
            free((void*)lang);
            return 1;
        } else {
            region[0] = 0;
            region++;
            variant = strchr(region, '.');
            if (variant == NULL) {
                ((HyPortLibrary *)portlib_for_logger)->nls_set_locale((HyPortLibrary *)portlib_for_logger, lang, region, "");
                free((void*)lang);
                return 1;
            } else {
                variant[0] = 0;
                variant++;
                ((HyPortLibrary *)portlib_for_logger)->nls_set_locale((HyPortLibrary *)portlib_for_logger, lang, region, variant);
                free((void*)lang);
                return 1;
            }
        }
    }
    free((void*)lang);
    return 0;
}

void init_log_system(void *portlib) {
    int set_locale_success = 0;
    trace_levelPtr = new Level(Level::TRACE_INT, LOG4CXX_STR("TRACE"), 7);

    LoggerPtr logger = Logger::getRootLogger();
    ConsoleAppenderPtr cap = new ConsoleAppender(new PatternLayout(LOG4CXX_STR("%m%n")),LOG4CXX_STR("System.err"));
    logger->addAppender(cap);
    logger->setLevel(Level::getWarn());

    LoggerPtr info_logger = get_logger("info");
    info_logger->setLevel(Level::getInfo());

    portlib_for_logger = portlib;

    apr_pool_t *pool;
    apr_pool_create(&pool, 0);
    char* value;

    if (APR_SUCCESS == apr_env_get(&value, "LC_ALL", pool)) {
        if (set_locale(value)) {
            set_locale_success = 1;
        }
    } else if (APR_SUCCESS == apr_env_get(&value, "LC_MESSAGES", pool)) {
        if (!set_locale_success) {
            if (set_locale(value)) {
                set_locale_success = 1;
            }
        }
    } else if (APR_SUCCESS == apr_env_get(&value, "LANG", pool)) {
        if (!set_locale_success) {
            if (set_locale(value)) {
                set_locale_success = 1;
            }
        }
    }
    apr_pool_destroy(pool);

}

void shutdown_log_system() {
    LogManager::shutdown();
}

void set_logging_level_from_file(const char* filename) {
    string lfilename;
    lfilename += filename;
    try {
        PropertyConfigurator::configure(lfilename);
    } catch (...) {
        LoggerPtr logger = get_logger("logger");
        logger->log(get_log4cxx_level(WARN), "Couldn't initialize logging levels from file", 
            spi::LocationInfo::LocationInfo(__FILE__, __LOG4CXX_FUNC__, __LINE__));
    }
    max_level = TRACE; // Not easy to obtain actual value
    clear_cached_sites();
}

void log4cxx_from_c(const char *category, LoggingLevel level, const char* message, 
                    const char* file=0, const char* func = 0, int line=0) {
    LoggerPtr logger = get_logger(category);
    if (file == 0 || func == 0 || line == 0){
        logger->log(get_log4cxx_level(level), message, spi::LocationInfo::getLocationUnavailable());
    } else {
        logger->log(get_log4cxx_level(level), message, spi::LocationInfo::LocationInfo(file, func, line));
    }
}

void set_threshold(const char *category, LoggingLevel level) {
    LoggerPtr logger = get_logger(category);
    logger->setLevel(get_log4cxx_level(level));
    if (max_level < level) {
        max_level = level;
    }
    clear_cached_sites();
}

unsigned is_enabled(const char *category, LoggingLevel level) {
    LoggerPtr logger = get_logger(category);
    return logger->isEnabledFor(get_log4cxx_level(level));
}

unsigned is_warn_enabled(const char *category) {
    return (Logger::getLogger(category))->isEnabledFor(Level::getWarn());
}
unsigned is_info_enabled(const char *category) {
    return (Logger::getLogger(category))->isEnabledFor(Level::getInfo());
}
unsigned is_log_enabled(const char *category, LogSite *logSite) {
    if(!logSite->next) {
        //FIXME thread unsafe
        logSite->next = lastLogSite;
        lastLogSite = logSite;
    }
    // return cached value
    if (logSite->state != UNKNOWN) {
        return (DISABLED != logSite->state);
    }
    // no cache, calculate
    bool res = (max_level >= LOG) && Logger::getLogger(category)->isEnabledFor(Level::getDebug());
    logSite->state = res ? ENABLED : DISABLED;
    return res;
}

unsigned is_trace_enabled(const char *category, LogSite *logSite) {
    if(!logSite->next) {
        //FIXME thread unsafe
        logSite->next = lastLogSite;
        lastLogSite = logSite;
    }
    // return cached value
    if (logSite->state != UNKNOWN) {
        return (DISABLED != logSite->state);
    }
    // no cache, calculate
    bool res = (max_level >= TRACE) && Logger::getLogger(category)->isEnabledFor(trace_levelPtr);
    logSite->state = res ? ENABLED : DISABLED;
    return res;
}

inline static AppenderList getEffectiveAppenders(LoggerPtr logger) {
    AppenderList alist;
    while (logger && (alist = logger->getAllAppenders()).size() == 0 && logger->getAdditivity()) {
        logger = logger->getParent();
    }
    return alist;
}
void set_out(const char *category, const char* out) {   
    LoggerPtr logger = get_logger(category);
    if (out) {
        LogString lout;
        helpers::Transcoder::decode(out, strlen(out), lout);
        AppenderList alist = getEffectiveAppenders(logger);
        LayoutPtr layout;
        if (alist.size() != 0) {
            layout = alist[0]->getLayout();
        } else {
            layout = new PatternLayout(LOG4CXX_STR("%m%n"));
        }
        logger->removeAllAppenders();
        AppenderPtr fileap = new FileAppender(layout, lout, false);
        logger->setAdditivity(false);
        logger->addAppender(fileap);
    } else {
        logger->removeAllAppenders();
        logger->setAdditivity(true);
    }
}

void set_header_format(const char *category, HeaderFormat format) {
    bool header_not_empty = false;
    LogString str_format;
    LoggerPtr logger = get_logger(category);

    if (format & HEADER_LEVEL) {
        str_format.append(LOG4CXX_STR("%-5p "));
        header_not_empty = true;
    }
    if (format & HEADER_THREAD_ID) {
        str_format.append(LOG4CXX_STR("[%t] "));
        header_not_empty = true;
    }
    if (format & HEADER_TIMESTAMP) {
        str_format.append(LOG4CXX_STR("[%d] "));
        header_not_empty = true;
    }
    if (format & HEADER_CATEGORY) {
        str_format.append(LOG4CXX_STR("%c "));
        header_not_empty = true;
    }
    if (format & (HEADER_FILELINE | HEADER_FUNCTION)) {
        str_format.append(LOG4CXX_STR("("));
        if (format & HEADER_FUNCTION) {
            str_format.append(LOG4CXX_STR("%C::%M()"));
        }
        if (format & HEADER_FUNCTION && format & HEADER_FILELINE) {
            str_format.append(LOG4CXX_STR(" at "));
        }
        if (format & HEADER_FILELINE) {
            str_format.append(LOG4CXX_STR("%F:%L"));
        }
        str_format.append(LOG4CXX_STR(") "));
        header_not_empty = true;
    }

    if (header_not_empty) {
        str_format.append(LOG4CXX_STR(": "));
    }

    str_format.append(LOG4CXX_STR("%m%n"));

    AppenderList alist = getEffectiveAppenders(logger);
    AppenderList::iterator appender;
    for (appender = alist.begin(); appender != alist.end(); appender++)
    {
        (*appender)->setLayout(new PatternLayout(str_format));
    }
}

void set_thread_specific_out(const char* category, const char* pattern) {
    return;
}

VMEXPORT const char* log_printf(const char* format, ...){
    va_list args;
    va_start(args, format);
    int length = 255;
    char *message = (char*)STD_MALLOC(sizeof(char)*length);
    while(1){
        int count = vsnprintf(message, length, format, args);
        if(count > -1 && count < length)
            break;
        length *= 2;
        message = (char*)STD_REALLOC(message, sizeof(char)*length);
    }
    va_end(args);
    return message;
}
