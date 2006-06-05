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
import java.io.FileInputStream;
import java.io.IOException;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Comparator;
import java.util.HashSet;
import java.util.Iterator;
import java.util.List;
import java.util.Properties;
import java.util.Set;
import java.util.SortedSet;
import java.util.TreeSet;

import org.eclipse.core.runtime.IPath;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Path;
import org.eclipse.core.runtime.Status;
import org.eclipse.jdt.launching.AbstractVMInstallType;
import org.eclipse.jdt.launching.IVMInstall;
import org.eclipse.jdt.launching.LibraryLocation;

public class HyVMInstallType extends AbstractVMInstallType {

	static final String LAUNCHER_HOME_TOKEN = "%LAUNCHER_HOME%"; //$NON-NLS-1$

	public IVMInstall doCreateVMInstall(String id) {
		return new HyVMInstall(this, id);
	}

	public String getName() {
		return HyLauncherMessages.getString("HyVMType.name"); //$NON-NLS-1$
	}

	public IStatus validateInstallLocation(File installLocation) {
		// Check we can find the launcher.
		File java = new File(installLocation, "bin" + File.separator + "java"); //$NON-NLS-2$ //$NON-NLS-1$
		File javaExe = new File(installLocation,
				"bin" + File.separator + "java.exe"); //$NON-NLS-2$ //$NON-NLS-1$
		
		// IJVM: check ij vm executables 
		File javaIJ = new File(installLocation, "bin" + File.separator + "ij"); //$NON-NLS-2$ //$NON-NLS-1$
		File javaIJExe = new File(installLocation,
				"bin" + File.separator + "ij.exe"); //$NON-NLS-2$ //$NON-NLS-1$
		if (!(java.isFile() || javaExe.isFile() || javaIJ.isFile() || javaIJExe.isFile())) {
			if (HyLaunchingPlugin.isDebuggingInstalling()) {
				try {
					System.out.println(HyLaunchingPlugin.getUniqueIdentifier()
							+ "--> No Harmony launcher detected at location : " //$NON-NLS-1$
							+ installLocation.getCanonicalPath());
				} catch (IOException e) {
					// Intentionally empty
				}
			}

			return new Status(IStatus.ERROR, HyLaunchingPlugin
					.getUniqueIdentifier(), 0, HyLauncherMessages
					.getString("HyVMType.error.noLauncher"), null); //$NON-NLS-1$
		}

		// Check we can find the bootclasspath.properties file.
		File bootPropsFile = new File(
				installLocation,
				"lib"	+ File.separator + "boot" + File.separator + "bootclasspath.properties"); //$NON-NLS-1$ //$NON-NLS-2$ //$NON-NLS-3$
		if (!bootPropsFile.isFile()) {
			return new Status(IStatus.ERROR, HyLaunchingPlugin
					.getUniqueIdentifier(), 0, HyLauncherMessages
					.getString("HyVMType.error.noBootProperties"), null); //$NON-NLS-1$
		}

		// Everything looks good.
		return new Status(IStatus.OK, HyLaunchingPlugin.getUniqueIdentifier(),
				0, "ok", null); //$NON-NLS-1$
	}

	/*
	 * (non-Javadoc)
	 * 
	 * @see org.eclipse.jdt.launching.IVMInstallType#detectInstallLocation()
	 */
	public File detectInstallLocation() {
		// Try to detect wether the current VM is a Harmony installation.

		// IJVM: 
		// In case of ij vm we have no configuration property. Skip the check - the presence of 
		// bootclasspath.properties file is sufficient for correct installation detection.
		//if (!"clear".equals(System.getProperty("com.ibm.oti.configuration", "missing"))) //$NON-NLS-1$ //$NON-NLS-2$ //$NON-NLS-3$
			//return null;

		File home = new File(System.getProperty("java.home")); //$NON-NLS-1$
		IStatus status = validateInstallLocation(home);
		if (status.isOK()) {
			return home;
		}
		return null;
	}

