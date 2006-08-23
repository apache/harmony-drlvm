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
 * @author Salikh Zakirov
 * @version $Revision$
 */
#include "testframe.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>


/**
   @file testframe.c
   testframe.c contains implementation of Testframe framework functions
   @ingroup testframe
*/

int log_level = LOG_LEVEL_INFO;
int error_flag = 0;

void log_error(char *fmt, ...) {
    va_list argp;
    if (error_flag) return;
    error_flag = 1;
    fprintf(stderr, "ERROR: ");
    va_start(argp, fmt);
    vfprintf(stderr, fmt, argp);
    va_end(argp);
    fprintf(stderr, "\n");
    fflush(NULL);
}

void log_info(char *fmt, ...) {
    va_list argp;

    if (log_level<LOG_LEVEL_INFO) return;
    fprintf(stdout, "INFO: ");
    va_start(argp, fmt);
    vfprintf(stdout, fmt, argp);
    va_end(argp);
    fprintf(stdout, "\n");
    fflush(NULL);
}

void log_debug(char *fmt, ...) {
    va_list argp;
        
    if (log_level<LOG_LEVEL_DEBUG) return;
    fprintf(stdout, "DEBUG: ");
    va_start(argp, fmt);
    vfprintf(stdout, fmt, argp);
    va_end(argp);
    fprintf(stdout, "\n");
    fflush(NULL);
}

int execute(char *name, int (*f)(void)) {
    int status; 
    if (f==NULL) {
        log_error("no test function to execute");       
        return TEST_ERROR;
    }
    log_info("TEST %s start", name);
    error_flag = 0;
    setup();
    status = f();
    if (status || error_flag){
        log_info("TEST %s: FAILED", name);
    } else {
        log_info("TEST %s: PASSED", name);
    }
    teardown();
    return status;
}

void log_set_level(int level) {
    log_level = level;
}

int default_main(int argc, char *argv[]){
        
    int result;

    TestDescriptor *p;

    log_set_level(LOG_LEVEL_INFO);

    if (argc>=2) {
        /* execute given test only */
        for (p=testDescriptor; (p->name!=NULL) && (strncmp(argv[1], p->name, strlen(p->name))!=0); p++) {}
                                
        if ((p->name!=NULL) && (p->func!=NULL)) {
            return (execute(p->name, p->func));
            fflush(NULL);
        }
    } else {
        /* execute all tests */
        result = 0;
        for (p=testDescriptor; (p->name!=NULL); p++) {
            //log_debug("executing test %s", p->name);
            if (p->func!=NULL) { 
                result = execute(p->name, p->func) || result;
                fflush(NULL);
            }
        }
        return result;
    }
    return (0);
}

