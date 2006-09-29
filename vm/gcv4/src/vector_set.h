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
 * @version $Revision: 1.1.2.1.4.3 $
 */
#ifndef _VECTOR_SET_H
#define _VECTOR_SET_H

#include <assert.h>
#include <vector>
#include "hash_table.h"

//// logging code
#include "cxxlog.h"

/**
 * A hybrid vector of set, designed for a maximum performance in a release build
 * and ensuring correctness (no duplicate elements) in a debug build.
 */
template<typename T>
class vector_set {
    private:
#ifdef _DEBUG
        Hash_Table *dup_removal_enum_hash_table;
        unsigned warning_threshold;
#endif
        typedef std::vector<T> Vector;
        std::vector<T> v;
    public:
        typedef typename Vector::iterator iterator;

#ifdef _DEBUG
        vector_set() {
            // sensible default
            warning_threshold = 100000;
            // create hash table
            dup_removal_enum_hash_table = new Hash_Table();
            assert(dup_removal_enum_hash_table->is_empty());
        }

        ~vector_set() {
            delete dup_removal_enum_hash_table;
        }
#endif

        void reserve(size_t capacity) {
            v.reserve(capacity);
#ifdef _DEBUG
            warning_threshold = capacity*10;
#endif
        }

        void insert(T &element) {
#ifdef _DEBUG
            if (v.size() >= warning_threshold) {
                warning_threshold *= 2;
                // FIXME: warning assumes this container is used only for root set
                WARN("Root set array exceeded " << warning_threshold << " elements");
            }

            assert(dup_removal_enum_hash_table->size() == v.size());
            assert(dup_removal_enum_hash_table->add_entry_if_required(element));
#endif
            v.push_back(element);
        }

        T& operator[](unsigned index) {
            return v[index];
        }

        void clear() {
#ifdef _DEBUG
            // Clear the hash table used to remove duplicates from the enumerations.
            dup_removal_enum_hash_table->empty_all();
#endif
            v.clear();
        }

        typename Vector::iterator begin() {
            return v.begin();
        }

        typename Vector::iterator end() {
            return v.end();
        }

        Vector& vector() {
            return v;
        }

        size_t size() {
            return v.size();
        }
};

#endif // _VECTOR_SET_H
