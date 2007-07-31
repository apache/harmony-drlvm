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
 
class Small
{
    private int i, j;
};

class MyThread extends Thread
{
    int tableSize;
    int iterations;

    public MyThread(int _tableSize, int _iterations)
    {
        tableSize = _tableSize;
        iterations = _iterations;
    }

    public  void    run()
    {
        Small[] table = new Small[tableSize];

        int iter = 0;
        while (iter < iterations)
        {
            for (int i = 0; i < table.length; i++)
            {
                table[i] = new Small();             
                iter++;
            }
        }
    }
};

public class ThreadSim
{
    public static void test(int threadCount, int tableSize, int iterations)
    {
        MyThread t;

        System.out.println("Timing  " + threadCount + " threads, retaining "  + tableSize*threadCount + " Objects:  " );

        long    start = System.currentTimeMillis();

        MyThread[] threads = new MyThread[threadCount];

        
        for (int i = 0; i < threadCount; i++)
        {
             
             threads[i] = new MyThread(tableSize, iterations);
             threads[i].start();
        }

	try{
        	for (int i = 0; i < threadCount; i++)
        	{
             	threads[i].join();
        	}
	}
	catch(Exception e)
	{}
        
    
        double elapsedSeconds = (System.currentTimeMillis() - start)/1000.0;
        int smallSize = 16;
        long allocatedMegaBytes = (long)iterations*threadCount*smallSize/(1024*1024);

        System.out.println(elapsedSeconds + " seconds  " + allocatedMegaBytes/elapsedSeconds + " MB/sec");
    }

    public static void main (String[] args)
    {
        int iter = 50*1000*1000;

        if (args.length >= 1)
            iter = Integer.parseInt(args[0]);

        System.out.println("Timing " + iter/(1000*1000) + " million total object allocations");
        System.out.println("Varying number of threads and number of objects retained");
        System.out.println();

        long    start = System.currentTimeMillis();

        for (int threadCount = 1; threadCount <= 32; threadCount *= 2)
        {
            for (int tableSize = 64; tableSize <= 8192; tableSize *= 2)
            {
                test(threadCount, tableSize/threadCount, iter/threadCount);
            }   
        }

        System.out.println("Total: " + (System.currentTimeMillis() - start)/1000.0 + " seconds");
    }
}
