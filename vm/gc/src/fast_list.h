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
 * @author Ivan Volosyuk
 */

#ifndef __NON_MOVABLE_VECTOR_H__
#define __NON_MOVABLE_VECTOR_H__

#include <vector>
#include <assert.h>
#include <port_malloc.h>

template <typename Object, int size>
class fast_list {
    struct row {
        struct row *prev;
        Object data[size];
        struct row *next;
    };

    row *first;
    row *last;
    int pos;
public:

    fast_list() {
        first = last = (row*) STD_MALLOC(sizeof(row));
        first->next = 0;
        first->prev = 0;
        pos = 0;
    }

    ~fast_list() {
        for(row *r = first; r != 0; ) {
            row *next = r->next;
            STD_FREE(r);
            r = next;

        }
    }

    void clear() {
        last = first;
        pos = 0;
    }

    bool empty() {
        return first == last && pos == 0;
    }

    int count() {
        int res = pos;
        for(row *r = first; r != last; r = r->next) res += size;
        return res;
    }

    Object &push_back(Object obj) {
        // caching pos and last in local variables helpful for compiler
        // optimizations
        int p = pos;
        row *l = last;

        Object &res = l->data[p];
        res = obj;
        ++p;
        if (p < size) {
            pos = p;
            return res;
        }

        if (!l->next) {
            row *new_row = (row*) STD_MALLOC(sizeof(row));
            l->next = new_row;
            new_row->prev = l;
            new_row->next = 0;
        }
        last = l->next;
        pos = 0;
        return res;
    }

    Object pop_back() {
        // caching pos and last in local variables helpful for compiler
        // optimizations
        int p = pos - 1;
        row *l = last;

        if (p >= 0) {
            pos = p;
            return l->data[p];
        }

        last = l = l->prev;
        pos = size - 1;
        return l->data[size - 1];
    }

    Object& back() {
        int p = pos;
        row *l = last;
        --p;
        if (p >= 0) return l->data[p];
        return l->prev->data[size-1];
    }

    class iterator {
        private:
            row *r;
            int pos;
        public:
        iterator(row *_r, int _pos) {
            r = _r;
            pos = _pos;
        }

        Object &operator *() { return r->data[pos]; }

        iterator &operator ++() {
            ++pos;
            if (pos < size) return *this;
            pos = 0;
            r = r->next;
            return *this;
        }

        iterator &operator --() {
            --pos;
            if (pos >= 0) return *this;
            r = r->prev;
            pos = size - 1;
            return *this;
        }

        bool operator == (iterator i) {
            return i.r == r && i.pos == pos;
        }

        bool operator != (iterator i) {
            return ! operator == (i);
        }

        typedef std::input_iterator_tag iterator_category;
        typedef Object value_type;
        typedef Object* pointer;
        typedef Object& reference;
        typedef ptrdiff_t difference_type;
    };

    iterator begin() { return iterator(first, 0); }
    iterator end() { return iterator(last, pos); }
};

#endif /* __NON_MOVABLE_VECTOR_H__ */
