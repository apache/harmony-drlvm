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
 * @author Gregory Shimansky
 * @version $Revision: 1.1.2.1.4.5 $
 */  
#ifndef _JVMTI_INTERNAL_H_
#define _JVMTI_INTERNAL_H_

#include "jvmti_utils.h"
#include "vm_threads.h"
#include "jit_export_jpda.h"
#include <apr_dso.h>
#include <apr_strings.h>
#include "log_macro.h"

//using namespace du_ti;

typedef jint (JNICALL *f_Agent_OnLoad)
    (JavaVM *vm, char *options, void *reserved);

typedef void (JNICALL *f_Agent_OnUnLoad)
    (JavaVM *vm);

struct Agent
{
    const char* agentName;
    jint agent_id;
    jboolean dynamic_agent;
    apr_dso_handle_t* agentLib;
    apr_pool_t* pool;
    f_Agent_OnLoad Agent_OnLoad_func;
    f_Agent_OnUnLoad Agent_OnUnLoad_func;
    Agent* next;
    Agent(const char *name):
        agentLib(NULL),
        Agent_OnLoad_func(NULL),
        Agent_OnUnLoad_func(NULL),
        next(NULL){
        apr_pool_create(&pool, 0);
        agentName = apr_pstrdup(pool, name);
    }
    
    ~Agent() {
        apr_pool_destroy(pool);
    }
};

struct TIEnvList
{
    TIEnv *env;
    TIEnvList *next;
};

struct jvmti_frame_pop_listener
{
    jint depth;
    TIEnv *env;
    jvmti_frame_pop_listener *next;
};

/*
 * Type which will describe one breakpoint
 */
struct BreakPoint {
    jmethodID method;
    jlocation location;
    void *id;
    BreakPoint *next;
    TIEnvList *envs;

    TIEnvList *find_env(TIEnv *env)
    {
        for(TIEnvList *el = envs; NULL != el; el = el->next)
            if (el->env == env)
                return el;
        return NULL;
    }

    void add_env(TIEnvList *el)
    {
        // FIXME: linked list modification without synchronization
        el->next = envs;
        envs = el;
    }

    void remove_env(TIEnvList *el)
    {
        assert(envs);

        // FIXME: linked list modification without synchronization
        if (envs == el)
        {
            envs = envs->next;
            _deallocate((unsigned char *)el);
            return;
        }

        for (TIEnvList *p_el = envs->next; NULL != p_el->next; p_el = p_el->next)
            if (p_el->next == el)
            {
                p_el->next = el->next;
                _deallocate((unsigned char *)el);
                return;
            }

        ABORT("Can't find the element");
    }

    BreakPoint() : method(NULL), location(0), next(NULL), envs(NULL) {}
};

struct DynamicCode
{
    const char *name;
    const void *address;
    jint length;
    DynamicCode *next;
};

typedef struct Class Class;

class DebugUtilsTI {
    public:
        jint agent_counter;

        DebugUtilsTI();

        ~DebugUtilsTI();
        jint Init();
        void Shutdown();
        void setInterpreterState(Global_Env *p_env);
        int getVersion(char* version);
        void addAgent(const char*); // add agent name (string)
        Agent *getAgents();
        void setAgents(Agent *agent);
        bool isEnabled();
        void setEnabled();
        void setDisabled();

        jvmtiPhase getPhase()
        {
            return phase;
        }

        void nextPhase(jvmtiPhase phase)
        {
            this->phase = phase;
        }

        void addEnvironment(TIEnv *env)
        {
            // FIXME: thread unsafe modification
            env->next = p_TIenvs;
            p_TIenvs = env;
        }

        void removeEnvironment(TIEnv *env)
        {
            TIEnv *e = p_TIenvs;

            if (NULL == e)
                return;

            // FIXME: thread unsafe modification of the list
            if (e == env)
            {
                p_TIenvs = env->next;
                return;
            }

            while (NULL != e->next)
            {
                if (e->next == env)
                {
                    e->next = env->next;
                    return;
                }
                e = e->next;
            }
        }

        TIEnv *getEnvironments(void)
        {
            return p_TIenvs;
        }

        DynamicCode *getDynamicCodeList(void)
        {
            return dcList;
        }

