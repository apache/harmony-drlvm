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
 * @author Intel, Pavel Afremov
 * @version $Revision: 1.1.2.1.4.3 $
 */  

#ifndef _STACK_ITERATOR_H_
#define _STACK_ITERATOR_H_

#include "jit_export.h"
#include "open/types.h"
#include "vm_core_types.h"

// This module is for a "stack iterator" abstraction.
// A stack iterator allows other code to iterator over the managed and m2n
// frames of a thread in order from most recently pushed to first pushed.
// It also allows for resuming a frame in the current thread.

struct StackIterator;

// Create a new stack iterator for the current thread assuming the thread
// is currently in native code.
StackIterator* si_create_from_native();

// Create a new stack iterator for the given thread assuming it is currently
// in native code. Note that the thread can run concurrently with the stack
// iterator, but it should not pop (return past) the most recent M2nFrame at
// the time the iterator is called. Also note that creation is not atomic with
// respect to pushing/popping of m2n frames; the client should ensure that such
// operations are serialized.
StackIterator* si_create_from_native(VM_thread*);

// Create a new stack iterator for a thread assuming it is currently suspended
// from managed code. See also note in previous function. The registers
// arguments contains the values of the registers at the point of suspension,
// is_ip_past is described in the JIT frame context structure, and the M2nFrame
// should be the one immediately prior to the suspended frame.
StackIterator* si_create_from_registers(Registers*, bool is_ip_past, M2nFrame*);

// The implementation of stack iterators and m2n frames may not track all
// preserved registers needed for resuming a frame, but may instead track
// enough for root-set enumeration and stack walking. The following function
// and a corresponding addition stub generator for m2n frames allow all such
// registers to be tracked for exception propagation. Ensure that all
// preserved registers are transferred from the m2n frame to the iterator.
// This function should only be called when the iterator is at an M2nFrame
// that has all preserved registers saved.
void si_transfer_all_preserved_registers(StackIterator*);

// Return if the iterator is past all the frames.
bool si_is_past_end(StackIterator*);

// Goto the frame previous to the current one.
void si_goto_previous(StackIterator*);

// Make a copy of a stack iterator
StackIterator* si_dup(StackIterator*);

// Free the iterator.
void si_free(StackIterator*);

// Return the ip for the current frame.
NativeCodePtr si_get_ip(StackIterator*);

// Set the ip for the current frame
void si_set_ip(StackIterator*, NativeCodePtr,
               bool also_update_stack_itself = false);

// 20040713 Experimental: set the code chunk in the stack iterator
void si_set_code_chunk_info(StackIterator*, CodeChunkInfo*);

// Return the code chunk information for the current frame.
// Returns NULL for M2nFrames.
CodeChunkInfo* si_get_code_chunk_info(StackIterator*);

// Return the JIT frame context for the current frame. This can be mutated to
// reflect changes in registers desired in transfer control.
JitFrameContext* si_get_jit_context(StackIterator*);

// Return if the current frame is an M2nFrame.
bool si_is_native(StackIterator*);

// Return the M2nFrame if the current frame is an M2nFrame.
// Should only be called if si_is_native is true.
M2nFrame* si_get_m2n(StackIterator*);

// Set the return register appropriate for a pointer. If transfer control is
// subsequently called, the resumed frame will see this change.
// The argument points to the pointer to return.
void si_set_return_pointer(StackIterator*, void** return_value);

// Resume execution in the current frame of the iterator.
// Should only be called for an iterator on the current thread's frames.
// Does not return and frees the stack iterator.
void si_transfer_control(StackIterator*);

// Copy the stack iterators current frame value into the given registers, so
// that resumption of these registers would transfer control to the current frame.
void si_copy_to_registers(StackIterator*, Registers*);

// On architectures with register stacks, ensure that the register stack of
// the current thread is consistent with its backing store, as the backing
// store might have been modified by stack walking code.
// On other architectures do nothing.
void si_reload_registers();

#endif //!_STACK_ITERATOR_H_
