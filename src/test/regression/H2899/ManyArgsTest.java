package org.apache.harmony.drlvm.tests.regression.h2899;

import junit.framework.TestCase;

public class ManyArgsTest extends TestCase {
    static {
        System.loadLibrary("ManyArgs");
    }
    
    public native Object native_method(
        Object arg1,
        Object arg2,
        Object arg3,
        Object arg4,
        Object arg5,
        Object arg6,
        Object arg7,
        Object arg8,
        Object arg9,
        Object arg10,
        Object arg11,
        Object arg12,
        Object arg13,
        Object arg14,
        Object arg15,
        Object arg16,
        Object arg17,
        Object arg18,
        Object arg19,
        Object arg20,
        Object arg21,
        Object arg22,
        Object arg23,
        Object arg24,
        Object arg25,
        Object arg26,
        Object arg27,
        Object arg28,
        Object arg29,
        Object arg30,
        Object arg31,
        Object arg32,
        Object arg33,
        Object arg34,
        Object arg35,
        Object arg36,
        Object arg37,
        Object arg38,
        Object arg39,
        Object arg40,
        Object arg41,
        Object arg42,
        Object arg43,
        Object arg44,
        Object arg45,
        Object arg46,
        Object arg47,
        Object arg48,
        Object arg49,
        Object arg50,
        Object arg51,
        Object arg52,
        Object arg53,
        Object arg54,
        Object arg55,
        Object arg56,
        Object arg57,
        Object arg58,
        Object arg59,
        Object arg60,
        Object arg61,
        Object arg62,
        Object arg63,
        Object arg64,
        Object arg65,
        Object arg66,
        Object arg67,
        Object arg68,
        Object arg69,
        Object arg70,
        Object arg71,
        Object arg72,
        Object arg73,
        Object arg74,
        Object arg75,
        Object arg76,
        Object arg77,
        Object arg78,
        Object arg79,
        Object arg80,
        Object arg81,
        Object arg82,
        Object arg83,
        Object arg84,
        Object arg85,
        Object arg86,
        Object arg87,
        Object arg88,
        Object arg89,
        Object arg90,
        Object arg91,
        Object arg92,
        Object arg93,
        Object arg94,
        Object arg95,
        Object arg96,
        Object arg97,
        Object arg98,
        Object arg99,
        Object arg100,
        Object arg101,
        Object arg102,
        Object arg103,
        Object arg104,
        Object arg105,
        Object arg106,
        Object arg107,
        Object arg108,
        Object arg109,
        Object arg110,
        Object arg111,
        Object arg112,
        Object arg113,
        Object arg114,
        Object arg115,
        Object arg116,
        Object arg117,
        Object arg118,
        Object arg119,
        Object arg120,
        Object arg121,
        Object arg122,
        Object arg123,
        Object arg124,
        Object arg125,
        Object arg126,
        Object arg127,
        Object arg128,
        Object arg129,
        Object arg130,
        Object arg131,
        Object arg132,
        Object arg133,
        Object arg134,
        Object arg135,
        Object arg136,
        Object arg137,
        Object arg138,
        Object arg139,
        Object arg140,
        Object arg141,
        Object arg142,
        Object arg143,
        Object arg144,
        Object arg145,
        Object arg146,
        Object arg147,
        Object arg148,
        Object arg149,
        Object arg150,
        Object arg151,
        Object arg152,
        Object arg153,
        Object arg154,
        Object arg155,
        Object arg156,
        Object arg157,
        Object arg158,
        Object arg159,
        Object arg160,
        Object arg161,
        Object arg162,
        Object arg163,
        Object arg164,
        Object arg165,
        Object arg166,
        Object arg167,
        Object arg168,
        Object arg169,
        Object arg170,
        Object arg171,
        Object arg172,
        Object arg173,
        Object arg174,
        Object arg175,
        Object arg176,
        Object arg177,
        Object arg178,
        Object arg179,
        Object arg180,
        Object arg181,
        Object arg182,
        Object arg183,
        Object arg184,
        Object arg185,
        Object arg186,
        Object arg187,
        Object arg188,
        Object arg189,
        Object arg190,
        Object arg191,
        Object arg192,
        Object arg193,
        Object arg194,
        Object arg195,
        Object arg196,
        Object arg197,
        Object arg198,
        Object arg199,
        Object arg200,
        Object arg201,
        Object arg202,
        Object arg203,
        Object arg204,
        Object arg205,
        Object arg206,
        Object arg207,
        Object arg208,
        Object arg209,
        Object arg210,
        Object arg211,
        Object arg212,
        Object arg213,
        Object arg214,
        Object arg215,
        Object arg216,
        Object arg217,
        Object arg218,
        Object arg219,
        Object arg220,
        Object arg221,
        Object arg222,
        Object arg223,
        Object arg224,
        Object arg225,
        Object arg226,
        Object arg227,
        Object arg228,
        Object arg229,
        Object arg230,
        Object arg231,
        Object arg232,
        Object arg233,
        Object arg234,
        Object arg235,
        Object arg236,
        Object arg237,
        Object arg238,
        Object arg239,
        Object arg240,
        Object arg241,
        Object arg242,
        Object arg243,
        Object arg244,
        Object arg245,
        Object arg246,
        Object arg247,
        Object arg248,
        Object arg249,
        Object arg250,
        Object arg251,
        Object arg252,
        Object arg253,
        Object arg254);

    public void testNativeCall() {
        Object ret = native_method(null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null);
        assertNull(ret);
    }
}

