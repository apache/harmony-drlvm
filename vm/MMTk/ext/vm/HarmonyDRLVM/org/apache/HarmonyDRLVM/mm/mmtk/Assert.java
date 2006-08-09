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

/*
 * (C) Copyright Department of Computer Science,
 * Australian National University. 2004
 *
 * (C) Copyright IBM Corp. 2001, 2003
 */
package org.apache.HarmonyDRLVM.mm.mmtk;

import org.mmtk.policy.Space;

import org.vmmagic.unboxed.*;
import org.vmmagic.pragma.*;


public class Assert extends org.mmtk.vm.Assert implements Uninterruptible {
  
  protected final boolean getVerifyAssertionsConstant() { return true;}

  /**
   * This method should be called whenever an error is encountered.
   *
   * @param str A string describing the error condition.
   */
  public final void error(String str) {
    System.out.println("org.apache.HarmonyDRLVM.mm.mmtk.Assert.error(): " + str);
    str = null;
    str.notifyAll();  // this should cause a stack trace and exit
  }

  /**
   * Logs a message and traceback, then exits.
   *
   * @param message the string to log
   */
  public final void fail(String message) { 
      System.out.println("org.apache.HarmonyDRLVM.mm.mmtk.Assert.fail(): " + message);
      message = null;
      message.notifyAll();  // this should cause a stack trace and exit 
  }

  public final void exit(int rc) throws UninterruptiblePragma {
      System.out.println("org.apache.HarmonyDRLVM.mm.mmtk.Assert.exit(): " + rc);
      Object obj = new Object();
      obj = null;
      obj.notifyAll();  // this should cause a stack trace and exit
  }

  /**
   * Checks that the given condition is true.  If it is not, this
   * method does a traceback and exits.
   *
   * @param cond the condition to be checked
   */
    public final void _assert(boolean cond) throws InlinePragma 
    {
        if (cond == false) 
        {
            System.out.println("****** org.apache.HarmonyDRLVM.mm.mmtk.Assert._assert() ******");
            Object obj = new Object();
            obj = null;
            obj.notifyAll();
        }
    }


  /**
   * <code>true</code> if assertions should be verified
   */
 /* public static final boolean VerifyAssertions = VM.VerifyAssertions; */

  public final void _assert(boolean cond, String s) throws InlinePragma {
      System.out.println("org.apache.HarmonyDRLVM.mm.mmtk.Assert._assert(): " + s);
      s.notifyAll();  // this should cause a stack trace and exit
  }

  public final void dumpStack() {
      System.out.println("org.apache.HarmonyDRLVM.mm.mmtk.Assert.dumpStack(): ");
      Object obj = new Object();
      obj = null;
      obj.notifyAll();  // this should cause a stack trace and exit
  }

  /**
   * Throw an out of memory exception.  If the context is one where
   * we're already dealing with a problem, first request some
   * emergency heap space.
   */
  public final void failWithOutOfMemoryError()
    throws LogicallyUninterruptiblePragma, NoInlinePragma {
    failWithOutOfMemoryErrorStatic();
  }

  /**
   * Throw an out of memory exception.  If the context is one where
   * we're already dealing with a problem, first request some
   * emergency heap space.
   */
  public static final void failWithOutOfMemoryErrorStatic()
    throws LogicallyUninterruptiblePragma, NoInlinePragma {
    throw new OutOfMemoryError();
  }

  /**
   * Checks if the virtual machine is running.  This value changes, so
   * the call-through to the VM must be a method.  In Jikes RVM, just
   * returns VM.runningVM.
   *
   * @return <code>true</code> if the virtual machine is running
   */
  public final boolean runningVM() { return true; }

}
