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

#include <assert.h>
#include "port_crash_handler.h"

static port_signal_handler signal_callbacks[PORT_SIGNAL_MAX] =
{
    NULL, // PORT_SIGNAL_GPF
    NULL, // PORT_SIGNAL_STACK_OVERFLOW
    NULL, // PORT_SIGNAL_ABORT
    NULL, // PORT_SIGNAL_TRAP
    NULL, // PORT_SIGNAL_PRIV_INSTR
    NULL  // PORT_SIGNAL_ARITHMETIC
};

struct crash_additional_actions
{
    port_crash_handler_action action;
    crash_additional_actions *next;
};

static crash_additional_actions *crash_actions = NULL;

static unisigned crash_output_flags;

Boolean port_init_crash_handler(port_signal_handler_registration *registrations,
    unsigned count)
{
    if (initialize_signals() == FALSE)
        return FALSE;

    for (int iii = 0; iii < count; iii++)
    {
        assert(registrations[iii]->signum >= PORT_SIGNAL_MIN);
        assert(registrations[iii]->signum <= PORT_SIGNAL_MAX);
        signal_callbacks[registrations[iii]->signum] = registrations[iii]->callback;
    }

    return TRUE;
}

void port_crash_handler_set_flags(port_crash_handler_flags flags)
{
    crash_output_flags = flags;
}

Boolean port_crash_handler_add_action(port_crash_handler_action action)
{
    crash_additional_actions *a = STD_MALLOC(sizeof(crash_additional_actions));
    if (NULL == a)
        return FALSE;

    a->action = action;
    a->next = crash_actions;
    crash_actions = a;
    return TRUE;
}

Boolean port_shutdown_crash_handler()
{
    if (shutdown_signals() == FALSE)
        return FALSE;

    for (crash_additional_actions *a = crash_actions; NULL != a;)
    {
        crash_additional_actions *next = a->next;
        STD_FREE(a);
        a = next;
    }

    return TRUE;
}
