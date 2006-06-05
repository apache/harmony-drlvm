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
 * @author Alexander V. Astapchuk
 * @version $Revision: 1.3.12.3.4.4 $
 */
#include "jframe.h"
#include "trace.h"

/**
 * @file
 * @brief Implementation of JFrame methods.
 */

namespace Jitrino {
namespace Jet {

void JFrame::init(const JFrame * pparent)
{
    *this = *pparent;
}

void JFrame::init(unsigned stack_max, unsigned var_slots, 
                  unsigned istackregs, unsigned ivarregs, 
                  unsigned fstackregs, unsigned fvarregs)
{
    
    m_stack.resize(stack_max+1);
    m_vars.resize(var_slots);
    //
    m_fstack_regs = fstackregs;
    m_fvar_regs = fvarregs;
    m_fvar_regs_mask = (1 << m_fvar_regs) - 1;
    m_istack_regs = istackregs;
    m_ivar_regs = ivarregs;
    m_ivar_regs_mask = (1 << m_ivar_regs) - 1;
    //
    clear_stack();
    clear_vars();
    m_slotids = 0;
    m_need_update = 0;
}

#ifdef _DEBUG
void JFrame::verify(void)
{
    assert(m_top >= -1);
    assert(size() <= m_stack.size());
    assert(((int)m_need_update) >= 0);
    //unsigned depth = (unsigned)m_top + 1;
    //for( unsigned i=0; i<depth; i++ ) {
    //  jtype jt = get_jtype(m_stack[i]);
    //       if( jt == jvoid ) {
    //      assert( i != depth+1 );
    //      assert( m_stack[i+1] == i64 || m_stack[i+1] == dbl64 );
    //  }
    //}
}
#endif

unsigned JFrame::devirt(jtype jt, unsigned vslot)
{
    assert(vslot < vslots(jt));
    const bool isf = is_f(jt);
    for (unsigned slot = 0; slot < size(); slot++) {
        const JFrame::Slot& s = m_stack[slot];
        if (s.vslot() == (int)vslot && (isf == is_f(s.jt))) {
            return slot;
        }
    }
    assert(false);
    return (unsigned)-1;
};

void JFrame::push(jtype jt)
{
    if( jt < i32 ) jt = i32;
    switch( jt ) {
    //case i8:
    //case i16:
    //case u16:
    case i32:
    case jobj:
    case jretAddr:
        m_stack[++m_top] = Slot(jt, false, m_islots++);
        break;
    case i64:
        m_stack[++m_top] = Slot(jt,true, m_islots++);
        m_stack[m_top].m_id = ++m_slotids;
        assert( is_big(i64) );
        m_stack[++m_top] = Slot(jt, false, m_islots++);
        break;
    case flt32:
        m_stack[++m_top] = Slot(jt, false, m_fslots++);
        break;
    case dbl64:
        m_stack[++m_top] = Slot(jt,true, -1);
        m_stack[m_top].m_id = ++m_slotids;
        m_stack[++m_top] = Slot(jt, false, m_fslots++);
        break;
    default:
        assert(false);
        break;
    }
    Slot& s = m_stack[m_top];
    s.m_id = ++m_slotids;
    const unsigned rstackregs = _rstackregs(jt);
    s.m_regid = s.vslot() % rstackregs;
    if (is_big(jt)) {
        assert(jt == i64);
        Slot& shi = m_stack[m_top-1];
        shi.m_regid = shi.vslot() % rstackregs;
    }
    //
    // check whether an overflow happened and, if so, mark some slots to be 
    // unloaded
    //
    const unsigned slots = vslots(jt);
    if (slots <= rstackregs) {
        verify();
        return;
    }
    unsigned to_unload = is_big(jt) ? (slots == rstackregs+1 ? 1 : 2) : 1;
    unsigned unload_vslot = slots >= rstackregs ? slots - rstackregs - 1 : 0;
    unsigned slot = devirt(jt, unload_vslot);
    unsigned depth = slot2depth(slot);
    Slot& sunl = dip(depth);
    if (!(sunl.state() & SS_SWAPPED)) {
        sunl.m_state |= SS_NEED_SWAP;
        ++m_need_update;
    }
    if (to_unload > 1) {
        assert(unload_vslot > 0);
        --unload_vslot;
        slot = devirt(jt, unload_vslot);
        depth = slot2depth(slot);
        Slot& sunl2 = dip(depth);
        if (!(sunl2.state() & SS_SWAPPED)) {
            sunl2.m_state |= SS_NEED_SWAP;
            ++m_need_update;
        }
    }
    verify();
}

void JFrame::pop(jtype jt)
{
    assert(m_top>=0); // cant pop on empty stack
    if (jt < i32) {
        jt = i32;
    }
    assert(top() == jt);
    
    switch(jt) {
        //      case i8:
        //      case i16:
        //      case u16:       jt = i32;
    case jretAddr:
    case i32:
    case jobj:      --m_top;        --m_islots;     break;
    case flt32:     --m_top;        --m_fslots;     break;
    case i64:       m_top -= 2;     m_islots -= 2;  break;
    case dbl64:     m_top -= 2;     --m_fslots;     break;
    default:        assert(false);  break;
    }
    verify();
}

void JFrame::pop2(void)
{
    if (top() == i64 || top() == dbl64) {
        pop( top() );
    }
    else {
        pop(top());
        assert(top() != i64 && top() != dbl64);
        pop(top());
    }
    verify();
}

void JFrame::pop_n(const ::std::vector<jtype>& args)
{
    for (unsigned i=0; i<args.size(); i++) {
        pop(args[(args.size()-i-1)]);
    }
}

void JFrame::dup(JavaByteCodes op)
{
    jtype jt1, jt2;

    jt1 = top();
    if( op != OPCODE_DUP ) {
        pop( jt1 );
    }

    switch( op ) {
    case OPCODE_DUP:
        push( jt1 );
        __get(0).assign( __get(1) );
        break;
    case OPCODE_DUP_X1:
        // [..., value2, value1] => [.. value1, value2, value1]
        // jt1 = top(); pop(jany32);
        jt2 = top();    pop(jt2);
        push( jt1 );
        push( jt2 );
        push( jt1 );
        break;
    case OPCODE_DUP_X2:
        // jt1 = top(); pop(jany32);
        jt2 = top();    pop(jt2);
        if( jt2 == dbl64 || jt2 == i64 ) {
            // [.. value2, value1] => [.. value1, value2, value1]
            push( jt1 );
            push( jt2 );
            push( jt1 );
        }
        else {
            // [.. value3, value2, value1] => [.., value1, value3, value2, value1]
            jtype jt3 = top();      pop(jt3);
            push( jt1 );
            push( jt3 );
            push( jt2 );
            push( jt1 );
        }
        break;
    case OPCODE_DUP2:
        if( jt1 == dbl64 || jt1 == i64 ) {
            // [.. value] => [.. value, value]
            push( jt1 ); push( jt1 );
        }
        else {
            // [.. value2, value1] => [.. value2, value1, value2, value1]
            jt2 = top();    /*pop();
            push( jt2 );*/ push( jt1 );  push( jt2 ); push( jt1 );
        }
        break;
    case OPCODE_DUP2_X1:
        if( jt1 == dbl64 || jt1 == i64 ) {
            // [.. value2, value1.64] =>  [.. value1.64, value2, value1.64]
            jt2 = top();    pop(jt2);

            push(jt1); push(jt2); push(jt1);
        }
        else {
            // [.. value3, value2, value1] =>  [.. value2, value1, value3, value2, value1]
            jt2 = top();        pop(jt2);
            jtype jt3 = top();  pop(jt3);
            push( jt2 );  push( jt1 );  push( jt3 );  push( jt2 );  push( jt1 );
        }
        break;
    case OPCODE_DUP2_X2:
        if( jt1 == dbl64 || jt1 == i64 ) {
            // either Form 2 or Form 4
            jt2 = top();
            if( top() == dbl64 || top() == i64 ) {
                // Form 4
                // [.. value2, value1] =>  [.. value1, value2, value1]
                pop2();
                push( jt1 ); push( jt2 ); push( jt1 );
            }
            else {
                // Form 2
                // [ .. value3, value2, value1] => [.. value1, value3, value2, value1]
                pop(jt2);
                jtype jt3 = top();  pop(jt3);
                push( jt1 ); push( jt3 ); push( jt2 ); push( jt1 );
            }
        }
        else {
            jt2 = top();    pop(jt2);
            jtype jt3 = top();  pop(jt3);
            if( jt3 == dbl64 || jt3 == i64 ) {
                // Form 3
                // [..  value3, value2, value1] => [.. value2, value1, value3, value2, value1]
                push( jt2 ); push( jt1 ); push( jt3 ); push( jt2 ); push( jt1 );
            }
            else {
                // Form 1
                // [.. value4, value3, value2, value1] => [.. value2, value1, value4, value3, value2, value1]
                jtype jt4 = top();  pop(jt4);
                push(jt2); push(jt1); push(jt4); push(jt3); push(jt2); push(jt1);
            }
        };
        break;
    case OPCODE_SWAP:
        // jtype jt1 = top();       pop( jany32 );
        jt2 = top();        pop( jt2 );
        push( jt1 );
        push( jt2 );
        break;
    default: assert( false );
    }
}

unsigned JFrame::gc_mask(unsigned word_no) const
{
    assert( word_no < gc_width() );

    unsigned pos = word_no*WORD_SIZE; // where to start
    unsigned end_pos = ::std::min(pos + WORD_SIZE, size());
    unsigned mask = 0;

    for( ; pos < end_pos; pos++ ) {
        if( m_stack[pos].jt == jobj ) {
            mask |= (1<<bit_no(pos));
        }
    }
    return mask;
}

void JFrame::erase(unsigned idx)
{
    Slot& v = m_vars[idx];
    if (v.m_regid == -1) {
        return;
    }
    const unsigned rbit = 1<<v.m_regid;
    assert(_rmask(v.jt) & rbit);
    assert(_rmap(v.jt)[v.m_regid] == (int)idx);
    _rmap(v.jt)[v.m_regid] = -1;
    _rmask(v.jt) &= ~rbit;
    v = Slot();
    jtype savejt = v.jt;
    unsigned savestate = v.state();
    if (is_wide(savejt)) {
        erase(savestate & SS_HI_PART ? idx-1 : idx+1);
    }
}

void JFrame::release(jtype jt, unsigned rid)
{
    const unsigned rbit = 1<<rid;
    if (!(_rmask(jt) & rbit)) {
        assert(_rmap(jt)[rid] == -1);
        return;
    }
    assert(_rmap(jt)[rid] != -1);
    unsigned idx = _rmap(jt)[rid];
    Slot& v = m_vars[idx];
    assert(v.regid() == (int)rid);
    _rmap(jt)[rid] = -1;
    _rmask(jt) &= ~rbit;
    if (v.needswap()) {
        // already waiting for unloading
        return;
    }
    if (!v.changed()) {
        // not changed - no need to unload
        return;
    }
    v.m_state |= SS_NEED_SWAP;
    ++m_need_update;
}

unsigned JFrame::alloc(jtype jt, int idx, bool upper, bool willDef)
{
    // if its not a scratch register requested, then check whether the slot
    // has a register allocated.
    if (idx != -1) {
        Slot& v = m_vars[idx];
        if (v.jt != jt || upper != (0!=(v.m_state&SS_HI_PART))) {
            // the slot either has another type, or represent
            // another half of a wide type - release the slot 
            erase(idx);
        }
        else if (v.m_regid != -1) {
            // type is the same, and the slot already has 
            // a register allocated - return it
            if (v.swapped() && !willDef) {
                v.m_state |= SS_NEED_UPLOAD;
                ++m_need_update;
            }
            return v.m_regid;
        }
    }
    // try to allocate register
    unsigned& rmask = _rmask(jt);
    // is there any register available ?
    unsigned avail = _rallmask(jt) & ~rmask;
    unsigned rid = 0;
    if (avail) {
        // yes, we have at least one register
        for (rid = 0; !(avail & 0x1); avail >>= 1, rid++);
        // mark the register as 'occupied'
        rmask |= (1 << rid);
    }
    else {
        // no free registers left, release one
        // which to swap ? try to swap out not the recently used register to 
        // avoid too many register<->memory transfers.
        unsigned tmpregid = (rlastused(jt)+1) % _rlocalregs(jt);
        assert(rmask & (1<<tmpregid));
        unsigned swapid = _rmap(jt)[tmpregid];
        assert((int)swapid != idx);
        JFrame::Slot & sswap = m_vars[swapid];
        assert(is_f(sswap.jt) == is_f(jt));
        assert(sswap.m_regid == (int)tmpregid);
        if (sswap.changed()) {
            // need to unload
            sswap.m_state |= SS_NEED_SWAP;
            // temporary keep its regid
            ++m_need_update;
        }
        if (idx != -1) {
            if (!willDef) {
                m_vars[idx].m_state |= SS_NEED_UPLOAD;
                ++m_need_update;
            }
            else {
                m_vars[idx].m_state |= SS_CHANGED;
            }
        }
        rid = tmpregid;
    }

    if (idx==-1) {
        rmask &= ~(1<<rid);
    }
    else {
        Slot& v = m_vars[idx];
        v.jt = jt;
        v.m_regid = rid;
        if (upper) {
            v.m_state |= SS_HI_PART;
        }
        _rmap(jt)[rid] = idx;
        rlastused(jt) = rid;
    }
    return rid;
}


void JFrame::st(jtype jt, unsigned idx)
{
    //release slot at idx
    // if is_wide, then also release idx+1
    //set info about a new slot
    
    // note: 's' is a copy, not a ref, as the item itself get 
    // modified in free()
    Slot s = m_stack[m_top]; 
    Slot& var = m_vars[idx];
    
    if (s.jt != var.jt ) { //m_id && var.m_id != 0) {
        erase(idx);
    }
    var.assign(s);
    if (var.m_id == 0) {
        var.m_id = ++m_slotids;
    }
    if (is_wide(s.type())) {
        Slot& v1 = m_vars[idx+1];
        Slot& s1 = m_stack[m_top-1];
        v1.assign(s1);
        v1.m_id = ++m_slotids;
    }
    pop( s.jt );
}

void JFrame::ld(jtype jt, unsigned idx)
{
    Slot& v = m_vars[idx];
    assert(v.jt == jt || v.jt == jvoid);
    if (v.m_id == 0) {
        v.m_id = ++m_slotids;
        v.jt = jt;
    }
    push(jt);
    m_stack[m_top].assign(v);
    if (is_wide(jt)) {
        Slot& v1 = m_vars[idx+1];
        assert(v1.jt == jt || v1.jt == jvoid);
        v1.m_state |= SS_HI_PART;
        if (v1.m_id == 0) {
            assert(v1.jt == jvoid && v1.m_regid == -1);
            v1.m_id = ++m_slotids;
            v1.jt = jt;
        }
        m_stack[m_top-1].assign(v1);
    }
}

void JFrame::var_def(jtype jt, unsigned idx, unsigned attrs)
{
    assert(idx<m_vars.size());
    // wide types are not expected here.
    assert(!is_wide(jt));
    Slot& v = m_vars[idx];
    assert(v.type() == jvoid || v.type() == jt);
    v.jt = jt;
    v.m_attrs = attrs;
    v.m_id = ++m_slotids;
}

void JFrame::stack_attrs(unsigned depth, unsigned attrs)
{
    Slot& s = m_stack[m_top-depth];
    s.m_attrs |= attrs;
    
    if (s.id() == 0) {
        return;
    }
    
    // propagate properties to local variables
    for(unsigned i=0; i<m_vars.size(); i++) {
        Slot& v = m_vars[i];
        if (v.id() == s.id()) {
            v.m_attrs |= attrs;
        }
    }
    // propagate properties to other stack slots
    for(unsigned i=0; i<size(); i++) {
        Slot& stmp = m_stack[i];
        if (stmp.id() == s.id()) {
            stmp.m_attrs |= attrs;
        }
    }
}

void JFrame::var_state(unsigned idx, unsigned set_state, 
                       unsigned clr_state)
{
    assert(idx<m_vars.size());
    Slot& v = m_vars[idx];
    const unsigned saveState = v.m_state;
    v.m_state |= set_state;
    v.m_state &= ~clr_state;
    if (!(saveState & SS_NEED_SWAP) && (v.m_state & SS_NEED_SWAP)) {
        ++m_need_update;
    }
    if (!(saveState & SS_NEED_UPLOAD) && (v.m_state & SS_NEED_UPLOAD)) {
        ++m_need_update;
    }
    if ((saveState & SS_NEED_SWAP) && !(v.m_state & SS_NEED_SWAP)) {
        --m_need_update;
    }
    if ((saveState & SS_NEED_UPLOAD) && !(v.m_state & SS_NEED_UPLOAD)) {
        --m_need_update;
    }
    if (!(saveState & SS_SWAPPED) && (v.m_state & SS_SWAPPED) && 
        v.regid() != -1) {
        // the item was just swapped out. release its register
        const unsigned rbit = 1<<v.regid();
        if (_rmap(v.jt)[v.regid()] == (int)idx) {
            _rmap(v.jt)[v.regid()] = -1;
            _rmask(v.jt) &= ~rbit;
        }    
        v.m_regid = -1;
        // swapped item also implies 'not changed'
        v.m_state &= ~SS_CHANGED;
    }
}

void JFrame::stack_state(unsigned depth, unsigned set_state,
                         unsigned clr_state) {
    Slot& s = m_stack[depth2slot(depth)];
    const unsigned saveState = s.m_state;
    s.m_state |= set_state;
    s.m_state &= ~clr_state;
    if (!(saveState & SS_NEED_SWAP) && (s.m_state & SS_NEED_SWAP)) {
        ++m_need_update;
    }
    if (!(saveState & SS_NEED_UPLOAD) && (s.m_state & SS_NEED_UPLOAD)) {
        ++m_need_update;
    }
    if ((saveState & SS_NEED_SWAP) && !(s.m_state & SS_NEED_SWAP)) {
        --m_need_update;
    }
    if ((saveState & SS_NEED_UPLOAD) && !(s.m_state & SS_NEED_UPLOAD)) {
        --m_need_update;
    }
}

void JFrame::clear_attrs(void)
{
    assert(need_update()==0);
    clear_vars();
    for (unsigned i=0; i<size(); i++) {
        Slot& s = m_stack[i];
        s.m_attrs = 0;
        s.m_id = ++this->m_slotids;
    }
}

void JFrame::clear_stack(void)
{
    m_top = -1;
    m_fslots = 0;
    m_islots = 0;
}

void JFrame::clear_vars(void)
{
    m_fmask = 0;
    m_imask = 0;
    m_flast_used = 0;
    m_ilast_used = 0;

    for (unsigned i=0; i<m_vars.size(); i++) {
        m_vars[i] = Slot();
    }
    for (unsigned i=0; i<MAX_REGS; i++) {
        m_fvarmap[i] = -1;
        m_ivarmap[i] = -1;
    }
}

bool JFrame::regable(unsigned depth) const
{
    const Slot& s = m_stack[depth2slot(depth)];
    return vslots(s.jt) - s.vslot() <= _rstackregs(s.jt);
}


};};    // ~namespace Jitrino::Jet
