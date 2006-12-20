
package org.apache.harmony.drlvm.tests.regression.h2259;

import junit.framework.TestCase;

import java.lang.reflect.InvocationHandler;
import java.lang.reflect.Method;
import java.lang.reflect.Proxy;
import java.lang.reflect.UndeclaredThrowableException;

interface I1 {
	String string(String s) throws ParentException;
}

interface I2 {
	String string(String s) throws SubException;
}

class ParentException extends Exception {}
class SubException extends ParentException {}

public class H2259 extends TestCase {

	/*
	 * When multiple interfaces define the same method, the list of thrown
	 * exceptions are those which can be mapped to another exception in the
	 * other method:
	 * 
	 * String foo(String s) throws SubException, LinkageError;
	 * 
	 * UndeclaredThrowableException wrappers any checked exception which is not
	 * in the merged list. So ParentException would be wrapped, BUT LinkageError
	 * would not be since its not an Error/RuntimeException.
	 * 
	 * interface I1 { String foo(String s) throws ParentException, LinkageError; }
	 * interface I2 { String foo(String s) throws SubException, Error; }
	 */

	public void test_H2259() {

		Object p = Proxy.newProxyInstance(I1.class.getClassLoader(),
                                          new Class[] { I1.class, I2.class },
                                          new InvocationHandler() {
                                  			  public Object invoke(Object proxy,
                                                                   Method method,
                                                                   Object[] args)
                                        					throws Throwable {
                                        						throw new ArrayStoreException();
                                                			}
		});

		I1 proxy = (I1) p;
        int res = 0;

        try {
            proxy.string("error");
        } catch (ParentException e) { // is never thrown
        } catch (UndeclaredThrowableException e) {
        } catch (RuntimeException e) {
            res = 104;
        }
        assertFalse("RuntimeException was not thrown", res == 0);
	}
}

