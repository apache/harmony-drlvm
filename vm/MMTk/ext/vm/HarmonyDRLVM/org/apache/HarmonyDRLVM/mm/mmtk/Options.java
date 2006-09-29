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

package org.apache.HarmonyDRLVM.mm.mmtk;
 
import org.vmmagic.unboxed.Extent;

import org.mmtk.utility.options.*;


/**
 * Class to handle command-line arguments and options for GC.
 */
public final class Options extends org.mmtk.vm.Options {

  /**
   * Map a name into a key in the VM's format
   *
   * @param name the space delimited name. 
   * @return the vm specific key.
   */
  public final String getKey(String name) {
    //System.out.println("org.apache.HarmonyDRLVM.mm.mmtk.Options.getKey(String) " + name);    
    return null;
  }

  /**
   * Failure during option processing. This must never return.
   *
   * @param o The option that was being set.
   * @param message The error message.
   */
  public final void fail(Option o, String message) {
      System.out.println("ERROR: Option '" + o.getKey() + "' : " + 
                 message);
      Object obj = new Object();
      obj = null;
      try 
      {
          obj.wait();  // this should cause a system exit
      } 
      catch (Exception e) 
      {
          System.out.println("org.apache.HarmonyDRLVM.mm.mmtk.Options.fail has bugs");
      }
  }

  /**
   * Warning during option processing.
   *
   * @param o The option that was being set.
   * @param message The warning message.
   */
  public final void warn(Option o, String message) {
      System.out.println("WARNING: Option '" + o.getKey() + "' : " + 
                    message);
  }
}
