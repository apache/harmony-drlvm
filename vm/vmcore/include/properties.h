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
 * @author Intel, Alexey V. Varlamov, Gregory Shimansky
 * @version $Revision: 1.1.2.1.4.6 $
 */  



#ifndef _VM_PROPERTIES_H
#define _VM_PROPERTIES_H

#include "platform.h"
#include "environment.h"
#include <string.h>

#ifdef USE_MEM_MANAGER_FOR_ALLOCATION 
#include "tl/memory_pool.h"
#endif

/***************************************************************************************/
class Prop_Value
{
public:
    virtual ~Prop_Value(){}

    virtual Prop_Value* clone() = 0;        /*Clone this Prop_Value*/

    virtual void print() = 0;

    virtual const char* as_string() { return NULL; }

#ifdef USE_MEM_MANAGER_FOR_ALLOCATION 
public:
    void *operator new(size_t sz, tl::MemoryPool& m) {
        return m.alloc(sz);
    }
    void operator delete(void *obj, tl::MemoryPool& m){
        //do nothing
    };
    void *operator new(size_t sz) {
        return m_malloc(sz);
    }
    void operator delete(void *obj){
        m_free(obj);
    };
    virtual Prop_Value* clone(tl::MemoryPool& m) = 0;
#endif
};

class Prop_String: public Prop_Value
{
public:
    Prop_String()
    {
        value = NULL;
    }

    Prop_String(char* v)                    /*Do not clone v*/
    {
        value = v;
    }
    ~Prop_String()
    {
        if(value)
            STD_FREE(value);
    }
    Prop_Value* clone()
    {
        return (new Prop_String(strdup((char*)value))); /*use strdup() to clone the value*/
    }
#ifdef USE_MEM_MANAGER_FOR_ALLOCATION 
    Prop_Value* clone(tl::MemoryPool& m)
    {
        return (new (m) Prop_String(strdup((char*)value))); /*use strdup() to clone the value*/
    }
#endif
    void print()
    {
        printf("%s", value);
    } 
    const char* as_string()
    {
        return value;
    }
public:
    char *value;
};

/***************************************************************************************/

class Prop_uLong: public Prop_Value
{
public:
    Prop_uLong()
    {
        value = 0;
    }
    Prop_uLong(unsigned long v)
    {
        value = v;
    }
    ~Prop_uLong()
    {
    }
    Prop_Value* clone()
    {
        return (new Prop_uLong(((unsigned long)value)));
    }
#ifdef USE_MEM_MANAGER_FOR_ALLOCATION 
    Prop_Value* clone(tl::MemoryPool& m)
    {
        return (new (m) Prop_uLong(((unsigned long)value)));
    }
#endif
    void print()
    {
        printf("%lu", ((unsigned long)value));
    }
public:
    unsigned long value;
};


/***************************************************************************************/
struct Prop_entry
{
    char *key;
    Prop_Value *value;                  /*value can be Prop_Sting,Prop_Properties,etc.*/
    struct Prop_entry *next;
    int compareTo(const char *akey)
    {
        return strcmp(akey, key);
    }
    void replace(Prop_entry *e)
    {
        if(value)delete value;
        value = e->value->clone();  /*clone e's value*/
    }
public:
    Prop_entry()
    {
        key = NULL;
        value = NULL;
        next = NULL;
    }
    ~Prop_entry()
    {
        if(key) STD_FREE(key); 
        if(value)delete value;
        if(next)delete next;
    }
#ifdef USE_MEM_MANAGER_FOR_ALLOCATION 
public:
    void *operator new(size_t sz, tl::MemoryPool& m) {
        return m.alloc(sz);
    }
    void operator delete(void *obj, tl::MemoryPool& m){
        //do nothing
    };
    void *operator new(size_t sz) {
        return m_malloc(sz);
    }
    void operator delete(void *obj){
        m_free(obj);
    };
#endif
};


/**************** The following is Properties *********************/

