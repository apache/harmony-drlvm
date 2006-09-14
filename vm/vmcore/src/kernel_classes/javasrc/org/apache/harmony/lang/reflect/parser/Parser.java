/*
 *  Copyright 2005 The Apache Software Foundation or its licensors, as applicable.
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

package org.apache.harmony.lang.reflect.parser;

import java.lang.reflect.GenericSignatureFormatError;

/**
 * @author Serguei S. Zapreyev
 * @version $Revision: 1.1.2.1 $
 */
public class Parser {

    public static enum SignatureKind {
        FIELD_SIGNATURE(2),
        METHOD_SIGNATURE(3),
        CONSTRUCTOR_SIGNATURE(4),
        CLASS_SIGNATURE(1);
        SignatureKind(int value) { this.value = value; }
        private final int value;
        public int value() { return value; }
    };
    public static InterimGenericDeclaration parseSignature(String signature, SignatureKind kind, java.lang.reflect.GenericDeclaration startPoint) throws GenericSignatureFormatError {
        return SignatureParser.parseSignature(signature, kind.value());
    }
}