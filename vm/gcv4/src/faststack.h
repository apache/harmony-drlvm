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
 * @author Intel, Aleksey Ignatenko
 * @version $Revision: 1.1.2.1.4.3 $
 */  

// Fast LIFO stack. 
// To improve memory footprint 2 dimensional array is used: stack elemnts
// are stored in the sequence of consistent memory blocks. 
// When stack is created only one first block is allocated.
// Memory for other blocks is allocated if it's necessary.
// Once block was allocated, it's not deallocated anywhere 
// and can be used further without additional impact on performance.
// Parameters STACK_BLOCK_SIZE and STACK_NUMBER_BLOCKS should be tuned 
// for specific task. If these parameters are not set properly some low performance 
// decrease possible for stack size enlargement.

#ifndef _fast_stack_H_
#define _fast_stack_H_
#include <vector>

template <typename ObjectPtr>
class FastStack{
protected:
    unsigned int bottom; // number of top element in currently used memory block
    unsigned int block; // number of currently used block
    unsigned int STACK_BLOCK_SIZE; // number of elements in block
    unsigned int STACK_NUMBER_BLOCKS; // number of reserved blocks

    std::vector<ObjectPtr * > stack; // blocks container

public:
    FastStack(unsigned int number_blocks, unsigned int block_size)
    {
        STACK_BLOCK_SIZE = block_size;
        STACK_NUMBER_BLOCKS = number_blocks;
        assert(STACK_BLOCK_SIZE != 0 && STACK_NUMBER_BLOCKS != 0); // both must be > 0
        stack.resize(STACK_NUMBER_BLOCKS);
        for (unsigned int i = 0; i < STACK_NUMBER_BLOCKS; i++)
            stack[i] = NULL;
        bottom = 0;
        block = 0;
        stack[0] = (ObjectPtr *)STD_MALLOC(STACK_BLOCK_SIZE*sizeof(ObjectPtr *));
        memset(stack[0], 0, STACK_BLOCK_SIZE*sizeof(ObjectPtr));
    }
    ~FastStack()
    {
        for (unsigned int i = 0; i < stack.size(); i++)
        {
            if (stack[i] != NULL)
                STD_FREE(stack[i]);
        }
    }

    // clean the stack
    inline void clear()
    {
        for (unsigned int i = 0; i <= block; i++)
        {
            memset(stack[i], 0, STACK_BLOCK_SIZE*sizeof(ObjectPtr));
        }
        bottom = 0;
        block = 0;
    }

    // get number of elemtnts in the stack
    inline unsigned int size()
    {
        assert(bottom <= STACK_BLOCK_SIZE);
        return (bottom + block*STACK_BLOCK_SIZE);
    }

    // checks if the stack is empty
    inline bool empty()
    {
        return (size() == 0)?true:false;
    }

    // push element to the top of the stack
    inline bool push_back(ObjectPtr p_obj)
    {
        assert(bottom <= STACK_BLOCK_SIZE);

        if (bottom >= STACK_BLOCK_SIZE) 
        {
            if((block + 1) >= STACK_NUMBER_BLOCKS)
            {
                // if we reached the end of stack - push back new element
                // !!! better not to do this - performance drop!!! 
                // You shoud allocate more elements in STACK_NUMBER_BLOCKS parameter (do it in constructor)
                assert(block < stack.size());
                if ((block + 1) >= stack.size())
                {
                    stack.push_back(NULL);
                }
            }

            // the stack has space for enlargement
            // go to next block
            block ++;
            bottom = 0;
            if (stack[block] == NULL)
            {
                stack[block] = (ObjectPtr *)STD_MALLOC(STACK_BLOCK_SIZE*sizeof(ObjectPtr));
                memset(stack[block], 0, STACK_BLOCK_SIZE*sizeof(ObjectPtr));
            }
        }

        stack[block][bottom] = p_obj;
        (bottom)++;
        return true;
    }

    // pop top element from the stack
    inline ObjectPtr pop_back()
    {
        assert(bottom <= STACK_BLOCK_SIZE);

        if (bottom == 0) 
        {
            if (block == 0)
            {
                // Mark stack is empty
                assert(stack[block][bottom] == NULL);
                return NULL;
            }
            else // need to go to previous block
            {
                bottom = STACK_BLOCK_SIZE;
                block --;
            }
        }

        ObjectPtr p_ret = stack[block][bottom - 1];
        // Means better not have a null reference
        assert(p_ret != NULL);
        // Adjust TOS
        (bottom)--;
        // Null that entry
        stack[block][bottom] = NULL;

        return p_ret;
    }
} ;


#endif //_fast_stack_H_