class Properties
{

public:
    class Iterator
    {
    private:
        Properties *prop;
        Prop_entry *current;
        int idx;
        Iterator(Properties *prop_)         /*Couldn't new instances except Properties*/
        {
            prop = prop_;
        }
    public:
        Prop_entry *currentEntry() const {return current;}
        void reset()
        {
            current = NULL;
            idx = 0;
        }
        const Prop_entry* next()
        {
            if(current && current->next)
                return current = current->next;
            int i = current? idx+1 : idx;
            for(; i < prop->BUCKET_SIZE; i++)
                if(prop->bucket[i])
                    break;
            if(i == prop->BUCKET_SIZE) {
                current = NULL;
            } else {
                current = prop->bucket[i];
            }
            idx = i;
            return current;
        }
        friend class Properties;
    }; //end of class Iterator

    friend class Properties::Iterator;
public:
    Properties()
    {
        BUCKET_SIZE = 11; //wgs: is it reasonable?
        init(BUCKET_SIZE);
    }
    Properties(long size_)
    {
        BUCKET_SIZE = size_;
        init(BUCKET_SIZE);
    }
    ~Properties(){
        delete iterator;
        clear();
        STD_FREE(bucket);
    }
    Iterator *getIterator(){
        iterator->reset();
        return iterator;
    }
    Iterator* getNewIterator()
    {
        Iterator* iter = new Iterator(this);
        iter->reset();
        return iter;
    }
    void destroyIterator(Iterator* it)
    {
        assert(it!=iterator);
        assert(it->prop == this);
        delete it;
    }
    void add(const char* k, Prop_Value* v);     /*Caller delete k and v*/
#ifdef USE_MEM_MANAGER_FOR_ALLOCATION 
    void add(const char* k, Prop_Value* v, tl::MemoryPool& m);
#endif
    void add(Prop_entry* e);                /*Caller can not delete e*/
    void add(const char *line);
    inline void clear();
    Properties* clone();
    Prop_Value* get(const char* key);
    Prop_entry* get_entry(const char* key);
    void print();
    bool contains_key(const char* key);

private:
    Prop_entry** bucket;
    int index_of(const char*key);
    Iterator *iterator;
    long BUCKET_SIZE;
    void init(long size_){
        bucket = (Prop_entry**)STD_CALLOC(sizeof(Prop_entry*), size_);
        iterator = new Iterator(this);
    }
#ifdef USE_MEM_MANAGER_FOR_ALLOCATION 
public:
    void *operator new(size_t sz, tl::MemoryPool& m) {
        return m.alloc(sz);
    }
    void operator delete(void *obj, tl::MemoryPool& m){
        //do nothing
    };
    void *operator new(size_t sz) {
        return m_malloc(sz);
    }
    void operator delete(void *obj){
        m_free(obj);
    };
#endif
};

typedef struct _properties* PropertiesHandle;

VMEXPORT const char* properties_get_string_property(PropertiesHandle prop, const char* key);
VMEXPORT void properties_set_string_property(PropertiesHandle ph, const char* key, const char* value);
VMEXPORT PropertiesIteratorHandle properties_iterator_create(PropertiesHandle ph);
VMEXPORT void properties_iterator_destroy(PropertiesHandle ph, PropertiesIteratorHandle pih);
VMEXPORT Boolean properties_iterator_advance(PropertiesIteratorHandle pih);
VMEXPORT const char* properties_get_name(PropertiesIteratorHandle pih);
VMEXPORT const char* properties_get_string_value(PropertiesIteratorHandle pih);

inline int Properties::index_of(const char *key)
{
    unsigned int hash = 0;
    const char *end = key - 1;
    while(*(++end));
    for (; key < end; key += 3)
        hash = hash * 31 + *key;
    return hash % BUCKET_SIZE;
}

inline void Properties::add(const char* k,Prop_Value* v)
{
    Prop_entry* e = new Prop_entry();
#ifdef CLONE_ENTRY_WHEN_ADD
    e->key = strdup(k);                     /*caller can delete k*/
    e->value = v->clone();                  /*caller can delete v*/
#else
    e->key = (char*)k;
    e->value = v;
#endif
    add(e);                                 /*e will be invalid*/
}

#ifdef USE_MEM_MANAGER_FOR_ALLOCATION 
inline void Properties::add(const char* k,Prop_Value* v, tl::MemoryPool& m)
{
    Prop_entry* e = new (m) Prop_entry();
    e->key = strdup(k);                     /*caller can delete k*/
    e->value = v->clone(m);                 /*caller can delete v*/
    add(e);                                 /*e will be invalid*/
}
#endif

