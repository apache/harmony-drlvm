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
 * @version $Revision: 1.23.4.1.4.3 $
 */  

#define LOG_DOMAIN "interpreter.unspecified"
#include "cxxlog.h"

#include "environment.h"
#include "vm_threads.h"
#include "open/bytecodes.h"
#include "open/vm_util.h"
#include "ini.h"
#include "jvmti_types.h"

//#define INTERPRETER_DEEP_DEBUG
#define DEBUG_PRINT(a) TRACE2("interpreter", a)

#define DEBUG(a)

#ifdef NDEBUG
#  define DEBUG_BYTECODE(a)
#else
#  define DEBUG_BYTECODE(a) { if (frame.dump_bytecodes)  DEBUG_PRINT(a); }
#endif
#define DEBUG_GC(a)             TRACE2("gc_interpreter", a)

#define DEBUG2(a) INFO(a)

extern bool interpreter_enable_debug;
#define DEBUG_TRACE_PLAIN(a)    TRACE2("interpreter", a)
#define DEBUG_TRACE(a)          TRACE2("folded_interpreter", a)

#define ASSERT_TAGS(a)

#define ASSERT_OBJECT(a) assert((a == 0) || ((*((a)->vt()->clss->get_class_handle()))->vt()->clss == VM_Global_State::loader_env->JavaLangClass_Class))

#ifndef INTERPRETER_USE_MALLOC_ALLOCATION
#define ALLOC_FRAME(sz) alloca(sz)
#define FREE_FRAME(ptr)
#else
#define ALLOC_FRAME(sz) m_malloc(sz)
#define FREE_FRAME(ptr) m_free(ptr)
#endif

/***** Compressed pointers on *****/
#if defined _IPF_ || defined _EM64T_
#define COMPRESS_MODE
#endif

#if defined _IPF_ || defined _EM64T_
#  define COMPACT_FIELDS
#  define uword uint64
#  define word int64
#  ifdef COMPRESS_MODE
#    define CREF COMPRESSED_REFERENCE
#    define PTR32
#  else
#    define CREF ManagedObject*
#  endif
#else
// no define for: COMPACT_FIELDS
#    define uword uint32
#    define word int32

#    define CREF uint32
#    define PTR32
#endif

#ifdef COMPRESS_MODE

#define COMPRESS_REF(ref) compress_reference(ref)
#define UNCOMPRESS_REF(cref) uncompress_compressed_reference(cref)

#else /* ! COMPRESS_MODE */

static inline CREF
fake_compress_reference(ManagedObject *obj) {
    return (CREF) obj;
}
static inline ManagedObject*
fake_uncompress_compressed_reference(CREF compressed_ref) {
    return (ManagedObject*) compressed_ref;
}

#define COMPRESS_REF(ref) fake_compress_reference(ref)
#define UNCOMPRESS_REF(cref) fake_uncompress_compressed_reference(cref)

#endif

// Defines byte ordering in Value2 in different situations
#define s0 1 // stack val, NOTE: Values on java stack placed in reversed order
#define s1 0 // so that reversed copy in function call to work correctly
#define l0 0 // local val
#define l1 1
#define c0 0 // const val
#define c1 1
#define a0 0 // arg val
#define a1 1
#define ar0 0 // array val
#define ar1 1
#define res0 1
#define res1 0


union Value {
    uint32 u;
    int32 i;
    float f;
    CREF cr;
};

union Value2 {
#ifdef PTR32
    Value v[2];
#else
    Value v0;
#endif
    int64 i64;
    uint64 u64;
    double d;
};

enum {
    FLAG_NONE = 0,
    FLAG_RET_ADDR = 2,
    FLAG_OBJECT = 3
};

enum PopFrameState {
    POP_FRAME_UNAVAILABLE,
    POP_FRAME_AVAILABLE,
    POP_FRAME_NOW
};

class Stack {
    Value *data;
    uint8 *refs;
    int32 index;
    int32 size;

    public:
    inline Stack() {}
    inline ~Stack();

    inline void init(void *ptr, int size);

