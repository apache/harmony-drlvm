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
 *  Portions, Copyright c 1991-2005 Unicode, Inc.
 *  The following applies to Unicode:
 *
 *  Copyright c 1991-2005 Unicode, Inc. All rights reserved. Distributed under the Terms of Use in
 *
 *  http://www.unicode.org/copyright.html
 *
 *  Permission is hereby granted, free of charge, to any person obtaining
 *  a copy of the Unicode data files and any associated documentation (the "Data Files")
 *  or Unicode software and any associated documentation (the "Software") to deal in the Data Files
 *  or Software without restriction, including without limitation the rights
 *  to use, copy, modify, merge, publish, distribute, and/or sell copies of the Data Files or Software,
 *  and to permit persons to whom the Data Files or Software are furnished to do so, provided that
 *  (a) the above copyright notice(s) and this permission notice appear with all copies of the Data Files or Software,
 *  (b) both the above copyright notice(s) and this permission notice appear in associated documentation, and
 *  (c) there is clear notice in each modified Data File or in the Software as well as in the documentation
 *  associated with the Data File(s) or Software that the data or software has been modified.
 *
 *  THE DATA FILES AND SOFTWARE ARE PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 *  INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 *  NONINFRINGEMENT OF THIRD PARTY RIGHTS. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR HOLDERS INCLUDED
 *  IN THIS NOTICE BE LIABLE FOR ANY CLAIM, OR ANY SPECIAL INDIRECT OR CONSEQUENTIAL DAMAGES, OR
 *  ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 *  NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THE DATA FILES OR SOFTWARE.
 *
 *  Except as contained in this notice, the name of a copyright holder shall not be used in advertising
 *  or otherwise to promote the sale, use or other dealings in these Data Files or Software without prior
 *  written authorization of the copyright holder.
 *
 *  2. Additional terms from the Database:
 *  Copyright c 1995-1999 Unicode, Inc. All Rights reserved.
 *  Disclaimer
 *  The Unicode Character Database is provided as is by Unicode, Inc.
 *  No claims are made as to fitness for any particular purpose.
 *  No warranties of any kind are expressed or implied. The recipient agrees
 *  to determine applicability of information provided. If this file has been purchased
 *  on magnetic or optical media from Unicode, Inc., the sole remedy for any claim will be exchange
 *  of defective media within 90 days of receipt.
 *  This disclaimer is applicable for all other data files accompanying the Unicode Character Database,
 *  some of which have been compiled by the Unicode Consortium, and some of which have been supplied by other sources.
 *  Limitations on Rights to Redistribute This Data
 *  Recipient is granted the right to make copies in any form for internal distribution and
 *  to freely use the information supplied in the creation of products supporting the UnicodeTM Standard.
 *  The files in the Unicode Character Database can be redistributed to third parties or other organizations
 *  (whether for profit or not) as long as this notice and the disclaimer notice are retained.
 *  Information can be extracted from these files and used in documentation or programs,
 *  as long as there is an accompanying notice indicating the source.
 *
*/

/**
 * @author Dmitry B. Yershov
 * @version $Revision: 1.1.2.2.4.6 $
 */

package java.lang;

import java.io.Serializable;
import java.io.UnsupportedEncodingException;
import java.nio.ByteBuffer;
import java.nio.CharBuffer;
import java.nio.charset.Charset;
import java.util.Comparator;
import java.util.Locale;
import java.util.regex.Pattern;

/**
 * @com.intel.drl.spec_ref 
 */
public final class String implements Serializable, Comparable, CharSequence {

    /**
     * @com.intel.drl.spec_ref 
     */
    public static final Comparator CASE_INSENSITIVE_ORDER = new CaseInsensitiveComparator();

