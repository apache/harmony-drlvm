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
 * @author Pavel Pervov
 * @version $Revision: 1.1.2.1.4.4 $
 */  
#ifndef _MANIFEST_H_
#define _MANIFEST_H_

#include "properties.h"

class JarFile;

class Manifest
{
    Properties m_main;
    bool m_parsed;
public:
    Manifest( const JarFile* jf );
    ~Manifest();

    Properties* GetMainProperties() { return &m_main; }
    bool operator! () { return !m_parsed; }

protected:
    bool IsMainProperty( const char* );
};

#endif