    // get reference to value on top of stack
    inline Value& pick(int offset = 0);

    // set/reset value to be object reference
    inline uint8& ref(int offset = 0);

    // just move stack pointer
    inline void push(int size = 1);
    inline void pop(int size = 1);
    inline void popClearRef(int size = 1);

    inline void setLong(int idx, Value2 val);
    inline Value2 getLong(int idx);

    // Clear stack
    inline void clear();

    static inline int getStorageSize(int size);
    inline int getIndex() { return index + 1; }
    friend void interp_enumerate_root_set_single_thread_on_stack(VM_thread*);
    friend void interp_ti_enumerate_root_set_single_thread_on_stack(jvmtiEnv* ti_env, VM_thread *thread);
};

class Locals {
    Value *var;
    uint8 *refs;
    uint32 varNum;

    public:
    inline Locals() {}
    inline ~Locals();

    inline void init(void *ptr, uint32 size);
    inline Value& operator () (uint32 id);
    inline void setLong(int idx, Value2 val);
    inline Value2 getLong(int idx);

    inline uint8& ref(uint32 id);

    static inline int getStorageSize(int size);
    inline uint32 getLocalsNumber() { return varNum; }

    friend void interp_enumerate_root_set_single_thread_on_stack(VM_thread*);
    friend void interp_ti_enumerate_root_set_single_thread_on_stack(jvmtiEnv* ti_env, VM_thread *thread);
};

struct FramePopListener {
    void *listener;
    FramePopListener *next;
};

struct MonitorList {
    ManagedObject *monitor;
    MonitorList *next;
};

struct StackFrame {
    public:
    uint8 *ip;
    Stack stack;
    Locals locals;
    Method *method;
    StackFrame *prev;
    FramePopListener *framePopListener;
    ManagedObject *This;
    struct MonitorList *locked_monitors;
    struct MonitorList *free_monitors;
    PopFrameState jvmti_pop_frame;
#ifndef NDEBUG
    bool dump_bytecodes;
#endif
#ifdef INTERPRETER_DEEP_DEBUG
    uint8 last_bytecodes[8];
    int n_last_bytecode;
#endif
    ManagedObject *exc;
    ManagedObject *exc_catch;
};

/********* PROTOTYPES ********/
extern uint8 Opcode_BREAKPOINT(StackFrame& frame);
extern void interp_enumerate_root_set_single_thread_on_stack(VM_thread*);
extern void interpreter_execute_native_method(
        Method *method, jvalue *return_value, jvalue *args);
extern void interpreterInvokeStaticNative(
        StackFrame& prevFrame, StackFrame& frame, Method *method);
extern void interpreterInvokeVirtualNative(
        StackFrame& prevFrame, StackFrame& frame, Method *method, int sz);

extern void interpreter_execute_method(
        Method *method, jvalue *return_value, jvalue *args);

void method_entry_callback(Method *method);
void method_exit_callback(Method *method, bool was_popped_by_exception, jvalue ret_val);
void method_exit_callback_with_frame(Method *method, StackFrame& frame);
void putfield_callback(Field *field, StackFrame& frame);
void getfield_callback(Field *field, StackFrame& frame);
void putstatic_callback(Field *field, StackFrame& frame);
void getstatic_callback(Field *field, StackFrame& frame);
void frame_pop_callback(FramePopListener *l, Method *method, jboolean was_popped_by_exception);
void single_step_callback(StackFrame &frame);
bool findExceptionHandler(StackFrame& frame, ManagedObject **exception, Handler **h);
bool load_method_handled_exceptions(Method *m);

/********* INLINE FUNCTIONS *******/
static inline StackFrame*
getLastStackFrame() {
    return (StackFrame*)get_thread_ptr()->lastFrame;
}

static inline StackFrame*
getLastStackFrame(VM_thread *thread) {
    return (StackFrame*)(thread->lastFrame);
}

enum interpreter_state {
    INTERP_STATE_STACK_OVERFLOW = 1
};

static inline void setLastStackFrame(StackFrame *frame) {
    get_thread_ptr()->lastFrame = frame;
}