    /*
     * The array was generated basing on the Unicode Standard, version 3.0.
     * See the following links:
     * http://www.unicode.org
     * http://www.unicode.org/Public/3.0-Update/SpecialCasing-2.txt
     */
    private static final char[] charUpperSpecial = { '\u0048',
        '\u0331', '\u0054', '\u0308', '\u0057', '\u030a', '\u0059', '\u030a',
        '\u0041', '\u02be', '\u0000', '\u0000', '\u03a5', '\u0313', '\u0000',
        '\u03a5', '\u0313', '\u0300', '\u03a5', '\u0313', '\u0301', '\u03a5',
        '\u0313', '\u0342', '\u1f08', '\u0399', '\u0000', '\u1f09', '\u0399',
        '\u0000', '\u1f0a', '\u0399', '\u0000', '\u1f0b', '\u0399', '\u0000',
        '\u1f0c', '\u0399', '\u0000', '\u1f0d', '\u0399', '\u0000', '\u1f0e',
        '\u0399', '\u0000', '\u1f0f', '\u0399', '\u0000', '\u1f08', '\u0399',
        '\u0000', '\u1f09', '\u0399', '\u0000', '\u1f0a', '\u0399', '\u0000',
        '\u1f0b', '\u0399', '\u0000', '\u1f0c', '\u0399', '\u0000', '\u1f0d',
        '\u0399', '\u0000', '\u1f0e', '\u0399', '\u0000', '\u1f0f', '\u0399',
        '\u0000', '\u1f28', '\u0399', '\u0000', '\u1f29', '\u0399', '\u0000',
        '\u1f2a', '\u0399', '\u0000', '\u1f2b', '\u0399', '\u0000', '\u1f2c',
        '\u0399', '\u0000', '\u1f2d', '\u0399', '\u0000', '\u1f2e', '\u0399',
        '\u0000', '\u1f2f', '\u0399', '\u0000', '\u1f28', '\u0399', '\u0000',
        '\u1f29', '\u0399', '\u0000', '\u1f2a', '\u0399', '\u0000', '\u1f2b',
        '\u0399', '\u0000', '\u1f2c', '\u0399', '\u0000', '\u1f2d', '\u0399',
        '\u0000', '\u1f2e', '\u0399', '\u0000', '\u1f2f', '\u0399', '\u0000',
        '\u1f68', '\u0399', '\u0000', '\u1f69', '\u0399', '\u0000', '\u1f6a',
        '\u0399', '\u0000', '\u1f6b', '\u0399', '\u0000', '\u1f6c', '\u0399',
        '\u0000', '\u1f6d', '\u0399', '\u0000', '\u1f6e', '\u0399', '\u0000',
        '\u1f6f', '\u0399', '\u0000', '\u1f68', '\u0399', '\u0000', '\u1f69',
        '\u0399', '\u0000', '\u1f6a', '\u0399', '\u0000', '\u1f6b', '\u0399',
        '\u0000', '\u1f6c', '\u0399', '\u0000', '\u1f6d', '\u0399', '\u0000',
        '\u1f6e', '\u0399', '\u0000', '\u1f6f', '\u0399', '\u0000', '\u1fba',
        '\u0399', '\u0000', '\u0391', '\u0399', '\u0000', '\u0386', '\u0399',
        '\u0000', '\u0391', '\u0342', '\u0000', '\u0391', '\u0342', '\u0399',
        '\u0391', '\u0399', '\u0000', '\u1fca', '\u0399', '\u0000', '\u0397',
        '\u0399', '\u0000', '\u0389', '\u0399', '\u0000', '\u0397', '\u0342',
        '\u0000', '\u0397', '\u0342', '\u0399', '\u0397', '\u0399', '\u0000',
        '\u0399', '\u0308', '\u0300', '\u0399', '\u0308', '\u0301', '\u0399',
        '\u0342', '\u0000', '\u0399', '\u0308', '\u0342', '\u03a5', '\u0308',
        '\u0300', '\u03a5', '\u0308', '\u0301', '\u03a1', '\u0313', '\u0000',
        '\u03a5', '\u0342', '\u0000', '\u03a5', '\u0308', '\u0342', '\u1ffa',
        '\u0399', '\u0000', '\u03a9', '\u0399', '\u0000', '\u038f', '\u0399',
        '\u0000', '\u03a9', '\u0342', '\u0000', '\u03a9', '\u0342', '\u0399',
        '\u03a9', '\u0399', '\u0000', '\u0046', '\u0046', '\u0000', '\u0046',
        '\u0049', '\u0000', '\u0046', '\u004c', '\u0000', '\u0046', '\u0046',
        '\u0049', '\u0046', '\u0046', '\u004c', '\u0053', '\u0054', '\u0000',
        '\u0053', '\u0054', '\u0000', '\u0544', '\u0546', '\u0000', '\u0544',
        '\u0535', '\u0000', '\u0544', '\u053b', '\u0000', '\u054e', '\u0546',
        '\u0000', '\u0544', '\u053d', '\u0000'           };

    private static Charset defaultCharset = null;

    private static final byte[] indexUp = { 4, 0, 5, 0, 6, 0,
        7, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 8, 9, 10, 11, 12,
        13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30,
        31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48,
        49, 50, 51, 52, 53, 54, 55, 0, 0, 56, 57, 58, 0, 59, 60, 0, 0, 0, 0,
        61, 0, 0, 0, 0, 0, 62, 63, 64, 0, 65, 66, 0, 0, 0, 0, 67, 0, 0, 0, 0,
        0, 68, 69, 0, 0, 70, 71, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 72, 73, 74, 0,
        75, 76, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 77, 78, 79, 0, 80, 81, 0, 0, 0,
        0, 82                                            };

    private static final byte[] indexUp1 = { 83, 84, 85, 86,
        87, 88, 89, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 90, 91, 92, 93, 94 };

    private static final long serialVersionUID = -6849794470754667710L;

        final char[] value;
        final int offset;
        final int count;

    private int hashCode;

    char[] getValue() {
        return value;
    }

    /**
     * Returns the smaller of two int values.
     * Is implemented to avoid loading the Math class on startup
     * @param first the first int value
     * @param second the second int value
     * @return the smaller of the two values
     */
    private static int min(int first, int second) {
        return (first >= second ? second : first);
    }
    
