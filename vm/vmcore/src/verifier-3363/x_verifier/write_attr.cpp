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
 * @author Mikhail Loenko
 */  



#include <iostream>

using namespace std;

#include "recompute.h"
#include "../java6/stackmap_6.h"
#include "time.h"

using namespace CPVerifier;
using namespace CPVerifier_5;
using namespace CPVerifier_6;

static char err_message[5000];


void vf_Context_5e::writeStackMapFrame( Address instr )
{
    assert(instr != -1 || 1 + lastInstr < instr );
    uint32 offset = lastInstr == -1 ? instr : instr - lastInstr - 1;
    lastInstr = instr;

    if( curFrame->depth == 0 ) {
        //possible variants when not the whole frame is recorded: 
        //locals are the same, 0-2 locals are cut, locals are extended by 0-2 elements
        bool all_locals_same = true;
        unsigned first_changed_local = 0;
        unsigned last_changed_local = 0;

        for( int i = (int)m_max_locals - 1; i >= 0; i-- ) {
            if( curFrame->elements[i].getConst() != workmap->elements[i].getConst() ) {
                all_locals_same = false;
                first_changed_local = i;
                if( !last_changed_local ) last_changed_local = i;
            }
        }

        if( all_locals_same ) {
            return writeStackMapFrame_Same(offset);
        } 

        if( first_changed_local < lastLocalsNo ) {
            //check whether it's a CUT
            if( last_changed_local >= lastLocalsNo ) {
                return writeStackMapFrame_Full(offset);
            }

            int cut_sz = 0; // number of elements cut in attribute (long and double are single size units)
            int cut_realsz = 0; // number of elements cut in workmap structure (long and double are double size units)
            for( unsigned i = first_changed_local; i < lastLocalsNo; i++ ) {
                if( curFrame->elements[i].getConst() != SM_BOGUS ) {
                    return writeStackMapFrame_Full(offset);
                }

                cut_realsz++;
                if( workmap->elements[i].getConst() != SM_HIGH_WORD ) {
                    //can't cut more than 3 elements
                    cut_sz++;
                }
            }
            if( cut_sz > 3 ) {
                return writeStackMapFrame_Full(offset);
            }
            return writeStackMapFrame_Cut(offset, cut_sz, cut_realsz);
        } else {
            //check whether it's an APPEND
            int app_sz = 0;  // number of elements appended in attribute (long and double are single size units)
            int app_realsz = 0; // number of elements appended in workmap structure (long and double are double size units)
            for( unsigned i = lastLocalsNo; i < last_changed_local + 1; i++ ) {
                assert( workmap->elements[i].getConst() == SM_BOGUS );

                app_realsz++;
                if( curFrame->elements[i].getConst() != SM_HIGH_WORD ) {
                    app_sz++;
                }
            }
            if( app_sz > 3 ) {
                //can't append more than 3 elements
                return writeStackMapFrame_Full(offset);
            }
            return writeStackMapFrame_Append(offset, app_sz, app_realsz);
        }
    } else if( curFrame->depth == 1 || curFrame->depth == 2 && curFrame->elements[m_stack_start + 1].getConst() == SM_HIGH_WORD) {
        for( unsigned i = 0; i < m_max_locals; i++ ) {
            if( curFrame->elements[i].getConst() != workmap->elements[i].getConst() ) {
                return writeStackMapFrame_Full(offset);
            }
        }
        return writeStackMapFrame_SameLocalsOneStack(offset);
    } else {
        return writeStackMapFrame_Full(offset);
    }

} // writeStackMapFrame

