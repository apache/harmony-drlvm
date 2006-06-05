/* Copyright 2000, 2005 The Apache Software Foundation or its licensors, as applicable
 * 
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 *     http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package org.apache.harmony.eclipse.jdt.launching;

import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Platform;
import org.eclipse.core.runtime.Plugin;
import org.eclipse.core.runtime.Status;
import org.osgi.framework.BundleContext;

public class HyLaunchingPlugin extends Plugin {

	/**
	 * Unique identifier constant.
	 */
	private static final String PI_Harmony_Launching = "org.apache.harmony.jdt.launching"; //$NON-NLS-1$

	private static HyLaunchingPlugin plugin;

	static final String DEBUG_PLUGIN = PI_Harmony_Launching + "/plugin"; //$NON-NLS-1$

	static final String DEBUG_INSTALLING = PI_Harmony_Launching + "/vm/installing"; //$NON-NLS-1$

	static final String DEBUG_LAUNCHING = PI_Harmony_Launching + "/application/launching"; //$NON-NLS-1$

	/**
	 * Default constructor.
	 */
	public HyLaunchingPlugin() {
		super();
		plugin = this;
	}

	public void start(BundleContext context) throws Exception {
		super.start(context);

		if (isDebugging()
				&& (Platform.getDebugOption(DEBUG_PLUGIN)
						.equalsIgnoreCase("true"))) { //$NON-NLS-1$
			System.out.println(PI_Harmony_Launching + "--> Plugin started"); //$NON-NLS-1$
		}
	}

	public void stop(BundleContext context) throws Exception {
		if (isDebugging()
				&& (Platform.getDebugOption(DEBUG_PLUGIN)
						.equalsIgnoreCase("true"))) { //$NON-NLS-1$
			System.out.println(PI_Harmony_Launching + "--> Plugin stopping"); //$NON-NLS-1$
		}

		super.stop(context);
	}

       public static synchronized HyLaunchingPlugin getDefault() {
               if(plugin==null) {
                       plugin=new HyLaunchingPlugin();
               }
		return plugin;
	}

	/**
	 * Convenience method which returns the unique identifier of this plugin.
	 */
	public static String getUniqueIdentifier() {
		return PI_Harmony_Launching;
	}

	/**
	 * Convenience method to write status information to the platform log.
	 * 
	 * @param msg
	 *            information to be written to the platform log.
	 */
	public void log(String msg) {
		log(msg, null);
	}

	/**
	 * Convenience method to write problem information to the platform log.
	 * 
	 * @param msg
	 *            additional information about the event
	 * @param e
	 *            exception encapsulating any non-fatal problem
	 */
	public void log(String msg, Exception e) {
		getLog().log(
				new Status(IStatus.INFO, getUniqueIdentifier(), IStatus.OK,
						msg, e));
	}

	/**
	 * Convenience method to write error information to the platform log.
	 * 
	 * @param msg
	 *            additional information about the event
	 * @param e
	 *            exception encapsulating the error
	 */
	public void logError(String msg, Exception e) {
		getLog().log(
				new Status(IStatus.INFO, getUniqueIdentifier(), IStatus.ERROR,
						msg, e));
	}

	/**
	 * Convenience method that returns a boolean indication of whether or not
	 * the plug-in is running with debug trace on <em>and</em> if the option
	 * to allow trace messages about the VM installation activities has been
	 * specified.
	 * 
	 * @return <code>true</code> if tracing of VM install activities is
	 *         enabled, otherwise <code>false</code>.
	 */
	public static boolean isDebuggingInstalling() {
		boolean result = false;
		if (plugin.isDebugging()) {
			String option = Platform.getDebugOption(DEBUG_INSTALLING);
			if ((option != null) && option.equalsIgnoreCase("true")) { //$NON-NLS-1$
				result = true;
			}
		}
		return result;
	}
}
