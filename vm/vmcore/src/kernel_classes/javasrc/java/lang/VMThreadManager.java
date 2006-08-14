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
 * @author Evgueni Brevnov, Roman S. Bushmanov
 * @version $Revision: 1.1.2.1.4.4 $
 */
package java.lang;

/**
 * Provides the methods to interact with VM Thread Manager that are used by
 * {@link java.lang.Thread Thread} class and {@link java.lang.Object Object}
 * synchronization implementation.
 * <p>
 * A public constructor of the <code>java.lang.Thread</code> class should be
 * called by the VM within startup phase to initialize the main thread.
 * Note that the <code>VMThread.currentThread()</code> method should return a
 * reference to the Thread object even if it hasn't been initialized yet.
 * <p>
 * This class must be implemented according to the common policy for porting
 * interfaces - see the porting interface overview for more details.
 * 
 * @api2vm
 */
final class VMThreadManager {

    /**
     * This class is not supposed to be instantiated.
     */
    private VMThreadManager() {
    }

    /**
     * This method satisfies the requirements of the specification for the
     * {@link Thread#countStackFrames() Thread.countStackFrames()} method.
     * @api2vm
     */
    static native int countStackFrames(Thread thread);

    /**
     * This method satisfies the requirements of the specification for the
     * {@link Thread#currentThread() Thread.currentThread()} method.
     * <p> 
     * But there is one special case. When this method is called while
     * initializing the main thread it should return a reference to the main
     * thread object even if it hasn't been initialized yet.
     * <p>
     * In other words the condition (VMThreadManager.currentThread() == this)
     * should be satisfied for <code>java.lang.Thread</code> object in the only
     * case: this object was created by VM to represent the application's main
     * thread and hasn't been initialized yet.
     * @api2vm
     */
    static native Thread currentThread();

    /**
     * This method satisfies the requirements of the specification for the
     * {@link Thread#holdsLock(java.lang.Object) Thread.holdsLock(Object obj)}
     * method. But it doesn't throw an <code>NullPointerException</code>
     * exception. The <code>object</code> argument must not be null.
     * @api2vm
     */
    static native boolean holdsLock(Object object);

    /**
     * This method satisfies the requirements of the specification for the
     * {@link Thread#interrupt() Thread.interrupt()} method. 
     * @api2vm
     */
    static native void interrupt(Thread thread);

    /**
     * Checks if the specified thread is dead.
     * <p>
     * <b>Note:</b> This method is used for the {@link Thread#isAlive()
     * Thread.isAlive()} method implementation.
     * 
     * @param thread the thread to check the status for.
     * @return true if the thread has died already, false otherwise.
     * @api2vm
     */
    static native boolean isDead(Thread thread);

    /**
     * This method satisfies the requirements of the specification for the
     * {@link Thread#interrupted() Thread.interrupted()} method.
     * @api2vm
     */
    static native boolean isInterrupted();

    /**
     * This method satisfies the requirements of the specification for the
     * {@link Thread#isInterrupted() Thread.isInterrupted()} method.
     * @api2vm
     */
    static native boolean isInterrupted(Thread thread);

    /**
     * This method satisfies the requirements of the specification for the
     * {@link Thread#join(long, int) Thread.join(long timeout, int nanos)}
     * method. But it doesn't throw an <code>IllegalArgumentException</code>
     * exception. The <code>millis</code> and <code>nanos</code> arguments
     * must be valid.
     * @api2vm
     */
    static native void join(Thread thread, long millis, int nanos)
        throws InterruptedException;

    /**
     * This method satisfies the requirements of the specification for the
     * {@link Object#notify() Object.notify()} method.
     * @api2vm
     */
    static native void notify(Object object);

    /**
     * This method satisfies the requirements of the specification for the
     * {@link Object#notifyAll() Object.notifyAll()} method.
     * @api2vm
     */
    static native void notifyAll(Object object);

    /**
     * This method satisfies the requirements of the specification for the
     * {@link Thread#resume() Thread.resume()} method.
     * @api2vm
     */
    static native void resume(Thread thread);

    /**
     * Changes the priority of the specified thread.
     * <p>
     * <b>Note:</b> This method is used for the {@link Thread#setPriority(int)
     * Thread.setPriority(int newPriority)} method implementation.
     * 
     * @param thread the thread to change the priority for
     * @param priority new priority value
     * @api2vm
     */
    static native void setPriority(Thread thread, int priority);

    /**
     * This method satisfies the requirements of the specification for the
     * {@link Thread#sleep(long, int) Thread.sleep(long timeout, int nanos)}
     * method. But it doesn't throw an <code>IllegalArgumentException</code>
     * exception. The <code>millis</code> and <code>nanos</code> arguments
     * must be valid.
     * @api2vm
     */
    static native void sleep(long millis, int nanos)
        throws InterruptedException;

    /**
     * Starts the specified thread. Causes JVM to start executing
     * <code>run()</code> method of the specified thread.
     * <p>
     * The method allows to set the stack size for the thread specified in its
     * {@link Thread#Thread(java.lang.ThreadGroup, java.lang.Runnable,
     * java.lang.String, long) constructor}.
     * <p>
     * <b>Note:</b> This method is used for the {@link Thread#start()
     * Thread.start()} method implementation.
     * 
     * @param thread the thread to be started
     * @param size the desired stack size in bytes or zero to be ignored
     * @api2vm
     */
    static native void start(Thread thread, long size);

    /**
     * This method satisfies the requirements of the specification for the
     * {@link Thread#stop(java.lang.Throwable) Thread.stop(Throwable obj)}
     * method. But it doesn't throw an <code>NullPointerException</code>
     * exception. The <code>throwable</code> argument must not be null.
     * @api2vm
     */
    static native void stop(Thread thread, Throwable throwable);

    /**
     * This method satisfies the requirements of the specification for the
     * {@link Thread#suspend() Thread.suspend()} method.
     * @api2vm
     */
    static native void suspend(Thread thread);

    /**
     * This method satisfies the requirements of the specification for the
     * {@link Object#wait(long, int) Object.wait(long timeout, int nanos)}
     * method. But it doesn't throw an <code>IllegalArgumentException</code>
     * exception. The <code>millis</code> and <code>nanos</code> arguments
     * must be valid.
     * @api2vm
     */
    static native void wait(Object object, long millis, int nanos)
        throws InterruptedException;

    /**
     * This method satisfies the requirements of the specification for the
     * {@link Thread#yield() Thread.yield()} method.
     * @api2vm
     */
    static native void yield();
}