    /**
     * @com.intel.drl.spec_ref 
     */
    public String() {
        this("");
    }

    /**
     * @com.intel.drl.spec_ref 
     */
    public String(byte[] bytes) {
        Charset cs = getDefaultCharset();
        ByteBuffer bb = ByteBuffer.allocate(bytes.length);
        bb.put(bytes, 0, bytes.length);
        bb.position(0);
        CharBuffer cb = cs.decode(bb);
        value = new char[cb.length()];
        cb.get(value);
        this.offset = 0;
        this.count = this.value.length;
    }

    /**
     * @com.intel.drl.spec_ref 
     */
    public String(byte[] ascii, int hibyte) {
        int count = ascii.length;
        value = new char[count];
        this.offset = 0;
        this.count = count;
        hibyte = (hibyte & 0xff) << 8;
        for (int i = 0; i < count; i++) {
            value[i] = (char)(hibyte | (ascii[i] & 0xff));
        }
    }

    /**
     * @com.intel.drl.spec_ref 
     */
    public String(byte[] bytes, int offset, int length) {
        if (offset < 0) {
            throw new StringIndexOutOfBoundsException(offset);
        }
        if (length < 0) {
            throw new StringIndexOutOfBoundsException(length);
        }
        if (bytes.length - offset < length) {
            throw new StringIndexOutOfBoundsException(offset + length);
        }
        Charset cs = getDefaultCharset();
        ByteBuffer bb = ByteBuffer.allocate(length);
        bb.put(bytes, offset, length);
        bb.position(0);
        CharBuffer cb = cs.decode(bb);
        this.value = new char[cb.length()];
        cb.get(value);
        this.offset = 0;
        this.count = this.value.length;
    }

    /**
     * @com.intel.drl.spec_ref 
     */
    public String(byte[] ascii, int hibyte, int offset, int count) {
        if (offset < 0) {
            throw new StringIndexOutOfBoundsException(offset);
        }
        if (count < 0) {
            throw new StringIndexOutOfBoundsException(count);
        }
        if (ascii.length - offset < count) {
            throw new StringIndexOutOfBoundsException(offset + count);
        }
        value = new char[count];
        this.offset = 0;
        this.count = count;
        hibyte = (hibyte & 0xff) << 8;
        for (int i = 0; i < count; i++, offset++) {
            value[i] = (char)(hibyte | (ascii[offset] & 0xff));
        }
    }

    /**
     * @com.intel.drl.spec_ref 
     */
    public String(byte[] bytes, int offset, int length, String charsetName)
        throws UnsupportedEncodingException {
        if (offset < 0) {
            throw new StringIndexOutOfBoundsException(offset);
        }
        if (length < 0) {
            throw new StringIndexOutOfBoundsException(length);
        }
        if (bytes.length - offset < length) {
            throw new StringIndexOutOfBoundsException(offset + length);
        }
        if (charsetName == null) {
            throw new NullPointerException("charsetName");
        }
        Charset cs;
        try {
            cs = Charset.forName(charsetName);
        } catch (Exception ex) {
            UnsupportedEncodingException throwEx = new UnsupportedEncodingException();
            throwEx.initCause(ex);
            throw throwEx;
        }
        ByteBuffer bb = ByteBuffer.allocate(length);
        bb.put(bytes, offset, length);
        bb.position(0);
        CharBuffer cb = cs.decode(bb);
        this.value = new char[cb.length()];
        cb.get(value);
        this.offset = 0;
        this.count = this.value.length;
    }

    /**
     * @com.intel.drl.spec_ref 
     */
    public String(byte[] bytes, String charsetName)
        throws UnsupportedEncodingException {
        int length = bytes.length;
        Charset cs;
        try {
            cs = Charset.forName(charsetName);
        } catch (Exception ex) {
            UnsupportedEncodingException throwEx = new UnsupportedEncodingException();
            throwEx.initCause(ex);
            throw throwEx;
        }
        ByteBuffer bb = ByteBuffer.allocate(length);
        bb.put(bytes, 0, length);
        bb.position(0);
        CharBuffer cb = cs.decode(bb);
        this.value = new char[cb.length()];
        cb.get(value);
        this.offset = 0;
        this.count = this.value.length;
    }

    /**
     * @com.intel.drl.spec_ref 
     */
    public String(char[] value) {
        this.count = value.length;
        this.value = new char[count];
        this.offset = 0;
        System.arraycopy(value, 0, this.value, 0, count);
    }

    /**
     * @com.intel.drl.spec_ref 
     */
    public String(char[] value, int offset, int count) {
        if (offset < 0) {
            throw new StringIndexOutOfBoundsException(offset);
        }
        if (count < 0) {
            throw new StringIndexOutOfBoundsException(count);
        }
        if (value.length - offset < count) {
            throw new StringIndexOutOfBoundsException(offset + count);
        }
        this.value = new char[count];
        this.offset = 0;
        this.count = count;
        System.arraycopy(value, offset, this.value, 0, count);
    }

