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
 * @author Dmitry B. Yershov
 * @version $Revision: 1.1.2.1.4.3 $
 */

package java.lang;

import java.io.IOException;
import java.io.Serializable;

/**
 * @com.intel.drl.spec_ref 
 */
public final class StringBuffer implements Serializable, CharSequence {

    private static final long serialVersionUID = 3388685877147921107L;

    private static final int DEF_CAPACITY = 16;

    int count;

    boolean shared = false;

    char[] value;

    char[] getValue() {
        return value;
    }

    /**
     * @com.intel.drl.spec_ref 
     */
    public StringBuffer() {
        value = new char[DEF_CAPACITY];
    }

    /**
     * @com.intel.drl.spec_ref 
     */
    public StringBuffer(int capacity) {
        if (capacity >= 0) {
            value = new char[capacity];
        } else {
            throw new NegativeArraySizeException();
        }
    }

    /**
     * @com.intel.drl.spec_ref 
     */
    public StringBuffer(String str) {
        count = str.count;
        value = new char[count + DEF_CAPACITY];
        System.arraycopy(str.value, str.offset, value, 0, count);
    }

    /**
     * @com.intel.drl.spec_ref 
     */
    public StringBuffer append(boolean b) {
        return append(b ? "true" : "false");
    }

    /**
     * @com.intel.drl.spec_ref 
     */
    public synchronized StringBuffer append(char c) {
        ensureCapacityNoSync(count + 1);
        value[count++] = c;
        return this;
    }

    /**
     * @com.intel.drl.spec_ref 
     */
    public synchronized StringBuffer append(char[] str) {
        int len = str.length;
        ensureCapacity(count + len);
        System.arraycopy(str, 0, value, count, len);
        count += len;
        return this;
    }

    /**
     * @com.intel.drl.spec_ref 
     */
    public synchronized StringBuffer append(char[] str, int offset, int len) {
        if (offset < 0) {
            throw new ArrayIndexOutOfBoundsException(offset);
        }
        if (len < 0) {
            throw new ArrayIndexOutOfBoundsException(len);
        }
        if (offset + len > str.length) {
            throw new ArrayIndexOutOfBoundsException(offset + len);
        }
        ensureCapacityNoSync(count + len);
        System.arraycopy(str, offset, value, count, len);
        count += len;
        return this;
    }

    /**
     * @com.intel.drl.spec_ref 
     */
    public StringBuffer append(double d) {
        return append(Double.toString(d));
    }

    /**
     * @com.intel.drl.spec_ref 
     */
    public StringBuffer append(float f) {
        return append(Float.toString(f));
    }

    /**
     * @com.intel.drl.spec_ref 
     */
    public StringBuffer append(int i) {
        return append(Integer.toString(i));
    }

    /**
     * @com.intel.drl.spec_ref 
     */
    public StringBuffer append(long l) {
        return append(Long.toString(l));
    }

    /**
     * @com.intel.drl.spec_ref 
     */
    public StringBuffer append(Object obj) {
        return append(obj == null ? "null" : obj.toString());
    }

    /**
     * @com.intel.drl.spec_ref 
     */
    public synchronized StringBuffer append(String str) {
        if (str == null) {
            str = "null";
        }
        ensureCapacity(count + str.count);
        System.arraycopy(str.value, str.offset, value, count, str.count);
        count += str.count;
        return this;
    }

    /**
     * @com.intel.drl.spec_ref 
     */
    public synchronized StringBuffer append(StringBuffer sb) {
        if (sb == null) {
            return append("null");
        }
        synchronized (sb) {
            int length = sb.count;
            ensureCapacityNoSync(count + length);
            System.arraycopy(sb.value, 0, value, count, length);
            count += length;
            return this;
        }
    }

    /**
     * @com.intel.drl.spec_ref 
     */
    public int capacity() {
        return value.length;
    }

    /**
     * @com.intel.drl.spec_ref 
     */
    public synchronized char charAt(int index) {
        if (index >= 0 && index < count) {
            return value[index];
        }
        throw new StringIndexOutOfBoundsException(index);
    }

    /**
     * @com.intel.drl.spec_ref 
     */
    public synchronized StringBuffer delete(int start, int end) {
        if (start < 0 || start > count || start > end) {
            throw new StringIndexOutOfBoundsException(start);
        }
        if (end > count) {
            end = count;
        }
        if (count != end) {
            ensureCapacity(count);
            System.arraycopy(value, end, value, start, count - end);
        }
        count -= (end - start);
        return this;
    }

    /**
     * @com.intel.drl.spec_ref 
     */
    public synchronized StringBuffer deleteCharAt(int index) {
        if (index >= 0 && index < count) {
            if (index < count - 1) {
                ensureCapacity(count);
                System.arraycopy(value, index + 1, value, index, count - index
                    - 1);
            }
            count--;
            return this;
        }
        throw new StringIndexOutOfBoundsException(index);
    }

    /**
     * @com.intel.drl.spec_ref 
     */
    public synchronized void ensureCapacity(int minimumCapacity) {
        ensureCapacityNoSync(minimumCapacity);
    }

