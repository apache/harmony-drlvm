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
 * @author Sergey L. Ivashin
 * @version $Revision: 1.1.22.3 $
 *
 */

#ifndef _COUNTER_H_
#define _COUNTER_H_

namespace Jitrino 
{

class CountWriter;


class CounterBase
{
public:

	CounterBase (const char* s = 0);
	virtual ~CounterBase ();

	void setName (const char* s)					{key = s;}
	void link ();
	virtual void write  (CountWriter&)				= 0;

	const char* key;

//protected:

	CounterBase* next;
	static CounterBase* head;
};


template <typename T>
class Counter : public CounterBase
{
public:

	Counter (const char* s)							: CounterBase(s) {}
	Counter (const char* s, const T& v)				: CounterBase(s), value(v) {}

	/*virtual*/void write  (CountWriter& logs)		{logs.write(key, value);}

	operator T& ()									{return value;}

	T value;
};


class CountWriter
{
public:

	virtual ~CountWriter ()		{}

	virtual void write (const char* key, const char*  value)    	= 0;
	virtual void write (const char* key, int          value)		= 0;
	virtual void write (const char* key, unsigned int value)		= 0;
	virtual void write (const char* key, double       value)		= 0;
};


} //namespace Jitrino 

#endif   //#ifndef _COUNTER_H_