    /**
     * @com.intel.drl.spec_ref 
     */
    public String(String original) {
        value = original.value;
        count = original.count;
        offset = original.offset;
        hashCode = original.hashCode;
    }

    /**
     * @com.intel.drl.spec_ref 
     */
    public String(StringBuffer buffer) {
        synchronized (buffer) {
            offset = 0;
            count = buffer.count;
            value = buffer.value;
            buffer.shared = true;
        }
    }

    /*
     * Internal constructor. It doesn't create new object char[] for created
     * String object.
     */
    String(int offset, int count, char[] value) {
        this.value = value;
        this.count = count;
        this.offset = offset;
    }

    /**
     * @com.intel.drl.spec_ref 
     */
    public static String copyValueOf(char[] data) {
        return new String(data);
    }

    /**
     * @com.intel.drl.spec_ref 
     */
    public static String copyValueOf(char[] data, int offset, int count) {
        return new String(data, offset, count);
    }

    /**
     * @com.intel.drl.spec_ref 
     */
    public static String valueOf(boolean b) {
        return b ? "true" : "false";
    }

    /**
     * @com.intel.drl.spec_ref 
     */
    public static String valueOf(char c) {
        return new String(0, 1, new char[] { c });
    }

    /**
     * @com.intel.drl.spec_ref 
     */
    public static String valueOf(char[] data) {
        return new String(data);
    }

    /**
     * @com.intel.drl.spec_ref 
     */
    public static String valueOf(char[] data, int offset, int count) {
        return new String(data, offset, count);
    }

    /**
     * @com.intel.drl.spec_ref 
     */
    public static String valueOf(double d) {
        return Double.toString(d);
    }

    /**
     * @com.intel.drl.spec_ref 
     */
    public static String valueOf(float f) {
        return Float.toString(f);
    }

    /**
     * @com.intel.drl.spec_ref 
     */
    public static String valueOf(int i) {
        return Integer.toString(i);
    }

    /**
     * @com.intel.drl.spec_ref 
     */
    public static String valueOf(long l) {
        return Long.toString(l);
    }

    /**
     * @com.intel.drl.spec_ref 
     */
    public static String valueOf(Object obj) {
        return obj == null ? "null" : obj.toString();
    }

    /**
     * @com.intel.drl.spec_ref 
     */
    public char charAt(int index) {
        if (index >= 0 && index < count) {
            return value[offset + index];
        }
        throw new StringIndexOutOfBoundsException(index);
    }
    
    /**
     * @com.intel.drl.spec_ref 
     */
    public int compareTo(Object o) {
        return compareTo((String)o);
    }

    /**
     * @com.intel.drl.spec_ref 
     */
    public int compareTo(String anotherString) {
        int diff;
        int j = anotherString.offset;
        int minLength = min(count, anotherString.count) + offset;
        for (int i = offset; i < minLength; i++, j++) {
            diff = this.value[i] - anotherString.value[j];
            if (diff != 0) {
                return diff;
            }
        }
        return this.count - anotherString.count;
    }

    /**
     * @com.intel.drl.spec_ref 
     */
    public int compareToIgnoreCase(String str) {
        int diff;
        int j = str.offset;
        int minLength = min(count, str.count) + offset;
        for (int i = offset; i < minLength; i++, j++) {
            if (value[i] != str.value[j]) {
                diff = Character.toLowerCase(Character.toUpperCase(value[i]))
                    - Character
                        .toLowerCase(Character.toUpperCase(str.value[j]));
                if (diff != 0) {
                    return diff;
                }
            }
        }
        return this.count - str.count;
    }

    /**
     * @com.intel.drl.spec_ref 
     */
    public String concat(String str) {
        if (str.count == 0) {
            return this;
        }
        if (count == 0) {
            return str;
        }
        char[] charArray = new char[count + str.count];
        System.arraycopy(value, offset, charArray, 0, count);
        System.arraycopy(str.value, str.offset, charArray, count, str.count);
        return new String(0, charArray.length, charArray);
    }

    /**
     * @com.intel.drl.spec_ref 
     */
    public boolean contentEquals(StringBuffer sb) {
        synchronized (sb) {
            if (count != sb.count) {
                return false;
            }
            int j = offset;
            for (int i = 0; i < count; i++, j++) {
                if (value[j] != sb.value[i]) {
                    return false;
                }
            }
            return true;
        }
    }

    /**
     * @com.intel.drl.spec_ref 
     */
    public boolean endsWith(String suffix) {
        int len = suffix.offset + suffix.count;
        int j = offset + count - suffix.count;
        if (j < offset) {
            return false;
        }
        for (int i = suffix.offset; i < len; i++, j++) {
            if (value[j] != suffix.value[i]) {
                return false;
            }
        }
        return true;
    }

