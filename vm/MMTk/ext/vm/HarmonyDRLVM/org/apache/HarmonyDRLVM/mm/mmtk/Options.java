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
   
    System.out.println("org.apache.HarmonyDRLVM.mm.mmtk.Options.getKey(String) " + name);    

    int space = name.indexOf(' ');
    if (space < 0) return name.toLowerCase();

    String word = name.substring(0, space); 
    String key = word.toLowerCase();
    
    do {
      int old = space+1;
      space = name.indexOf(' ', old);
      if (space < 0) {
        key += name.substring(old);
        return key;
      }
      key += name.substring(old, space);
    } while (true);

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


  /**
   * Take a string (most likely a command-line argument) and try to proccess it
   * as an option command.  Return true if the string was understood, false 
   * otherwise.
   *
   * @param arg a String to try to process as an option command
   * @return true if successful, false otherwise
   */
  public static boolean process(String arg) {

    // First handle the "option commands"
    if (arg.equals("help")) {
       System.out.println("org.apache.HarmonyDRLVM.mm.mmtk.process() help was called"); //printHelp();
       return true;
    }
    if (arg.equals("printOptions")) {
       System.out.println("org.apache.HarmonyDRLVM.mm.mmtk.process() printOptions was called"); //printHelp();//printOptions();
       return true;
    }
    if (arg.equals("")) {
      System.out.println("org.apache.HarmonyDRLVM.mm.mmtk.process() <zip> was called"); //printHelp();
      //printHelp();
      return true;
    }

    // Required format of arg is 'name=value'
    // Split into 'name' and 'value' strings
    int split = arg.indexOf('=');
    if (split == -1) {
      System.out.println("  Illegal option specification!\n  \""+arg+
                  "\" must be specified as a name-value pair in the form of option=value");
      return false;
    }

    return false;
  }
}
