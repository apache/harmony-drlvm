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

import java.io.File;
import java.util.ArrayList;
import java.util.List;
import java.util.Map;

import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.core.runtime.NullProgressMonitor;
import org.eclipse.core.runtime.Platform;
import org.eclipse.core.runtime.SubProgressMonitor;
import org.eclipse.debug.core.ILaunch;
import org.eclipse.debug.core.model.IProcess;
import org.eclipse.jdt.launching.IJavaLaunchConfigurationConstants;
import org.eclipse.jdt.launching.IVMInstall;
import org.eclipse.jdt.launching.VMRunnerConfiguration;

public class HyVMRunner extends JavaVMRunner {
	
	public HyVMRunner(IVMInstall vmInstance) {
		super(vmInstance);
	}

	/**
	 * @see org.eclipse.jdt.launching.IVMRunner#run(VMRunnerConfiguration, ILaunch, IProgressMonitor)
	 */
	public void run(VMRunnerConfiguration config, ILaunch launch, IProgressMonitor monitor) throws CoreException {
		
		if (monitor == null) {
			monitor = new NullProgressMonitor();
		}
		
		// check for cancellation
		if (monitor.isCanceled()) {
			return;
		}
		
		IProgressMonitor subMonitor = new SubProgressMonitor(monitor, 1);
		subMonitor.beginTask(HyLauncherMessages.getString("HyVMRunner.Launching_virtual_machine..._1"), 2); //$NON-NLS-1$
		subMonitor.subTask(HyLauncherMessages.getString("HyVMRunner.Constructing_command_line..._2"));	 //$NON-NLS-1$
		
		File workingDir = getWorkingDir(config);
		String location= getJDKLocation();
		String program = constructProgramString(location, config);
		List arguments= new ArrayList();

		arguments.add(program);
		
		addBootClassPathArguments(arguments, config);

		String[] cp= config.getClassPath();
		if (cp.length > 0) {
			arguments.add("-classpath"); //$NON-NLS-1$
			arguments.add(convertClassPath(cp));
		}
		String[] vmArgs= config.getVMArguments();
		addArguments(vmArgs, arguments);
		
		arguments.add(config.getClassToLaunch());
		
		String[] programArgs= config.getProgramArguments();
		addArguments(programArgs, arguments);
				
		String[] cmdLine= new String[arguments.size()];
		arguments.toArray(cmdLine);
		
		String[] envp= config.getEnvironment();
		
		// check for cancellation
		if (monitor.isCanceled()) {
			return;
		}
		
		subMonitor.worked(1);
		subMonitor.subTask(HyLauncherMessages.getString("HyVMRunner.Starting_virtual_machine..._3")); //$NON-NLS-1$
		
		Process p= exec(cmdLine, workingDir, envp);
		if (p != null) {
			// Log the current launch command to the platform log
			logLaunchCmd(cmdLine, false);
			if (HyLaunchingPlugin.getDefault().isDebugging()
					&& (Platform
							.getDebugOption(HyLaunchingPlugin.DEBUG_LAUNCHING)
							.equalsIgnoreCase("true"))) { //$NON-NLS-1$
				traceLaunchCmd(cmdLine, envp, false);
			}
			
			IProcess process= newProcess(launch, p, renderProcessLabel(cmdLine), getDefaultProcessMap());
			process.setAttribute(IProcess.ATTR_CMDLINE, renderCommandLine(cmdLine));
		}
		subMonitor.worked(1);
		subMonitor.done();
	}

	/**
	 * Write the launch invocation string to the platform log.
	 * 
	 * @param cmdLine
	 *            a <code>String</code> array whose elements contain the
	 *            VM launch options.
	 * @param debug
	 *            set to <code>true</code> if the launch is being done in
	 *            debug mode.
	 */
	protected void logLaunchCmd(String[] cmdLine, boolean debug) {
		StringBuffer sBuff = new StringBuffer("Launching Harmony VM "); //$NON-NLS-1$
		if (debug) {
			sBuff.append("in debug mode "); //$NON-NLS-1$
		}
		sBuff.append(": "); //$NON-NLS-1$
		sBuff.append(renderCommandLine(cmdLine));
		HyLaunchingPlugin.getDefault().log(sBuff.toString());
	}

	/**
	 * Write the launch invocation string to the debug console.
	 * 
	 * @param cmdLine
	 *            a <code>String</code> array whose elements contain the
	 *            VM launch options.
	 * @param envp
	 *            the launch environment
	 * @param debug
	 *            set to <code>true</code> if the launch is being done in
	 *            debug mode.
	 */
	protected void traceLaunchCmd(String[] cmdLine, String[] envp, boolean debug) {
		StringBuffer sBuff = new StringBuffer(HyLaunchingPlugin
				.getUniqueIdentifier()
				+ "--> Launching Harmony VM "); //$NON-NLS-1$
		if (debug) {
			sBuff.append("in debug mode "); //$NON-NLS-1$
		}
		sBuff.append(": "); //$NON-NLS-1$
		sBuff.append(renderCommandLine(cmdLine));
		sBuff.append("\n"); //$NON-NLS-1$
		if (envp != null) {
			sBuff.append("\t----Environment details begins----\n"); //$NON-NLS-1$
			for (int i = 0; i < envp.length; i++) {
				sBuff.append("\t\t"); //$NON-NLS-1$
				sBuff.append(envp[i]);
				sBuff.append("\n"); //$NON-NLS-1$
			}// end for
			sBuff.append("\t----Environment details ends------\n"); //$NON-NLS-1$
		}// end if
		System.out.println(sBuff);
	}

	protected String constructProgramString(String location, VMRunnerConfiguration config) {
		String command= null;
		Map map= config.getVMSpecificAttributesMap();
		if (map != null) {
			command= (String)map.get(IJavaLaunchConfigurationConstants.ATTR_JAVA_COMMAND);
		}
		
		StringBuffer program= new StringBuffer();
		program.append(location).append(File.separator).append("bin").append(File.separator); //$NON-NLS-1$
		int directoryLength= program.length();
		if (command == null) {
			program.append("javaw.exe"); //$NON-NLS-1$
			File javaw= new File(program.toString());
			if (!javaw.exists()) {
				program.replace(directoryLength, program.length(), "java"); //$NON-NLS-1$
			}
			// IJVM: try to use ij vm executable name 
			File java= new File(program.toString());
			if (!java.exists()) {
				program.replace(directoryLength, program.length(), "ij"); //$NON-NLS-1$
			}
		} else {
			program.append(command);
		}
		
		return program.toString();
	}
}