    /**
     * @com.intel.drl.spec_ref 
     */
    public boolean equals(Object anObject) {
        if (!(anObject instanceof String)) {
            return false;
        }
        String str = (String)anObject;
        if (count != str.count) {
            return false;
        }
        int j = str.offset;
        int k = offset + count;
        for (int i = offset; i < k; i++, j++) {
            if (value[i] != str.value[j]) {
                return false;
            }
        }
        return true;
    }

    /**
     * @com.intel.drl.spec_ref 
     */
    public boolean equalsIgnoreCase(String anotherString) {
        if ((anotherString == null) || (count != anotherString.count)) {
            return false;
        }
        int j = anotherString.offset;
        int k = offset + count;
        for (int i = offset; i < k; i++, j++) {
            char ch1 = value[i];
            char ch2 = anotherString.value[j];
            if ((ch1 != ch2)
                && (Character.toLowerCase(ch1) != Character.toLowerCase(ch2))
                && (Character.toUpperCase(ch1) != Character.toUpperCase(ch2))) {
                return false;
            }
        }
        return true;
    }

    /**
     * @com.intel.drl.spec_ref 
     */
    public byte[] getBytes() {
        Charset cs = getDefaultCharset();
        CharBuffer cb = CharBuffer.allocate(count);
        cb.put(value, offset, count);
        cb.position(0);
        ByteBuffer bb = cs.encode(cb);
        byte[] b = new byte[bb.limit()];
        bb.get(b);
        return b;
    }

    /**
     * @com.intel.drl.spec_ref 
     */
    public void getBytes(int srcBegin, int srcEnd, byte[] dst, int dstBegin) {
        if (srcBegin < 0) {
            throw new StringIndexOutOfBoundsException(srcBegin);
        }
        if (srcBegin > srcEnd) {
            throw new StringIndexOutOfBoundsException(srcEnd - srcBegin);
        }
        if (srcEnd > count) {
            throw new StringIndexOutOfBoundsException(srcEnd);
        }
        int length = offset + srcEnd;
        for (int i = offset + srcBegin; i < length; i++, dstBegin++) {
            dst[dstBegin] = (byte)value[i];
        }
    }

    /**
     * @com.intel.drl.spec_ref 
     */
    public byte[] getBytes(String charsetName)
        throws UnsupportedEncodingException {
        Charset cs;
        try {
            cs = Charset.forName(charsetName);
        } catch (Exception ex) {
            UnsupportedEncodingException throwEx = new UnsupportedEncodingException();
            throwEx.initCause(ex);
            throw throwEx;
        }
        CharBuffer cb = CharBuffer.allocate(count);
        cb.put(value, offset, count);
        cb.position(0);
        ByteBuffer bb = cs.encode(cb);
        byte[] b = new byte[bb.limit()];
        bb.get(b);
        return b;
    }

    /**
     * @com.intel.drl.spec_ref 
     */
    public void getChars(int srcBegin, int srcEnd, char[] dst, int dstBegin) {
        if (srcBegin < 0) {
            throw new StringIndexOutOfBoundsException(srcBegin);
        }
        if (srcEnd - srcBegin < 0) {
            throw new StringIndexOutOfBoundsException(srcEnd - srcBegin);
        }
        if (srcEnd > count) {
            throw new StringIndexOutOfBoundsException(srcEnd);
        }
        if (dstBegin < 0) {
            throw new StringIndexOutOfBoundsException(dstBegin);
        }
        if (dstBegin + srcEnd - srcBegin > dst.length) {
            throw new StringIndexOutOfBoundsException(dst.length - dstBegin
                - srcEnd + srcBegin);
        }
        System.arraycopy(value, offset + srcBegin, dst, dstBegin, srcEnd
            - srcBegin);
    }

    /**
     * @com.intel.drl.spec_ref 
     */
    public int hashCode() {
        if (hashCode != 0) {
            return hashCode;
        }
        int sum = 0;
        int j = offset + count;
        for (int i = offset; i < j; i++) {
            sum = sum * 31 + value[i];
        }
        return hashCode = sum;
    }

    /**
     * @com.intel.drl.spec_ref 
     */
    public int indexOf(int ch) {
        int j = offset + count;
        for (int i = offset; i < j; i++) {
            if (value[i] == ch) {
                return i - offset;
            }
        }
        return -1;
    }

    /**
     * @com.intel.drl.spec_ref 
     */
    public int indexOf(int ch, int fromIndex) {
        if (fromIndex < 0) {
            fromIndex = 0;
        }
        int j = offset + count;
        for (int i = offset + fromIndex; i < j; i++) {
            if (value[i] == ch) {
                return i - offset;
            }
        }
        return -1;
    }