	/*
	 * (non-Javadoc)
	 * 
	 * @see org.eclipse.jdt.launching.IVMInstallType#getDefaultLibraryLocations(java.io.File)
	 */
	public LibraryLocation[] getDefaultLibraryLocations(File installLocation) {

		// Find kernel types
		LibraryLocation kernel = getKernelLocation(installLocation,
				"default", "clearvm"); //$NON-NLS-1$ //$NON-NLS-2$
		
		// IJVM: 
		// for ij vm kernelSize = 0  
		// for ibm vm kernelSize = 1  
		int kernelSize = 1;
		if (kernel == null) {
			kernelSize = 0;
			// IJVM: continue without kernel libraries
			//return new LibraryLocation[] {};
		}

		List bootLibraries = getBootLibraries(installLocation);
		if (bootLibraries == null) {
			return new LibraryLocation[] {};
		}

		// Find the extension class libraries
		List extensions = getExtensionLibraries(installLocation);

		// Combine the libraries result
		LibraryLocation[] allLibraries = new LibraryLocation[kernelSize
				+ bootLibraries.size() + extensions.size()];
		
		int libraryCount = 0;

		// Start with the kernel library location
		if (kernel != null)
		allLibraries[libraryCount++] = kernel;

		// Append the boot libraries
		for (int i = 0; i < bootLibraries.size(); i++) {
			allLibraries[libraryCount++] = (LibraryLocation) bootLibraries
					.get(i);
		}

		// Append the extensions libraries
		for (int i = 0; i < extensions.size(); i++) {
			allLibraries[libraryCount++] = (LibraryLocation) extensions.get(i);
		}

		// We are done
		return allLibraries;
	}

	private List getBootLibraries(File installLocation) {
		// The location of the bootclasspath libraries
		File bootDirectory = new File(installLocation,
				"lib" + File.separator + "boot"); //$NON-NLS-1$ //$NON-NLS-2$

		// Load the bootclasspath properties file to figure out the required
		// libraries
		Properties bootclasspathProperties = new Properties();
		try {
			FileInputStream propertiesStream = new FileInputStream(new File(
					bootDirectory, "bootclasspath.properties")); //$NON-NLS-1$
			bootclasspathProperties.load(propertiesStream);
			propertiesStream.close();
		} catch (IOException exception) {
			// Cannot find bootclasspath.properties file or cannot read it.
			return null;
		}

		List bootOrder = findBootOrder(bootclasspathProperties);
		if (bootOrder == null) {
			return null;
		}

		List bootLibraries = new ArrayList(bootOrder.size());

		// Interpret the key values, in order, as library locations
		for (Iterator bootOrderKeyItr = bootOrder.iterator(); bootOrderKeyItr
				.hasNext();) {
			String bootOrderKey = (String) bootOrderKeyItr.next();
			// Here '14' is the offset past "bootclasspath."
			String orderSuffix = bootOrderKey.substring(14);

			// The library location first...
			String bootLibraryLocation = bootclasspathProperties
					.getProperty(bootOrderKey);
			File libraryFile = new File(bootDirectory, bootLibraryLocation);
			if (!libraryFile.exists()) {
				// Ignore library descriptions for files that don't exist
				continue;
			}
			IPath libraryPath;
			try {
				libraryPath = new Path(libraryFile.getCanonicalPath());
			} catch (IOException exception1) {
				// Ignore invalid path values.
				continue;
			}

			// The source location can be deduced from the boot library name
			String sourceLocationKey = "bootclasspath.source." + orderSuffix; //$NON-NLS-1$ 
			String sourceLocation = bootclasspathProperties
					.getProperty(sourceLocationKey);
			IPath sourcePath;
			if (sourceLocation == null) {
				// source location was not specified
				sourcePath = new Path(""); //$NON-NLS-1$
			} else {
				File sourceFile = new File(bootDirectory, sourceLocation);
				try {
					sourcePath = new Path(sourceFile.getCanonicalPath());
				} catch (IOException exception1) {
					// If we cannot find the source, we default to missing token
					sourcePath = new Path(""); //$NON-NLS-1$
				}
			}

			// The source package root is the offset in the jar where package
			// names begin
			String sourceRootKey = "bootclasspath.source.packageroot." + orderSuffix; //$NON-NLS-1$
			// Default root location is "/"
			String sourceRoot = bootclasspathProperties.getProperty(
					sourceRootKey, "/"); //$NON-NLS-1$
			IPath sourceRootPath = new Path(sourceRoot);

			// We have everything we need to build up a library location
			LibraryLocation libLocation = new LibraryLocation(libraryPath,
					sourcePath, sourceRootPath);
			bootLibraries.add(libLocation);
		}
		return bootLibraries;
	}

