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
/**
 * @author Evgueni Brevnov, Alexey V. Varlamov
 * @version $Revision: 1.1.2.2.4.4 $
 */

package java.lang;

import java.net.URL;
import java.net.JarURLConnection;
import java.security.AccessController;
import java.security.PrivilegedAction;
import java.util.Map;
import java.util.Hashtable;
import java.util.Collection;
import java.util.NoSuchElementException;
import java.util.StringTokenizer;
import java.util.jar.Attributes;
import java.util.jar.Manifest;

import org.apache.harmony.vm.VMStack;

/**
 * @com.intel.drl.spec_ref 
 */
public class Package {
    
    /**
     * A map of {url<String>, attrs<Manifest>} pairs for caching 
     * attributes of bootsrap jars.
     */
    private static final Map jarCache = new Hashtable();

    /**
     * An url of a source jar, for deffered attributes initialization.
     * After the initialization, if any, is reset to null.
     */
    private String jar;  
    
    private String implTitle;

    private String implVendor;

    private String implVersion;

    private String name;

    private URL sealBase;

    private String specTitle;

    private String specVendor;

    private String specVersion;

    /**
     * name can not be null.
     */
    Package(String packageName, String sTitle, String sVersion, String sVendor,
            String iTitle, String iVersion, String iVendor, URL base) {
        name = packageName;
        specTitle = sTitle;
        specVersion = sVersion;
        specVendor = sVendor;
        implTitle = iTitle;
        implVersion = iVersion;
        implVendor = iVendor;
        sealBase = base;
    }
    
    /**
     * Lazy initialization constructor; this Package instance will try to 
     * resolve optional attributes only if such value is requested. 
     * Name must not be null.
     */
    Package(String packageName, String jar) {
        name = packageName;
        this.jar = jar;
    }

    /**
     * @com.intel.drl.spec_ref 
     */
    public static Package getPackage(String name) {
        ClassLoader callerLoader = VMClassRegistry.getClassLoader(VMStack
                .getCallerClass(0));
        return callerLoader == null ? ClassLoader.BootstrapLoader
                .getPackage(name) : callerLoader.getPackage(name);
        }

    /**
     * @com.intel.drl.spec_ref 
     */
    public static Package[] getPackages() {
        ClassLoader callerLoader = VMClassRegistry.getClassLoader(VMStack
                .getCallerClass(0));
        if (callerLoader == null) {
            Collection pkgs = ClassLoader.BootstrapLoader.getPackages();
            return (Package[]) pkgs.toArray(new Package[pkgs.size()]);
        }
        return callerLoader.getPackages();
    }

    /**
     * @com.intel.drl.spec_ref 
     */
    public String getImplementationTitle() {
        if (jar != null) {
            init();
        }
        return implTitle;
    }

    /**
     * @com.intel.drl.spec_ref 
     */
    public String getImplementationVendor() {
        if (jar != null) {
            init();
        }
        return implVendor;
    }

    /**
     * @com.intel.drl.spec_ref 
     */
    public String getImplementationVersion() {
        if (jar != null) {
            init();
        }
        return implVersion;
    }

    /**
     * @com.intel.drl.spec_ref 
     */
    public String getName() {
        return name;
    }

    /**
     * @com.intel.drl.spec_ref 
     */
    public String getSpecificationTitle() {
        if (jar != null) {
            init();
        }
        return specTitle;
    }

    /**
     * @com.intel.drl.spec_ref 
     */
    public String getSpecificationVendor() {
        if (jar != null) {
            init();
        }
        return specVendor;
    }

    /**
     * @com.intel.drl.spec_ref 
     */
    public String getSpecificationVersion() {
        if (jar != null) {
            init();
        }
        return specVersion;
    }

    /**
     * @com.intel.drl.spec_ref 
     */
    public int hashCode() {
        return name.hashCode();
    }