    /**
     * @com.intel.drl.spec_ref 
     */
    public synchronized void getChars(int srcBegin, int srcEnd, char[] dst,
        int dstBegin) {
        if (srcBegin < 0) {
            throw new StringIndexOutOfBoundsException(srcBegin);
        }
        if (dstBegin < 0) {
            throw new StringIndexOutOfBoundsException(dstBegin);
        }
        if (srcEnd < srcBegin) {
            throw new StringIndexOutOfBoundsException(srcEnd - srcBegin);
        }
        if (srcEnd > count) {
            throw new StringIndexOutOfBoundsException(srcEnd);
        }
        if (dstBegin + srcEnd - srcBegin > dst.length) {
            throw new StringIndexOutOfBoundsException(dst.length - dstBegin
                - srcEnd + srcBegin);
        }
        System.arraycopy(value, srcBegin, dst, dstBegin, srcEnd - srcBegin);
    }

    /**
     * @com.intel.drl.spec_ref 
     */
    public synchronized int indexOf(String str) {
        if (str.count > 0) {
            int length = count - str.count;
            char firstStrChar = str.charAt(0);
            for (int i = 0; i <= length; i++) {
                if (value[i] == firstStrChar) {
                    if (sbMatches(i, str)) {
                        return i;
                    }
                }
            }
        } else {
            return 0;
        }
        return -1;
    }

    /**
     * @com.intel.drl.spec_ref 
     */
    public synchronized int indexOf(String str, int fromIndex) {
        if (fromIndex < 0)
            fromIndex = 0;
        if (str.count > 0) {
            int length = count - str.count;
            char firstStrChar = str.charAt(0);
            for (int i = fromIndex; i <= length; i++) {
                if (value[i] == firstStrChar) {
                    if (sbMatches(i, str)) {
                        return i;
                    }
                }
            }
        } else {
            return fromIndex > count ? count : fromIndex;
        }
        return -1;
    }

    /**
     * @com.intel.drl.spec_ref 
     */
    public StringBuffer insert(int offset, boolean b) {
        return insert(offset, (b ? "true" : "false"));
    }

    /**
     * @com.intel.drl.spec_ref 
     */
    public synchronized StringBuffer insert(int offset, char c) {
        if (offset >= 0 && offset <= count) {
            ensureCapacity(count + 1);
            System.arraycopy(value, offset, value, offset + 1, count - offset);
            value[offset] = c;
            count++;
            return this;
        }
        throw new StringIndexOutOfBoundsException(offset);
    }

    /**
     * @com.intel.drl.spec_ref 
     */
    public synchronized StringBuffer insert(int offset, char[] str) {
        if (offset >= 0 && offset <= count) {
            int length = str.length;
            ensureCapacity(count + length);
            System.arraycopy(value, offset, value, offset + length, count
                - offset);
            System.arraycopy(str, 0, value, offset, length);
            count += length;
            return this;
        }
        throw new StringIndexOutOfBoundsException(offset);
    }

    /**
     * @com.intel.drl.spec_ref 
     */
    public synchronized StringBuffer insert(int index, char[] str, int offset,
        int len) {
        if (index < 0 || index > count) {
            throw new StringIndexOutOfBoundsException(index);
        }
        if (offset < 0) {
            throw new StringIndexOutOfBoundsException(offset);
        }
        if (len < 0) {
            throw new StringIndexOutOfBoundsException(len);
        }
        if (offset + len > str.length) {
            throw new StringIndexOutOfBoundsException(offset + len);
        }
        ensureCapacityNoSync(count + len);
        System.arraycopy(value, index, value, index + len, count - index);
        System.arraycopy(str, offset, value, index, len);
        count += len;
        return this;
    }

    /**
     * @com.intel.drl.spec_ref 
     */
    public StringBuffer insert(int offset, double d) {
        return insert(offset, Double.toString(d));
    }

    /**
     * @com.intel.drl.spec_ref 
     */
    public StringBuffer insert(int offset, float f) {
        return insert(offset, Float.toString(f));
    }

    /**
     * @com.intel.drl.spec_ref 
     */
    public StringBuffer insert(int offset, int i) {
        return insert(offset, Integer.toString(i));
    }

    /**
     * @com.intel.drl.spec_ref 
     */
    public StringBuffer insert(int offset, long l) {
        return insert(offset, Long.toString(l));
    }

    /**
     * @com.intel.drl.spec_ref 
     */
    public StringBuffer insert(int offset, Object obj) {
        return insert(offset, (obj == null) ? "null" : obj.toString());
    }

    /**
     * @com.intel.drl.spec_ref 
     */
    public synchronized StringBuffer insert(int offset, String str) {
        if (offset >= 0 && offset <= count) {
            if (str == null) {
                str = "null";
            }
            ensureCapacityNoSync(count + str.count);
            System.arraycopy(value, offset, value, offset + str.count, count
                - offset);
            System.arraycopy(str.value, str.offset, value, offset, str.count);
            count += str.count;
            return this;
        }
        throw new StringIndexOutOfBoundsException(offset);
    }

