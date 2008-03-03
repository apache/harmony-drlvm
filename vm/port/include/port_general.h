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
* @author Alexey V. Varlamov
* @version $Revision: 1.1.2.1.4.3 $
*/  

#ifndef _PORT_GENERAL_H_
#define _PORT_GENERAL_H_

#include <apr.h>

#ifndef NULL
#ifdef __cplusplus
#   define NULL (0)
#else
#   define NULL ((void *)0)
#endif /* __cplusplus */
#endif /* NULL */


#ifdef __cplusplus
#define PORT_INLINE inline
#else // !__cplusplus

#ifdef WIN32
#define PORT_INLINE __inline
#else // !WIN32

#ifdef __linux__
#define PORT_INLINE inline  __attribute__((always_inline))
#else // !__linux__
#define PORT_INLINE static
#endif // __linux__

#endif // WIN32

#endif // __cplusplus


#endif /* _PORT_GENERAL_H_ */
