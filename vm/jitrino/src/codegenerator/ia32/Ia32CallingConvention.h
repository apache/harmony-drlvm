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
 * @author Vyacheslav P. Shakin
 * @version $Revision: 1.8.22.3 $
 */

#ifndef _IA32_CALLING_CONVENTION_H_
#define _IA32_CALLING_CONVENTION_H_

#include "open/types.h"
#include "Type.h"
#include "Ia32IRConstants.h"
#include "Ia32Constraint.h"


namespace Jitrino
{
namespace Ia32{


//=========================================================================================================


//========================================================================================
// class CallingConvention
//========================================================================================
/**
Interface CallingConvention describes a particular calling convention.

As calling convention rules can be more or less formally defined, 
it is worth to define this entity as a separate class or interface
Implementers of this interface are used as arguments to some IRManager methods 

*/
class CallingConvention
{   
public: 
    //--------------------------------------------------------------

    struct OpndInfo
    {
        uint32              typeTag;
        uint32              slotCount;
        bool                isReg;
        uint32              slots[4];
    };

    //--------------------------------------------------------------
    enum ArgKind
    {
        ArgKind_InArg,
        ArgKind_RetArg
    };

    virtual ~CallingConvention() {}

    /** Fills the infos array with information how incoming arguments or return values are passed 
    according to this calling convention 
    */
    virtual void    getOpndInfo(ArgKind kind, uint32 argCount, OpndInfo * infos) const =0;

    /** Returns a mask describing registers of regKind which are to be preserved by a callee
    */
    virtual Constraint  getCalleeSavedRegs(OpndKind regKind) const =0;
    
    /** Returns true if restoring arg stack is callee's responsibility
    */
    virtual bool    calleeRestoresStack() const =0;

    /** True arguments are pushed from the last to the first, false in the other case
    */
    virtual bool    pushLastToFirst()const =0;
    /**
     * Maps a string representation of CallingConvention to the 
     * appropriate CallingConvention_* item. 
     * If cc_name is NULL, then default for this platform convention 
     * is returned.
     */
    static const CallingConvention * str2cc(const char * cc_name);
};


typedef StlVector<const CallingConvention *> CallingConventionVector;


//========================================================================================
// class STDCALLCallingConvention
//========================================================================================
/** Implementation of CallingConvention for the STDCALL calling convention
*/

class STDCALLCallingConvention: public CallingConvention
{   
public: 
    
    virtual ~STDCALLCallingConvention() {}
    virtual void    getOpndInfo(ArgKind kind, uint32 argCount, OpndInfo * infos)const;
    virtual Constraint  getCalleeSavedRegs(OpndKind regKind)const;
#ifdef _EM64T_
    virtual bool    calleeRestoresStack()const{ return false; }
#else
    virtual bool    calleeRestoresStack()const{ return true; }
#endif
    virtual bool    pushLastToFirst()const{ return true; }

};

//========================================================================================
// class DRLCallingConvention
//========================================================================================
/** Implementation of CallingConvention for the DRL IA32 calling convention
*/
class DRLCallingConvention: public STDCALLCallingConvention
{   
public: 
    virtual ~DRLCallingConvention() {}
    virtual bool    pushLastToFirst()const{ return false; }

};

//========================================================================================
// class CDECLCallingConvention
//========================================================================================
/** Implementation of CallingConvention for the CDECL calling convention
*/
class CDECLCallingConvention: public STDCALLCallingConvention
{   
public: 
    virtual ~CDECLCallingConvention() {}
    virtual bool    calleeRestoresStack()const{ return false; }
#ifdef _EM64T_
    virtual void    getOpndInfo(ArgKind kind, uint32 argCount, OpndInfo * infos)const;
    virtual bool    pushLastToFirst()const{ return true; }
#endif
};

extern STDCALLCallingConvention     CallingConvention_STDCALL;
extern DRLCallingConvention         CallingConvention_DRL;
extern CDECLCallingConvention       CallingConvention_CDECL;

}; // namespace Ia32
}
#endif