void vf_Context_5e::writeStackMapFrame_Full( uint32 offset ) {
    writeByte(255); // full stack frame

    writeByte(offset >> 8); // offset
    writeByte(offset & 0xFF); // offset

    unsigned locals_realsz; // number of elements in workmap structure (long and double are double size units)
    for( locals_realsz = m_max_locals; locals_realsz > 0; locals_realsz-- ) {
        if( curFrame->elements[locals_realsz - 1].getConst() != SM_BOGUS ) {
             break;
        }
    }

    unsigned locals_sz = 0; // number of elements in attribute (long and double are single size units)
    for( unsigned i = 0; i < locals_realsz; i++ ) {
        if( curFrame->elements[i].getConst() != SM_HIGH_WORD ) {
             locals_sz++;
        }
    }

    writeByte(locals_sz >> 8); // locals_sz
    writeByte(locals_sz & 0xFF); // locals_sz

    writeStackMapElements(0, locals_realsz);
    lastLocalsNo = locals_realsz;

    /////////////////////////////////

    unsigned stack_sz = 0; // number of stack elements in attribute (long and double are single size units)
    for( unsigned i = 0; i < curFrame->depth; i++ ) {
        if( curFrame->elements[m_stack_start + i].getConst() != SM_HIGH_WORD ) {
             stack_sz++;
        }
    }

    writeByte(stack_sz >> 8); // stack depth
    writeByte(stack_sz & 0xFF); // stack depth

    writeStackMapElements(m_stack_start, curFrame->depth);

}

void vf_Context_5e::writeStackMapFrame_SameLocalsOneStack( uint32 offset ) {
    
    if( offset < 64 ) {
        writeByte(offset + 64); // same locals one stack item
        writeStackMapElements(m_stack_start, 1);
    } else {
        writeByte(247); // one stack extended

        writeByte(offset >> 8); // offset
        writeByte(offset & 0xFF); // offset
        writeStackMapElements(m_stack_start, 1);
    }
}

void vf_Context_5e::writeStackMapFrame_Same( uint32 offset ) {

    if( offset < 64 ) {
        writeByte(offset); // same
    } else {
        writeByte(251); // same extended
        writeByte(offset >> 8); // offset
        writeByte(offset & 0xFF); // offset
    }
}

void vf_Context_5e::writeStackMapFrame_Cut( uint32 offset, int cut_sz, int cut_realsz ) {

    writeByte(251 - cut_sz); // same extended

    writeByte(offset >> 8); // offset
    writeByte(offset & 0xFF); // offset

    lastLocalsNo -= cut_realsz;
}

void vf_Context_5e::writeStackMapFrame_Append( uint32 offset, int app_sz, int app_realsz ) {

    writeByte(251 + app_sz); // same extended

    writeByte(offset >> 8); // offset
    writeByte(offset & 0xFF); // offset

    writeStackMapElements(lastLocalsNo, app_realsz);

    lastLocalsNo += app_realsz;
}

void vf_Context_5e::writeStackMapElements( uint32 start, uint32 cnt ) {
    while( cnt ) {
        SmConstant el = curFrame->elements[start].const_val;
        workmap->elements[start].const_val = el;

        if( el.isReference() ) {
            writeByte(ITEM_OBJECT);

            uint16 cp_idx = class_get_cp_class_entry( k_class, tpool.sm_get_refname(el));
            writeByte(cp_idx >> 8);
            writeByte(cp_idx & 0xFF);
        } else if( el.isNewObject() ) {
            writeByte(ITEM_UNINITIALIZED);

            uint16 instr = el.getNewInstr();
            writeByte(instr >> 8);
            writeByte(instr & 0xFF);
        } else switch ( el.c ) {
            case SM_HIGH_WORD:
                break;
            case SM_NULL:
                writeByte(ITEM_NULL);
                break;
            case SM_INTEGER:
                writeByte(ITEM_INTEGER);
                break;
            case SM_FLOAT:
                writeByte(ITEM_FLOAT);
                break;
            case SM_LONG:
                writeByte(ITEM_LONG);
                break;
            case SM_DOUBLE:
                writeByte(ITEM_DOUBLE);
                break;
            case SM_THISUNINIT:
                writeByte(ITEM_UNINITIALIZEDTHIS);
                break;
            default:
                writeByte(ITEM_TOP);
                break;
        }
        start++;
        cnt--;
    }
}