        BreakPoint *find_breakpoint(jmethodID m, jlocation l)
        {
            for (BreakPoint *bp = brkpntlst; NULL != bp; bp = bp->next)
                if (bp->method == m && bp->location == l)
                    return bp;

            return NULL;
        }

        bool have_breakpoint(jmethodID m)
        {
            for (BreakPoint *bp = brkpntlst; NULL != bp; bp = bp->next)
                if (bp->method == m)
                    return true;

            return false;
        }

        BreakPoint* find_first_bpt(jmethodID m)
        {
            for (BreakPoint *bp = brkpntlst; NULL != bp; bp = bp->next)
                if (bp->method == m)
                    return bp;

            return NULL;
        }

        BreakPoint* find_next_bpt(BreakPoint* bpt, jmethodID m)
        {
            for (BreakPoint *bp = bpt->next; NULL != bp; bp = bp->next)
                if (bp->method == m)
                    return bp;

            return NULL;
        }

        void add_breakpoint(BreakPoint *bp)
        {
            // FIXME: linked list modification without synchronization
            bp->next = brkpntlst;
            brkpntlst = bp;
        }

        void remove_breakpoint(BreakPoint *bp)
        {
            assert(brkpntlst);

            if (bp == brkpntlst)
            {
                brkpntlst = bp->next;
                _deallocate((unsigned char *)bp);
                return;
            }

            // FIXME: linked list modification without synchronization
            for (BreakPoint *p_bp = brkpntlst; NULL != p_bp->next; p_bp = p_bp->next)
                if (p_bp->next == bp)
                {
                    p_bp->next = bp->next;
                    _deallocate((unsigned char *)bp);
                    return;
                }

            ABORT("Can't find the breakpoint");
        }

        void SetPendingNotifyLoadClass( Class *klass );
        void SetPendingNotifyPrepareClass( Class *klass );
        unsigned GetNumberPendingNotifyLoadClass();
        unsigned GetNumberPendingNotifyPrepareClass();
        Class * GetPendingNotifyLoadClass( unsigned number );
        Class * GetPendingNotifyPrepareClass( unsigned number );
        void ReleaseNotifyLists();

        enum GlobalCapabilities {
            TI_GC_ENABLE_METHOD_ENTRY           = 0x01,
            TI_GC_ENABLE_METHOD_EXIT            = 0x02,
            TI_GC_ENABLE_FRAME_POP_NOTIFICATION = 0x04,
            TI_GC_ENABLE_SINGLE_STEP            = 0x08,
            TI_GC_ENABLE_EXCEPTION_EVENT        = 0x10
        };

        void set_global_capability(GlobalCapabilities ti_gc)
        {
            global_capabilities |= ti_gc;
        }

        void reset_global_capability(GlobalCapabilities ti_gc)
        {
            global_capabilities &= ti_gc;
        }

        unsigned get_global_capability(GlobalCapabilities ti_gc)
        {
            return global_capabilities & ti_gc;
        }

    private:

    protected:
        friend jint JNICALL create_jvmti_environment(JavaVM *vm, void **env, jint version);
        BreakPoint *brkpntlst;
        bool status;
        Agent* agents;
        TIEnv* p_TIenvs;
        jvmtiPhase phase;
        DynamicCode *dcList;
        const unsigned MAX_NOTIFY_LIST;
        Class **notifyLoadList;
        unsigned loadListNumber; 
        Class **notifyPrepareList;
        unsigned prepareListNumber;
        unsigned global_capabilities;
}; /* end of class DebugUtilsTI */

jvmtiError add_event_to_thread(jvmtiEnv *env, jvmtiEvent event_type, jthread event_thread);
void remove_event_from_thread(jvmtiEnv *env, jvmtiEvent event_type, jthread event_thread);
void add_event_to_global(jvmtiEnv *env, jvmtiEvent event_type);
void remove_event_from_global(jvmtiEnv *env, jvmtiEvent event_type);
jthread getCurrentThread();

jint load_agentlib(Agent *agent, const char *str, JavaVM_Internal *vm);
jint load_agentpath(Agent *agent, const char *str, JavaVM_Internal *vm);

// Object check functions
Boolean is_valid_thread_object(jthread thread);

// JIT support
jvmtiError jvmti_translate_jit_error(OpenExeJpdaError error);

#endif /* _JVMTI_INTERNAL_H_ */
