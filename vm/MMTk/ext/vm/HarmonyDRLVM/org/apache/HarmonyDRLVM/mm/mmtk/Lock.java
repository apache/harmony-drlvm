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

package org.apache.HarmonyDRLVM.mm.mmtk;

import org.vmmagic.unboxed.*;
import org.vmmagic.pragma.*;

import org.mmtk.utility.Log;

/**
 * Simple, fair locks with deadlock detection.
 *
 * The implementation mimics a deli-counter and consists of two values: 
 * the ticket dispenser and the now-serving display, both initially zero.
 * Acquiring a lock involves grabbing a ticket number from the dispenser
 * using a fetchAndIncrement and waiting until the ticket number equals
 * the now-serving display.  On release, the now-serving display is
 * also fetchAndIncremented.
 * 
 */
public class Lock extends org.mmtk.vm.Lock implements Uninterruptible {


  static private int idx;             // lock id (based on a non-resetting counter)
  static Object criticalSection = new Object();

  // Core Instance fields
  private String name;        // logical name of lock
  private int id;
  private Thread owningThread;
  private int recursionCount;

  public Lock() 
  {
        this("no name lock");
  }

  public Lock(String name) {
    this.name = name;
    
      try 
      {
          synchronized(criticalSection) 
          {
              this.id = Lock.idx;
              Lock.idx++;
          }
      } 
      catch (Exception e) {System.out.println("org.apache.HarmonyDRLVM.mm.mmtk.Lock() has a problem");  }
  }
 

  public void setName(String str) {
    name = str;
  }

  public void acquire() {
        //System.out.println("org.apache.HarmonyDRLVM.mm.mmtk.Lock.acquire(): " + name);
   
      try 
      {
          while(true) 
          {
              synchronized(criticalSection) 
              {
                  if (recursionCount == 0) 
                  {
                      recursionCount++;
                      owningThread = Thread.currentThread();
                      break;
                  }
                  else if (  owningThread == Thread.currentThread()  ) 
                      {
                          recursionCount++;
                          break;
                      }
              }
              Thread.sleep(500);
          }
      } 
      catch (Exception e) 
      { 
          System.out.println("org.apache.HarmonyDRLVM.mm.mmtk.Lock.acquire() has a problem: " + e);
      }
  }

  public void check (int w) {
        //System.out.println("org.apache.HarmonyDRLVM.mm.mmtk.Lock.check(), w = " + w + name);    
  }

  // Release the lock by incrementing serving counter.
  // (1) The sync is needed to flush changes made while the lock is held and also prevent 
  //        instructions floating into the critical section.
  // (2) When verbose, the amount of time the lock is ehld is printed.
  //
  public void release() 
  {
       //System.out.println("org.apache.HarmonyDRLVM.mm.mmtk.Lock.release(): " + name + "--- " + this);
      try 
      {
              synchronized(criticalSection) 
              {
                  if (recursionCount != 0) 
                  {
                      if (owningThread != Thread.currentThread() ) 
                      {
                          System.out.println("org.apache.HarmonyDRLVM.mm.mmtk.Lock.release() -- locks out of balance 1");
                      } 
                      else 
                      {
                          recursionCount--;
                          if (recursionCount ==0) owningThread = null;
                      }
                  } else System.out.println("org.apache.HarmonyDRLVM.mm.mmtk.Lock.release() -- locks out of balance 2");
              }
      } 
      catch (Exception e) 
      { 
          System.out.println("org.apache.HarmonyDRLVM.mm.mmtk.Lock.acquire() has a problem: " + e);
      }
  }
}