	private List findBootOrder(Properties bootclasspathProperties) {

		// Only keep keys that are "bootclasspath.<something>"
		Set allKeys = bootclasspathProperties.keySet();
		Set bootKeys = new HashSet(allKeys.size());
		for (Iterator iter = allKeys.iterator(); iter.hasNext();) {
			String key = (String) iter.next();
			if ((key.startsWith("bootclasspath.") && //$NON-NLS-1$
			(key.indexOf('.', 14) == -1))) { // Ensure there are no more '.'s
				bootKeys.add(key);
			}
		}
		// Now order the keys by their numerical postfix.
		SortedSet bootOrder = new TreeSet(new Comparator() {
			public int compare(Object object1, Object object2) {
				// Here '14' is the offset past "bootclasspath."
				String str1 = ((String) object1).substring(14);
				String str2 = ((String) object2).substring(14);
				// Puts entries to the end, in any order, if they do not
				// parse.
				int first, second;
				try {
					first = Integer.parseInt(str1);
				} catch (NumberFormatException exception) {
					first = Integer.MAX_VALUE;
				}
				try {
					second = Integer.parseInt(str2);
				} catch (NumberFormatException exception1) {
					second = Integer.MAX_VALUE;
				}
				if (first == second) {
					return 0;
				}
				return (first < second) ? -1 : 1;
			}
		});
		bootOrder.addAll(bootKeys);
		return Arrays.asList(bootOrder.toArray(new String[bootOrder.size()]));
	}

	/**
	 * Returns a list of default extension jars that should be placed on the
	 * build path and runtime classpath, by default.
	 * 
	 * @param installLocation
	 * @return List
	 */
	protected List getExtensionLibraries(File installLocation) {
		File extDir = getDefaultExtensionDirectory(installLocation);
		List extensions = new ArrayList();
		if (extDir != null && extDir.exists() && extDir.isDirectory()) {
			String[] names = extDir.list();
			for (int i = 0; i < names.length; i++) {
				String name = names[i];
				File jar = new File(extDir, name);
				if (jar.isFile()) {
					int length = name.length();
					if (length > 4) {
						String suffix = name.substring(length - 4);
						if (suffix.equalsIgnoreCase(".zip") || suffix.equalsIgnoreCase(".jar")) { //$NON-NLS-1$ //$NON-NLS-2$
							try {
								IPath libPath = new Path(jar.getCanonicalPath());
								LibraryLocation library = new LibraryLocation(
										libPath, Path.ROOT, Path.EMPTY);
								extensions.add(library);
							} catch (IOException e) {
								// Ignored.
							}
						}
					}
				}
			}
		}
		return extensions;
	}

	/**
	 * Returns the default location of the extension directory, based on the
	 * given install location. The resulting file may not exist, or be
	 * <code>null</code> if an extension directory is not supported.
	 * 
	 * @param installLocation
	 * @return default extension directory or <code>null</code>
	 */
	protected File getDefaultExtensionDirectory(File installLocation) {
		File lib = new File(installLocation, "lib"); //$NON-NLS-1$
		File ext = new File(lib, "ext"); //$NON-NLS-1$
		return ext;
	}

	
	LibraryLocation getKernelLocation(File installLocation, String vmdir,
			String vmname) {
		Properties kernelProperties = new Properties();
		File propertyFile = new File(installLocation, "bin" + File.separator + //$NON-NLS-1$
				vmdir + File.separator + vmname + ".properties"); //$NON-NLS-1$
		try {
			FileInputStream propsFile = new FileInputStream(propertyFile);
			kernelProperties.load(propsFile);
			propsFile.close();
		} catch (IOException ex) {
			// IJVM: Print no warnings on ij vm
			//System.out
			//		.println("Warning: could not open properties file " + propertyFile.getPath()); //$NON-NLS-1$
			return null;
		}

		String libStr = tokenReplace(installLocation, kernelProperties
				.getProperty("bootclasspath.kernel")); //$NON-NLS-1$
		IPath libPath = new Path(libStr);

		String srcStr = tokenReplace(installLocation, kernelProperties
				.getProperty("bootclasspath.source.kernel")); //$NON-NLS-1$
		IPath srcPath;
		if (srcStr == null) {
			srcPath = Path.EMPTY;
		} else {
			srcPath = new Path(srcStr);
		}

		String rootStr = tokenReplace(installLocation, kernelProperties
				.getProperty("bootclasspath.source.packageroot.kernel")); //$NON-NLS-1$
		IPath rootPath;
		if (rootStr == null) {
			rootPath = Path.ROOT;
		} else {
			rootPath = new Path(rootStr);
		}

		return new LibraryLocation(libPath, srcPath, rootPath);
	}

	private String tokenReplace(File installLocation, String str) {
		if (str == null) {
			return null;
		}
		int index = str.indexOf(LAUNCHER_HOME_TOKEN);
		if (index == -1) {
			return str;
		}
		String realHome = installLocation.getPath();
		StringBuffer buf = new StringBuffer(str.length() + realHome.length());
		buf.append(str.substring(0, index));
		buf.append(realHome);
		buf.append(IPath.SEPARATOR);
		buf.append("bin"); //$NON-NLS-1$
		buf.append(IPath.SEPARATOR);
		buf.append(str.substring(index + LAUNCHER_HOME_TOKEN.length(), str
				.length()));
		return buf.toString();
	}
}