    /**
     * @com.intel.drl.spec_ref 
     */
    public synchronized int lastIndexOf(String str) {
        if (str.count > 0) {
            char firstChar = str.value[str.offset];
            for (int i = count - str.count; i >= 0; i--) {
                if (value[i] == firstChar) {
                    if (sbMatches(i, str)) {
                        return i;
                    }
                }
            }
        } else {
            return count;
        }
        return -1;
    }

    /**
     * @com.intel.drl.spec_ref 
     */
    public synchronized int lastIndexOf(String str, int fromIndex) {
        fromIndex = Math.min(fromIndex, count - str.count);
        if (str.count > 0) {
            char firstChar = str.value[str.offset];
            for (; fromIndex >= 0; fromIndex--) {
                if (value[fromIndex] == firstChar) {
                    if (sbMatches(fromIndex, str)) {
                        return fromIndex;
                    }
                }
            }
        } else {
            return fromIndex >= 0 ? fromIndex : -1;
        }
        return -1;
    }

    /**
     * @com.intel.drl.spec_ref 
     */
    public int length() {
        return count;
    }

    /**
     * @com.intel.drl.spec_ref 
     */
    public synchronized StringBuffer replace(int start, int end, String str) {
        if (start < 0 || start > count) {
            throw new StringIndexOutOfBoundsException(start);
        }
        if (start > end) {
            throw new StringIndexOutOfBoundsException(end - start);
        }
        if (end > count) {
            end = count;
        }
        int newLen = str.count + count - end + start;
        ensureCapacityNoSync(newLen);
        if (newLen != count && end < count) {
            System.arraycopy(value, end, value, end + newLen - count, count
                - end);
        }
        System.arraycopy(str.value, str.offset, value, start, str.count);
        count = newLen;
        return this;
    }

    /**
     * @com.intel.drl.spec_ref 
     */
    public synchronized StringBuffer reverse() {
        char c;
        int lenFor = count >> 1;
        if (!shared) {
            for (int i = 0, j = count - 1; i < lenFor; i++, j--) {
                c = value[i];
                value[i] = value[j];
                value[j] = c;
            }
        } else {
            ensureCapacityNoSync(count);
            for (int i = 0, j = count - 1; i < lenFor; i++, j--) {
                c = value[i];
                value[i] = value[j];
                value[j] = c;
            }
        }
        return this;
    }

    /**
     * @com.intel.drl.spec_ref 
     */
    public synchronized void setCharAt(int index, char ch) {
        if (index >= 0 && index < count) {
            if (!shared) {
                value[index] = ch;
            } else {
                ensureCapacityNoSync(count);
                value[index] = ch;
            }
        } else {
            throw new StringIndexOutOfBoundsException(index);
        }
    }

    /**
     * @com.intel.drl.spec_ref 
     */
    public synchronized void setLength(int newLength) {
        if (newLength >= 0) {
            if (newLength > count) {
                ensureCapacityNoSync(newLength);
                while (count < newLength) {
                    value[count++] = '\u0000';
                }
            } else {
                if (!shared) {
                    count = newLength;
                } else {
                    ensureCapacityNoSync(newLength);
                    count = newLength;
                }
            }
        } else {
            throw new StringIndexOutOfBoundsException(newLength);
        }
    }

    /**
     * @com.intel.drl.spec_ref 
     */
    public CharSequence subSequence(int start, int end) {
        return substring(start, end);
    }

    /**
     * @com.intel.drl.spec_ref 
     */
    public String substring(int start) {
        return substring(start, count);
    }

    /**
     * @com.intel.drl.spec_ref 
     */
    public synchronized String substring(int start, int end) {
        if (start < 0)
            throw new StringIndexOutOfBoundsException(start);
        if (end > count)
            throw new StringIndexOutOfBoundsException(end);
        if (start > end)
            throw new StringIndexOutOfBoundsException(end - start);
        if (end - start == 0)
            return "";
        shared = true;
        return new String(start, end - start, value); 
    }

    /**
     * @com.intel.drl.spec_ref 
     */
    public String toString() {
        return new String(this);
    }

    private void ensureCapacityNoSync(int minimumCapacity) {
        if (shared || minimumCapacity > value.length) {
            int length = (minimumCapacity > value.length ? value.length * 2 + 2
                : value.length);
            length = (minimumCapacity < length ? length : minimumCapacity);
            char[] array = new char[length];
            System.arraycopy(value, 0, array, 0, count);
            value = array;
            shared = false;
        }
    }

    private void readObject(java.io.ObjectInputStream in) throws IOException,
        ClassNotFoundException {
        in.defaultReadObject();
        shared = false;
    }

    private boolean sbMatches(int sbIndex, String str) {
        int strIndex = str.offset;
        int strLen = strIndex + str.count;
        for (; strIndex < strLen; strIndex++, sbIndex++) {
            if (value[sbIndex] != str.value[strIndex]) {
                return false;
            }
        }
        return true;
    }
}