inline char* unquote(char *str)
{
    const char *tokens = " \t\n\r\'\"";
    size_t i = strspn(str, tokens);
    str += i;
    char *p = str + strlen(str) - 1;
    while(strchr(tokens, *p) && p >= str)
        *(p--) = '\0';
    return str;
}

inline void Properties::add(const char *line)
{
    // Ignore the line if it starts with #
    if (line[0]=='#') return;
    char *src = strdup(line);
    char *tok = strchr(src, '=');
    if(tok)
    {
        *tok = '\0';
        Prop_entry *e = new Prop_entry();
        e->key = strdup(unquote(src));
        e->value = new Prop_String(strdup(unquote(tok + 1)));
        if((e->key[0] == '\0') ) // esostrov: properties may have emply string values || (((Prop_String*)e->value)->value[0] == '\0'))
            delete e;
        else
            add(e);
    }
    STD_FREE(src);
}

inline void Properties::print()
{
    int i = 0;
    for(i = 0; i < BUCKET_SIZE; i++)
        if(bucket[i]){
            Prop_entry *e = bucket[i];
            while(e)
            {
                printf("%s=", e->key);
                e->value->print();
                printf("\n");
                e = e->next;
            }
        }
}

inline void Properties::clear()
{
    for(int i = 0; i < BUCKET_SIZE; i++)
        if(bucket[i])
        {
            delete bucket[i];
            bucket[i]=NULL;
        }
}

inline Properties* Properties::clone()
{
    Properties* cloned_p = new Properties(BUCKET_SIZE);
    for(int i = 0; i < BUCKET_SIZE; i++)
        if(bucket[i])
        {
            Prop_entry *itr = bucket[i];
            while(itr)
            {
                Prop_entry *newe = new Prop_entry();
                newe->key = strdup(itr->key);
                newe->value = itr->value->clone();
                cloned_p->add(newe);                /*Can NOT delete newe*/
                itr = itr->next;
            }
        }
    return cloned_p;

}

inline Prop_Value* Properties::get(const char* key)
{
    int idx = index_of(key);
    if(bucket[idx] == NULL)
        return NULL;

    Prop_entry *itr = bucket[idx];
    while(itr)
    {
        int cmp = itr->compareTo(key);
        if(cmp > 0)
            return NULL;
        if(cmp == 0)
            return itr->value;
        itr = itr->next;
    }
    return NULL;
}

inline Prop_entry* Properties::get_entry(const char* key)
{
    int idx = index_of(key);
    if(bucket[idx] == NULL)
        return NULL;

    Prop_entry *itr = bucket[idx];
    while(itr)
    {
        int cmp = itr->compareTo(key);
        if(cmp > 0)
            return NULL;
        if(cmp == 0)
            return itr;
        itr = itr->next;
    }
    return NULL;
}

inline bool Properties::contains_key(const char* key)
{
    int idx = index_of(key);
    if(bucket[idx] == NULL)
        return false;

    Prop_entry *itr = bucket[idx];
    while(itr)
    {
        int cmp = itr->compareTo(key);
        if(cmp > 0)
            return false;
        if(cmp == 0)
            return true;
        itr = itr->next;
    }
    return false;
}


/***************************************************************************************/

class Prop_Properties: public Prop_Value
{
public:
    Prop_Properties()
    {
        value = NULL;
    }
    Prop_Properties(Properties* v)
    {
        value = v;
    }
    ~Prop_Properties()
    {
        if(value)
            delete value;
    }
    Prop_Value* clone()
    {
        return (new Prop_Properties(value->clone()));
    }
#ifdef USE_MEM_MANAGER_FOR_ALLOCATION 
    Prop_Value* clone(tl::MemoryPool& m)
    {
        return (new (m) Prop_Properties(value->clone()));
    }
#endif

    void print()
    {
        value->print();
    }
public:
    Properties* value;
};

void initialize_properties(Global_Env *p_env, Properties &prop);
void add_to_properties(Properties& prop, const char* line);
VMEXPORT void add_pair_to_properties(Properties& prop, const char* key, const char* value);
void post_initialize_ee_dlls(PropertiesHandle ph);



#endif // _VM_PROPERTIES_H