void
Stack::init(void *ptr, int sz) {
    data = (Value*)ptr;
    refs = (uint8*)(data + sz);
    size = sz;
    index = -1;
    for(int i = 0; i < size; i++) refs[i] = 0;
}

Stack::~Stack() {
    FREE_FRAME(data);
}

Locals::~Locals() {
    FREE_FRAME(data);
}

void
Locals::init(void *ptr, uint32 size) {
    var = (Value*) ptr;
    refs = (uint8*)(var + size);
    varNum = size;
    for(uint32 i = 0; i < varNum; i++) refs[i] = 0;
}

int
Locals::getStorageSize(int size) {
    return (size * (sizeof(Value) + sizeof(uint8)) + 7) & ~7;
}

Value&
Locals::operator () (uint32 id) {
    assert(id < varNum);
    return var[id];
}

void
Locals::setLong(int idx, Value2 val) {
#ifdef PTR32
    operator() (idx+l0) = val.v[a0];
    operator() (idx+l1) = val.v[a1];
#else
    operator() (idx+l0) = val.v0;
#endif
}

Value2
Locals::getLong(int idx) {
    Value2 val;
#ifdef PTR32
    val.v[a0] = operator() (idx+l0);
    val.v[a1] = operator() (idx+l1);
#else
    val.v0 = operator() (idx+l0);
#endif
    return val;
}

uint8&
Locals::ref(uint32 id) {
    assert(id < varNum);
    return refs[id];
}

void
Stack::clear() {
    index = -1;
    for(int i = 0; i < size; i++) refs[i] = 0;
}

uint8&
Stack::ref(int offset) {
    assert(index - offset >= 0);
    return refs[index - offset];
}

Value&
Stack::pick(int offset) {
    assert(index - offset >= 0);
    return data[index - offset];
}

void
Stack::setLong(int idx, Value2 val) {
#ifdef PTR32
    pick(idx + s0) = val.v[a0];
    pick(idx + s1) = val.v[a1];
#else
    pick(idx + s0) = val.v0;
#endif
}

Value2
Stack::getLong(int idx) {
    Value2 val;
#ifdef PTR32
    val.v[a0] = pick(idx + s0);
    val.v[a1] = pick(idx + s1);
#else
    val.v0 = pick(idx + s0);
#endif
    return val;
}

void
Stack::pop(int off) {
    index -= off;
    assert(index >= -1);
}

void
Stack::popClearRef(int off) {
    assert(index - off >= -1);
    for(int i = 0; i < off; i++)
        refs[index - i] = FLAG_NONE;
    index -= off;
}

void
Stack::push(int off) {
    index += off;
    assert(index < size);
}

int
Stack::getStorageSize(int size) {
    return (size * (sizeof(Value) + sizeof(uint8)) + 7) & ~7;
}

// Setup locals and stack on C stack.
#define SETUP_LOCALS_AND_STACK(frame,method)                   \
    int max_stack = method->get_max_stack();                   \
    frame.stack.init(ALLOC_FRAME(                              \
                Stack::getStorageSize(max_stack)), max_stack); \
    int max_locals = method->get_max_locals();                 \
    frame.locals.init(ALLOC_FRAME(                             \
                Locals::getStorageSize(max_locals)), max_locals)

enum interpreter_ti_events {
    INTERPRETER_TI_METHOD_ENTRY_EVENT = 1,
    INTERPRETER_TI_METHOD_EXIT_EVENT  = 2,
    INTERPRETER_TI_SINGLE_STEP_EVENT  = 4,
    INTERPRETER_TI_POP_FRAME_EVENT = 8,
    INTERPRETER_TI_FIELD_ACCESS = 16,
    INTERPRETER_TI_FIELD_MODIFICATION = 32,
    INTERPRETER_TI_OTHER = 64 /* EXCEPTION, EXCEPTION_CATCH */
};

/**
 * Global flags section.
 *
 *  Bitwise or of enabled interpreter_ti_events:
 */
extern int interpreter_ti_notification_mode;