    /**
     * @com.intel.drl.spec_ref 
     */
    public boolean isCompatibleWith(String desiredVersion)
            throws NumberFormatException {
        if (jar != null) {
            init();
        }

        StringTokenizer specVersionTokens = new StringTokenizer(specVersion,
                ".");
        StringTokenizer desiredVersionTokens = new StringTokenizer(
                desiredVersion, ".");
        try {
            while (specVersionTokens.hasMoreElements()) {
                int desiredVer = Integer.parseInt(desiredVersionTokens
                        .nextToken());
                int specVer = Integer.parseInt(specVersionTokens.nextToken());
                if (specVer != desiredVer) {
                    return specVer > desiredVer;
                }
            }
        } catch (NoSuchElementException e) {
        	
        	/* 
        	 * ignore - this seems to be the case when we run out of tokens
        	 * for desiredVersion
        	 */
        }
        
        /*
         *   now, if desired is longer than spec, and they have been 
         *   equal so far (ex.  1.4  <->  1.4.0.0) then the remainder
         *   better be zeros
         */

    	while (desiredVersionTokens.hasMoreTokens()) {
    		if (0 != Integer.parseInt(desiredVersionTokens.nextToken())) {
        		return false;
        	}
    	}

        
        return true;
    }

    /**
     * @com.intel.drl.spec_ref 
     */
    public boolean isSealed() {
        if (jar != null) {
            init();
        }
        return sealBase != null;
    }

    /**
     * @com.intel.drl.spec_ref 
     */
    public boolean isSealed(URL url) {
        if (jar != null) {
            init();
        }
        return sealBase != null && sealBase.equals(url);
    }

    /**
     * @com.intel.drl.spec_ref 
     */
    public String toString() {
        if (jar != null) {
            init();
        }
        return "package " + name + (specTitle != null ? " " + specTitle : "")
                + (specVersion != null ? " " + specVersion : "");
    }
    
    /**
     * Performs initialization of optional attributes, if the source jar location 
     * was specified in the lazy constructor.
     */
    private void init() {
        try {
            final URL sealURL = new URL(jar);
            Manifest manifest = (Manifest)jarCache.get(jar);
            if (manifest == null) {
                manifest = (Manifest)AccessController.doPrivileged(
                    new PrivilegedAction() {
                        public Object run()
                        {
                            try {
                                return ((JarURLConnection)sealURL
                                        .openConnection()).getManifest();
                            } catch (Exception e) {
                                return new Manifest();
                            }
                        }
                    });
                jarCache.put(jar, manifest);
            }

            Attributes mainAttrs = manifest.getMainAttributes();
            Attributes pkgAttrs = manifest.getAttributes(name);
            specTitle = (specTitle = pkgAttrs
                    .getValue(Attributes.Name.SPECIFICATION_TITLE)) == null
                    ? mainAttrs.getValue(Attributes.Name.SPECIFICATION_TITLE)
                    : specTitle;
            specVersion = (specVersion = pkgAttrs
                    .getValue(Attributes.Name.SPECIFICATION_VERSION)) == null
                    ? mainAttrs.getValue(Attributes.Name.SPECIFICATION_VERSION)
                    : specVersion;
            specVendor = (specVendor = pkgAttrs
                    .getValue(Attributes.Name.SPECIFICATION_VENDOR)) == null
                    ? mainAttrs.getValue(Attributes.Name.SPECIFICATION_VENDOR)
                    : specVendor;
            implTitle = (implTitle = pkgAttrs
                    .getValue(Attributes.Name.IMPLEMENTATION_TITLE)) == null
                    ? mainAttrs.getValue(Attributes.Name.IMPLEMENTATION_TITLE)
                    : implTitle;
            implVersion = (implVersion = pkgAttrs
                    .getValue(Attributes.Name.IMPLEMENTATION_VERSION)) == null
                    ? mainAttrs
                        .getValue(Attributes.Name.IMPLEMENTATION_VERSION)
                    : implVersion;
            implVendor = (implVendor = pkgAttrs
                    .getValue(Attributes.Name.IMPLEMENTATION_VENDOR)) == null
                    ? mainAttrs.getValue(Attributes.Name.IMPLEMENTATION_VENDOR)
                    : implVendor;
            String sealed = (sealed = pkgAttrs
                    .getValue(Attributes.Name.SEALED)) == null ? mainAttrs
                    .getValue(Attributes.Name.SEALED) : sealed;
            sealBase = Boolean.valueOf(sealed).booleanValue() ? sealURL
                    : null;
        } catch (Exception e) {}
        jar = null;
    }
}
