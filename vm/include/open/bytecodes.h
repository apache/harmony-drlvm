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
 * @author Pavel Pervov, Pavel Rebriy
 * @version $Revision: 1.1.2.1.4.3 $
 */  
/** 
 * Enum of bytecode opcodes.
 */
enum JavaByteCodes {
    OPCODE_NOP = 0,        /* 0x00 */
    OPCODE_ACONST_NULL,    /* 0x01 */
    OPCODE_ICONST_M1,      /* 0x02 */
    OPCODE_ICONST_0,       /* 0x03 */
    OPCODE_ICONST_1,       /* 0x04 */
    OPCODE_ICONST_2,       /* 0x05 */
    OPCODE_ICONST_3,       /* 0x06 */
    OPCODE_ICONST_4,       /* 0x07 */
    OPCODE_ICONST_5,       /* 0x08 */
    OPCODE_LCONST_0,       /* 0x09 */
    OPCODE_LCONST_1,       /* 0x0a */
    OPCODE_FCONST_0,       /* 0x0b */
    OPCODE_FCONST_1,       /* 0x0c */
    OPCODE_FCONST_2,       /* 0x0d */
    OPCODE_DCONST_0,       /* 0x0e */
    OPCODE_DCONST_1,       /* 0x0f */
    OPCODE_BIPUSH,         /* 0x10 + s1 */
    OPCODE_SIPUSH,         /* 0x11 + s2 */
    OPCODE_LDC,            /* 0x12 + u1 */
    OPCODE_LDC_W,          /* 0x13 + u2 */
    OPCODE_LDC2_W,         /* 0x14 + u2 */
    OPCODE_ILOAD,          /* 0x15 + u1|u2 */
    OPCODE_LLOAD,          /* 0x16 + u1|u2 */
    OPCODE_FLOAD,          /* 0x17 + u1|u2 */
    OPCODE_DLOAD,          /* 0x18 + u1|u2 */
    OPCODE_ALOAD,          /* 0x19 + u1|u2 */
    OPCODE_ILOAD_0,        /* 0x1a */
    OPCODE_ILOAD_1,        /* 0x1b */
    OPCODE_ILOAD_2,        /* 0x1c */
    OPCODE_ILOAD_3,        /* 0x1d */
    OPCODE_LLOAD_0,        /* 0x1e */
    OPCODE_LLOAD_1,        /* 0x1f */
    OPCODE_LLOAD_2,        /* 0x20 */
    OPCODE_LLOAD_3,        /* 0x21 */
    OPCODE_FLOAD_0,        /* 0x22 */
    OPCODE_FLOAD_1,        /* 0x23 */
    OPCODE_FLOAD_2,        /* 0x24 */
    OPCODE_FLOAD_3,        /* 0x25 */
    OPCODE_DLOAD_0,        /* 0x26 */
    OPCODE_DLOAD_1,        /* 0x27 */
    OPCODE_DLOAD_2,        /* 0x28 */
    OPCODE_DLOAD_3,        /* 0x29 */
    OPCODE_ALOAD_0,        /* 0x2a */
    OPCODE_ALOAD_1,        /* 0x2b */
    OPCODE_ALOAD_2,        /* 0x2c */
    OPCODE_ALOAD_3,        /* 0x2d */
    OPCODE_IALOAD,         /* 0x2e */
    OPCODE_LALOAD,         /* 0x2f */
    OPCODE_FALOAD,         /* 0x30 */
    OPCODE_DALOAD,         /* 0x31 */
    OPCODE_AALOAD,         /* 0x32 */
    OPCODE_BALOAD,         /* 0x33 */
    OPCODE_CALOAD,         /* 0x34 */
    OPCODE_SALOAD,         /* 0x35 */
    OPCODE_ISTORE,         /* 0x36 + u1|u2 */
    OPCODE_LSTORE,         /* 0x37 + u1|u2 */
    OPCODE_FSTORE,         /* 0x38 + u1|u2 */
    OPCODE_DSTORE,         /* 0x39 + u1|u2 */
    OPCODE_ASTORE,         /* 0x3a + u1|u2 */
    OPCODE_ISTORE_0,       /* 0x3b */
    OPCODE_ISTORE_1,       /* 0x3c */
    OPCODE_ISTORE_2,       /* 0x3d */
    OPCODE_ISTORE_3,       /* 0x3e */
    OPCODE_LSTORE_0,       /* 0x3f */
    OPCODE_LSTORE_1,       /* 0x40 */
    OPCODE_LSTORE_2,       /* 0x41 */
    OPCODE_LSTORE_3,       /* 0x42 */
    OPCODE_FSTORE_0,       /* 0x43 */
    OPCODE_FSTORE_1,       /* 0x44 */
    OPCODE_FSTORE_2,       /* 0x45 */
    OPCODE_FSTORE_3,       /* 0x46 */
    OPCODE_DSTORE_0,       /* 0x47 */
    OPCODE_DSTORE_1,       /* 0x48 */
    OPCODE_DSTORE_2,       /* 0x49 */
    OPCODE_DSTORE_3,       /* 0x4a */
    OPCODE_ASTORE_0,       /* 0x4b */
    OPCODE_ASTORE_1,       /* 0x4c */
    OPCODE_ASTORE_2,       /* 0x4d */
    OPCODE_ASTORE_3,       /* 0x4e */
    OPCODE_IASTORE,        /* 0x4f */
    OPCODE_LASTORE,        /* 0x50 */
    OPCODE_FASTORE,        /* 0x51 */
    OPCODE_DASTORE,        /* 0x52 */
    OPCODE_AASTORE,        /* 0x53 */
    OPCODE_BASTORE,        /* 0x54 */
    OPCODE_CASTORE,        /* 0x55 */
    OPCODE_SASTORE,        /* 0x56 */
    OPCODE_POP,            /* 0x57 */
    OPCODE_POP2,           /* 0x58 */
    OPCODE_DUP,            /* 0x59 */
    OPCODE_DUP_X1,         /* 0x5a */
    OPCODE_DUP_X2,         /* 0x5b */
    OPCODE_DUP2,           /* 0x5c */
    OPCODE_DUP2_X1,        /* 0x5d */
    OPCODE_DUP2_X2,        /* 0x5e */
    OPCODE_SWAP,           /* 0x5f */
    OPCODE_IADD,           /* 0x60 */
    OPCODE_LADD,           /* 0x61 */
    OPCODE_FADD,           /* 0x62 */
    OPCODE_DADD,           /* 0x63 */
    OPCODE_ISUB,           /* 0x64 */
    OPCODE_LSUB,           /* 0x65 */
    OPCODE_FSUB,           /* 0x66 */
    OPCODE_DSUB,           /* 0x67 */
    OPCODE_IMUL,           /* 0x68 */
    OPCODE_LMUL,           /* 0x69 */
    OPCODE_FMUL,           /* 0x6a */
    OPCODE_DMUL,           /* 0x6b */
    OPCODE_IDIV,           /* 0x6c */
    OPCODE_LDIV,           /* 0x6d */
    OPCODE_FDIV,           /* 0x6e */
    OPCODE_DDIV,           /* 0x6f */
    OPCODE_IREM,           /* 0x70 */
    OPCODE_LREM,           /* 0x71 */
    OPCODE_FREM,           /* 0x72 */
    OPCODE_DREM,           /* 0x73 */
    OPCODE_INEG,           /* 0x74 */
    OPCODE_LNEG,           /* 0x75 */
    OPCODE_FNEG,           /* 0x76 */
    OPCODE_DNEG,           /* 0x77 */
    OPCODE_ISHL,           /* 0x78 */
    OPCODE_LSHL,           /* 0x79 */
    OPCODE_ISHR,           /* 0x7a */
    OPCODE_LSHR,           /* 0x7b */
    OPCODE_IUSHR,          /* 0x7c */
    OPCODE_LUSHR,          /* 0x7d */
    OPCODE_IAND,           /* 0x7e */
    OPCODE_LAND,           /* 0x7f */
    OPCODE_IOR,            /* 0x80 */
    OPCODE_LOR,            /* 0x81 */
    OPCODE_IXOR,           /* 0x82 */
    OPCODE_LXOR,           /* 0x83 */
    OPCODE_IINC,           /* 0x84 + u1|u2 + s1|s2 */
    OPCODE_I2L,            /* 0x85 */
    OPCODE_I2F,            /* 0x86 */
    OPCODE_I2D,            /* 0x87 */
    OPCODE_L2I,            /* 0x88 */
    OPCODE_L2F,            /* 0x89 */
    OPCODE_L2D,            /* 0x8a */
    OPCODE_F2I,            /* 0x8b */
    OPCODE_F2L,            /* 0x8c */
    OPCODE_F2D,            /* 0x8d */
    OPCODE_D2I,            /* 0x8e */
    OPCODE_D2L,            /* 0x8f */
    OPCODE_D2F,            /* 0x90 */
    OPCODE_I2B,            /* 0x91 */
    OPCODE_I2C,            /* 0x92 */
    OPCODE_I2S,            /* 0x93 */
    OPCODE_LCMP,           /* 0x94 */
    OPCODE_FCMPL,          /* 0x95 */
    OPCODE_FCMPG,          /* 0x96 */
    OPCODE_DCMPL,          /* 0x97 */
    OPCODE_DCMPG,          /* 0x98 */
    OPCODE_IFEQ,           /* 0x99 + s2 (c) */
    OPCODE_IFNE,           /* 0x9a + s2 (c) */
    OPCODE_IFLT,           /* 0x9b + s2 (c) */
    OPCODE_IFGE,           /* 0x9c + s2 (c) */
    OPCODE_IFGT,           /* 0x9d + s2 (c) */
    OPCODE_IFLE,           /* 0x9e + s2 (c) */
    OPCODE_IF_ICMPEQ,      /* 0x9f + s2 (c) */
    OPCODE_IF_ICMPNE,      /* 0xa0 + s2 (c) */
    OPCODE_IF_ICMPLT,      /* 0xa1 + s2 (c) */
    OPCODE_IF_ICMPGE,      /* 0xa2 + s2 (c) */
    OPCODE_IF_ICMPGT,      /* 0xa3 + s2 (c) */
    OPCODE_IF_ICMPLE,      /* 0xa4 + s2 (c) */
    OPCODE_IF_ACMPEQ,      /* 0xa5 + s2 (c) */
    OPCODE_IF_ACMPNE,      /* 0xa6 + s2 (c) */
    OPCODE_GOTO,           /* 0xa7 + s2 (c) */
    OPCODE_JSR,            /* 0xa8 + s2 (c) */
    OPCODE_RET,            /* 0xa9 + u1|u2 (c) */
    OPCODE_TABLESWITCH,    /* 0xaa + pad + s4 * (3 + N) (c) */
    OPCODE_LOOKUPSWITCH,   /* 0xab + pad + s4 * 2 * (N + 1) (c) */
    OPCODE_IRETURN,        /* 0xac (c) */
    OPCODE_LRETURN,        /* 0xad (c) */
    OPCODE_FRETURN,        /* 0xae (c) */
    OPCODE_DRETURN,        /* 0xaf (c) */
    OPCODE_ARETURN,        /* 0xb0 (c) */
    OPCODE_RETURN,         /* 0xb1 (c) */
    OPCODE_GETSTATIC,      /* 0xb2 + u2 */
    OPCODE_PUTSTATIC,      /* 0xb3 + u2 */
    OPCODE_GETFIELD,       /* 0xb4 + u2 */
    OPCODE_PUTFIELD,       /* 0xb5 + u2 */
    OPCODE_INVOKEVIRTUAL,  /* 0xb6 + u2 */
    OPCODE_INVOKESPECIAL,  /* 0xb7 + u2 */
    OPCODE_INVOKESTATIC,   /* 0xb8 + u2 */
    OPCODE_INVOKEINTERFACE,/* 0xb9 + u2 + u1 + u1 */
    _OPCODE_UNDEFINED,     /* 0xba */
    OPCODE_NEW,            /* 0xbb + u2 */
    OPCODE_NEWARRAY,       /* 0xbc + u1 */
    OPCODE_ANEWARRAY,      /* 0xbd + u1 */
    OPCODE_ARRAYLENGTH,    /* 0xbe */
    OPCODE_ATHROW,         /* 0xbf (c) */
    OPCODE_CHECKCAST,      /* 0xc0 + u2 */
    OPCODE_INSTANCEOF,     /* 0xc1 + u2 */
    OPCODE_MONITORENTER,   /* 0xc2 */
    OPCODE_MONITOREXIT,    /* 0xc3 */
    OPCODE_WIDE,           /* 0xc4 */
    OPCODE_MULTIANEWARRAY, /* 0xc5 + u2 + u1 */
    OPCODE_IFNULL,         /* 0xc6 + s2 (c) */
    OPCODE_IFNONNULL,      /* 0xc7 + s2 (c) */
    OPCODE_GOTO_W,         /* 0xc8 + s4 (c) */
    OPCODE_JSR_W,          /* 0xc9 + s4 (c) */

    OPCODE_COUNT,          /* number of bytecodes */

    // extended bytecodes
    OPCODE_BREAKPOINT = OPCODE_COUNT    /* 0xca */
#ifdef FAST_BYTECODES
    ,
    OPCODE_FAST_GETFIELD_REF, /* 0xcb */
    OPCODE_FAST_GETFIELD_INT, /* 0xcc */
#endif /* FAST_BYTECODES */
};
