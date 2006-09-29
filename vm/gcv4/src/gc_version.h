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
 * @author Salikh Zakirov
 * @version $Revision: 1.1.2.9.4.9 $
 */
#ifndef _GC_VERSION_H
#define _GC_VERSION_H

// These macros are automatically updated upon commit.
// PLEASE DO NOT TOUCH
#define PATCH_BRANCH "DRL-M1-Harmony"
#define PATCH_LEVEL 39
#define PATCH_DATE "2006-03-28"
// end of automatically updated macros

#define _GC_EXPAND(x) #x
#define GC_EXPAND(x) _GC_EXPAND(x)
#define GC_NAME "GC v4"
#define GC_VERSION "M1-" GC_EXPAND(PATCH_LEVEL) " (" PATCH_DATE ")"

#endif // _GC_VERSION_H