    /**
     * @com.intel.drl.spec_ref 
     */
    public int indexOf(String str) {
        if (str.count > 0) {
            int len = offset + count - str.count + 1;
            char firstChar = str.value[str.offset];
            for (int i = offset; i < len; i++) {
                if (value[i] == firstChar) {
                    if (strMatches(str, i)) {
                        return i - offset;
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
    public int indexOf(String str, int fromIndex) {
        if (fromIndex < 0) {
            fromIndex = 0;
        }
        if (str.count > 0) {
            fromIndex = fromIndex + offset;
            int len = offset + count - str.count + 1;
            char firstChar = str.value[str.offset];
            for (; fromIndex < len; fromIndex++) {
                if (value[fromIndex] == firstChar) {
                    if (strMatches(str, fromIndex)) {
                        return fromIndex - offset;
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
        public native String intern();

    /**
     * @com.intel.drl.spec_ref 
     */
    public int lastIndexOf(int ch) {
        for (int i = offset + count - 1; i >= offset; i--) {
            if (value[i] == ch) {
                return i - offset;
            }
        }
        return -1;
    }

    /**
     * @com.intel.drl.spec_ref 
     */
    public int lastIndexOf(int ch, int fromIndex) {
        if (fromIndex >= count) {
            fromIndex = count - 1;
        }
        for (int i = offset + fromIndex; i >= offset; i--) {
            if (value[i] == ch) {
                return i - offset;
            }
        }
        return -1;
    }

    /**
     * @com.intel.drl.spec_ref 
     */
    public int lastIndexOf(String str) {
        if (str.count > 0) {
            char firstChar = str.value[str.offset];
            for (int i = offset + count - str.count; i >= offset; i--) {
                if (value[i] == firstChar) {
                    if (strMatches(str, i)) {
                        return i - offset;
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
    public int lastIndexOf(String str, int fromIndex) {
        if (fromIndex > count - str.count) {
            fromIndex = count - str.count;
        }
        if (str.count > 0) {
            fromIndex = fromIndex + offset;
            char firstChar = str.value[str.offset];
            for (; fromIndex >= offset; fromIndex--) {
                if (value[fromIndex] == firstChar) {
                    if (strMatches(str, fromIndex)) {
                        return fromIndex - offset;
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
    public boolean matches(String regex) {
        return Pattern.matches(regex, this);
    }

    /**
     * @com.intel.drl.spec_ref 
     */
    public boolean regionMatches(boolean ignoreCase, int toffset, String other,
        int ooffset, int len) {
        if (ooffset + len > other.count || toffset < 0 || ooffset < 0
            || toffset + len > count) {
            return false;
        }
        toffset = toffset + offset;
        ooffset = ooffset + other.offset;
        int firstLen = toffset + len;
        for (; toffset < firstLen; toffset++, ooffset++) {
            char c1 = value[toffset];
            char c2 = other.value[ooffset];
            if (c1 != c2
                && (!ignoreCase || (Character.toLowerCase(c1) != Character
                    .toLowerCase(c2) && Character.toUpperCase(c1) != Character
                    .toUpperCase(c2)))) {
                return false;
            }
        }
        return true;
    }

    /**
     * @com.intel.drl.spec_ref 
     */
    public boolean regionMatches(int toffset, String other, int ooffset, int len) {
        if (ooffset + len > other.count || toffset < 0 || ooffset < 0
            || toffset + len > count) {
            return false;
        }
        toffset = toffset + offset;
        ooffset = ooffset + other.offset;
        int firstLen = toffset + len;
        for (; toffset < firstLen; toffset++, ooffset++) {
            char c1 = value[toffset];
            char c2 = other.value[ooffset];
            if (c1 != c2) {
                return false;
            }
        }
        return true;
    }

    /**
     * @com.intel.drl.spec_ref 
     */
    public String replace(char oldChar, char newChar) {
        if (oldChar != newChar) {
            int i;
            boolean replaced = false;
            char[] ch = new char[count];
            for (i = 0; i < count; i++) {
                if (value[i + offset] == oldChar) {
                    System.arraycopy(value, offset, ch, 0, count);
                    ch[i] = newChar;
                    replaced = true;
                    break;
                }
            }
            for (++i; i < count; i++) {
                if (ch[i] == oldChar) {
                    ch[i] = newChar;
                }
            }
            if (replaced) {
                return new String(0, count, ch);
            }
        }
        return this;
    }

    /**
     * @com.intel.drl.spec_ref 
     */
    public String replaceAll(String regex, String replacement) {
        return Pattern.compile(regex).matcher(this).replaceAll(replacement);
    }

    /**
     * @com.intel.drl.spec_ref 
     */
    public String replaceFirst(String regex, String replacement) {
        return Pattern.compile(regex).matcher(this).replaceFirst(replacement);
    }

    /**
     * @com.intel.drl.spec_ref 
     */
    public String[] split(String regex) {
        return Pattern.compile(regex).split(this, 0);
    }

    /**
     * @com.intel.drl.spec_ref 
     */
    public String[] split(String regex, int limit) {
        return Pattern.compile(regex).split(this, limit);
    }

    /**
     * @com.intel.drl.spec_ref 
     */
    public boolean startsWith(String prefix) {
        if (prefix.count > count) {
            return false;
        }
        int len = prefix.offset + prefix.count;
        int j = offset;
        for (int i = prefix.offset; i < len; i++, j++) {
            if (value[j] != prefix.value[i]) {
                return false;
            }
        }
        return true;
    }

    /**
     * @com.intel.drl.spec_ref 
     */
    public boolean startsWith(String prefix, int toffset) {
        if (prefix.count > count - toffset || toffset < 0) {
            return false;
        }
        int len = prefix.offset + prefix.count;
        int j = offset + toffset;
        for (int i = prefix.offset; i < len; i++, j++) {
            if (value[j] != prefix.value[i]) {
                return false;
            }
        }
        return true;
    }

    /**
     * @com.intel.drl.spec_ref 
     */
    public CharSequence subSequence(int beginIndex, int endIndex) {
        return substring(beginIndex, endIndex);
    }

    /**
     * @com.intel.drl.spec_ref 
     */
    public String substring(int beginIndex) {
        return substring(beginIndex, count);
    }

    /**
     * @com.intel.drl.spec_ref 
     */
    public String substring(int beginIndex, int endIndex) {
        if (beginIndex < 0) {
            throw new StringIndexOutOfBoundsException(beginIndex);
        }
        if (endIndex > count) {
            throw new StringIndexOutOfBoundsException(endIndex);
        }
        if (beginIndex > endIndex) {
            throw new StringIndexOutOfBoundsException(endIndex - beginIndex);
        }
        return new String(offset + beginIndex, endIndex - beginIndex, value);
    }

    /**
     * @com.intel.drl.spec_ref 
     */
    public char[] toCharArray() {
        char[] chArray = new char[count];
        System.arraycopy(value, offset, chArray, 0, count);
        return chArray;
    }

    /**
     * @com.intel.drl.spec_ref 
     */
    public String toLowerCase() {
        return toLowerCase(Locale.getDefault());
    }

    /**
     * @com.intel.drl.spec_ref 
     */
    public String toLowerCase(Locale locale) {
        boolean tr = "tr".equals(locale.getLanguage());
        boolean shouldLC = false;
        int j = offset + count;
        if (!tr) {
            for (int i = offset; i < j; i++) {
                if (value[i] != Character.toLowerCase(value[i])) {
                    shouldLC = true;
                    break;
                }
            }
        } else {
            for (int i = offset; i < j; i++) {
                char ch = value[i];
                if (ch == '\u0049' || ch != Character.toLowerCase(ch)) {
                    shouldLC = true;
                    break;
                }
            }
        }
        if (!shouldLC) {
            return this;
        }
        char[] ch = new char[count];
        int k = 0;
        if (!tr) {
            for (int i = offset; i < j; i++, k++) {
                ch[k] = Character.toLowerCase(value[i]);
            }
            return new String(0, count, ch);
        } else {
            for (int i = offset; i < j; i++, k++) {
                char ch1 = value[i];
                ch[k] = (tr && ch1 == '\u0049') ? '\u0131' : Character
                    .toLowerCase(ch1);
            }
            return new String(0, count, ch);
        }
    }

    /**
     * @com.intel.drl.spec_ref 
     */
    public String toString() {
        return this;
    }

    /**
     * @com.intel.drl.spec_ref 
     */
    public String toUpperCase() {
        return toUpperCase(Locale.getDefault());
    }

    /**
     * @com.intel.drl.spec_ref 
     */
    public String toUpperCase(Locale locale) {
        int index = 0;
        int indSP;
        int length = offset + count;
        char ch;
        char[] charArray = new char[count * 3];
        if (!"tr".equals(locale.getLanguage())) {
            for (int i = offset; i < length; i++) {
                ch = value[i];
                if (ch < '\u00df') {
                    charArray[index++] = Character.toUpperCase(ch);
                } else if (ch == '\u00df') {
                    charArray[index++] = '\u0053';
                    charArray[index++] = '\u0053';
                } else if (ch == '\u0149') {
                    charArray[index++] = '\u02bc';
                    charArray[index++] = '\u004e';
                } else if (ch == '\u01f0') {
                    charArray[index++] = '\u004a';
                    charArray[index++] = '\u030c';
                } else if (ch == '\u0390') {
                    charArray[index++] = '\u0399';
                    charArray[index++] = '\u0308';
                    charArray[index++] = '\u0301';
                } else if (ch == '\u03b0') {
                    charArray[index++] = '\u03a5';
                    charArray[index++] = '\u0308';
                    charArray[index++] = '\u0301';
                } else if (ch == '\u0587') {
                    charArray[index++] = '\u0535';
                    charArray[index++] = '\u0552';
                } else if (ch >= '\u1e96' && ch <= '\u1e9a') {
                    indSP = (ch - '\u1e96') * 2;
                    charArray[index++] = charUpperSpecial[indSP++];
                    charArray[index++] = charUpperSpecial[indSP];
                } else if (ch >= '\u1f50' && ch <= '\u1ffc') {
                    indSP = indexUp[ch - '\u1f50'] * 3;
                    if (indSP == 0) {
                        charArray[index++] = Character.toUpperCase(ch);
                    } else {
                        charArray[index++] = charUpperSpecial[indSP++];
                        charArray[index++] = charUpperSpecial[indSP++];
                        if (charUpperSpecial[indSP] > 0) {
                            charArray[index++] = charUpperSpecial[indSP];
                        }
                    }
                } else if (ch >= '\ufb00' && ch <= '\ufb17') {
                    indSP = indexUp1[ch - '\ufb00'] * 3;
                    if (indSP == 0) {
                        charArray[index++] = Character.toUpperCase(ch);
                    } else {
                        charArray[index++] = charUpperSpecial[indSP++];
                        charArray[index++] = charUpperSpecial[indSP++];
                        if (charUpperSpecial[indSP] > 0) {
                            charArray[index++] = charUpperSpecial[indSP];
                        }
                    }
                } else {
                    charArray[index++] = Character.toUpperCase(ch);
                }
            }
        } else {
            for (int i = offset; i < length; i++) {
                ch = value[i];
                if (ch < '\u00df') {
                    charArray[index++] = ch == '\u0069' ? '\u0130' : Character
                        .toUpperCase(ch);
                } else if (ch == '\u00df') {
                    charArray[index++] = '\u0053';
                    charArray[index++] = '\u0053';
                } else if (ch == '\u0149') {
                    charArray[index++] = '\u02bc';
                    charArray[index++] = '\u004e';
                } else if (ch == '\u01f0') {
                    charArray[index++] = '\u004a';
                    charArray[index++] = '\u030c';
                } else if (ch == '\u0390') {
                    charArray[index++] = '\u0399';
                    charArray[index++] = '\u0308';
                    charArray[index++] = '\u0301';
                } else if (ch == '\u03b0') {
                    charArray[index++] = '\u03a5';
                    charArray[index++] = '\u0308';
                    charArray[index++] = '\u0301';
                } else if (ch == '\u0587') {
                    charArray[index++] = '\u0535';
                    charArray[index++] = '\u0552';
                } else if (ch >= '\u1e96' && ch <= '\u1e9a') {
                    indSP = (ch - '\u1e96') * 2;
                    charArray[index++] = charUpperSpecial[indSP++];
                    charArray[index++] = charUpperSpecial[indSP];
                } else if (ch >= '\u1f50' && ch <= '\u1ffc') {
                    indSP = indexUp[ch - '\u1f50'] * 3;
                    if (indSP == 0) {
                        charArray[index++] = ch == '\u0069' ? '\u0130'
                            : Character.toUpperCase(ch);
                    } else {
                        charArray[index++] = charUpperSpecial[indSP++];
                        charArray[index++] = charUpperSpecial[indSP++];
                        if (charUpperSpecial[indSP] > 0) {
                            charArray[index++] = charUpperSpecial[indSP];
                        }
                    }
                } else if (ch >= '\ufb00' && ch <= '\ufb17') {
                    indSP = indexUp1[ch - '\ufb00'] * 3;
                    if (indSP == 0) {
                        charArray[index++] = ch == '\u0069' ? '\u0130'
                            : Character.toUpperCase(ch);
                    } else {
                        charArray[index++] = charUpperSpecial[indSP++];
                        charArray[index++] = charUpperSpecial[indSP++];
                        if (charUpperSpecial[indSP] > 0) {
                            charArray[index++] = charUpperSpecial[indSP];
                        }
                    }
                } else {
                    charArray[index++] = ch == '\u0069' ? '\u0130' : Character
                        .toUpperCase(ch);
                }
            }
        }
        char[] array = new char[index];
        System.arraycopy(charArray, 0, array, 0, index);
        return new String(0, index, array);
    }

    /**
     * @com.intel.drl.spec_ref 
     */
    public String trim() {
        if (count == 0
            || (value[offset] > '\u0020' && value[offset + count - 1] > '\u0020')) {
            return this;
        }
        int begin = offset;
        int end = offset + count;
        do
            if (begin == end) {
                return new String();
            }
        while (value[begin++] <= '\u0020');
        while (value[--end] <= '\u0020');
        return substring(begin - offset - 1, end - offset + 1);
    }

    private Charset getDefaultCharset() {
        if (defaultCharset == null) {
            defaultCharset = Charset.forName(System
                .getProperty("file.encoding"));
        }
        return defaultCharset;
    }

    private boolean strMatches(String str, int index) {
        int len = str.offset + str.count;
        for (int i = str.offset; i < len; i++, index++) {
            if (value[index] != str.value[i]) {
                return false;
            }
        }
        return true;
    }

    private static final class CaseInsensitiveComparator implements Comparator,
        Serializable {

                static final long serialVersionUID = 8575799808933029326L;

                CaseInsensitiveComparator() {
        }

        public int compare(Object obj0, Object obj1) {
            return ((String)obj0).compareToIgnoreCase((String)obj1);
        }
    }

}