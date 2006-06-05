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
 * @author Vyacheslav P. Shakin
 * @version $Revision: 1.7.20.3 $
 */

#ifndef _IA32_DCE_H_
#define _IA32_DCE_H_

#include "Ia32IRManager.h"
namespace Jitrino
{
namespace Ia32{

//========================================================================================
// class DCE
//========================================================================================
/**
	class DCE performs Dead code elimination
*/
BEGIN_DECLARE_IRTRANSFORMER(DCE, "dce", "Dead code elimination")
    DCE(IRManager& irm, const char * params=0): IRTransformer(irm, params){} 
    void runImpl();
    uint32 getSideEffects() const {return 0;}
    uint32 getNeedInfo()const {return 0;}
END_DECLARE_IRTRANSFORMER(DCE) 


}}; // namespace Ia32

#endif
