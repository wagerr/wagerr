// Copyright (c) 2012-2018 The Bitcoin Core developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "key.h"

#include "base58.h"
#include "script/script.h"
#include "uint256.h"
#include "util.h"
#include "utilstrencodings.h"

#include <string>
#include <vector>

#include <boost/test/unit_test.hpp>

using namespace std;

static const string strSecret1     ("7h7XgpvrwDeK97GDe2aAytDbj2ddmWTMthT3vFX54CkeshRc5vN");
static const string strSecret2     ("7gLw2CxHKrVfSVq2MXYpE8tbSJbPpAyXBcNcy9kNGCHArsGLjVm");
static const string strSecret1C    ("WY3pSnYjQJoh4neAnRwR454cGWChHT1MDBXyt35iu8sgAiQ18Y3Q");
static const string strSecret2C    ("WUeySKm14DTWfYExybqRSypizgeohD2UDq8RkqcQr4nNWoYgjtgw");
static const CBitcoinAddress addr1 ("WgBT2BJAYFJVS6Z2nhLTsn3Dn5UP2bAGxc");
static const CBitcoinAddress addr2 ("Wk5KNqBJusWwe12PzDYdu7o7HmSwm8AhvM");
static const CBitcoinAddress addr1C("WQb9hYVWrezLyTMLFBuQRpT4MEM2wNm5px");
static const CBitcoinAddress addr2C("WimyRMKVz1a89mKR8aumXC6Qd51zTNfNZy");


static const string strAddressBad("Xta1praZQjyELweyMByXyiREw1ZRsjXzVP");

#ifdef KEY_TESTS_DUMPINFO
void dumpKeyInfo(CKey keyIn)
{
    CKey key;
    key.SetPrivKey(keyIn.GetPrivKey(), true);

    CKey keyC;
    keyC.SetPrivKey(keyIn.GetPrivKey(), true);

    CPubKey pubKey = key.GetPubKey();
    CPubKey pubKeyC = keyC.GetPubKey();

    printf("    * secret (base58): %s\n", CBitcoinSecret(key).ToString().c_str());
    printf("    * address (base58): %s\n", CBitcoinAddress(CTxDestination(pubKey.GetID())).ToString().c_str());

    printf("    * secret (C) (base58): %s\n", CBitcoinSecret(keyC).ToString().c_str());
    printf("    * address (C) (base58): %s\n", CBitcoinAddress(CTxDestination(pubKeyC.GetID())).ToString().c_str());
}
#endif // KEY_TESTS_DUMPINFO


BOOST_AUTO_TEST_SUITE(key_tests)

BOOST_AUTO_TEST_CASE(key_test1)
{
    CBitcoinSecret bsecret1, bsecret2, bsecret1C, bsecret2C, baddress1;
    BOOST_CHECK( bsecret1.SetString (strSecret1));
    BOOST_CHECK( bsecret2.SetString (strSecret2));
    BOOST_CHECK( bsecret1C.SetString(strSecret1C));
    BOOST_CHECK( bsecret2C.SetString(strSecret2C));
    BOOST_CHECK(!baddress1.SetString(strAddressBad));

    CKey key1  = bsecret1.GetKey();
    BOOST_CHECK(key1.IsCompressed() == false);
    CKey key2  = bsecret2.GetKey();
    BOOST_CHECK(key2.IsCompressed() == false);
    CKey key1C = bsecret1C.GetKey();
    BOOST_CHECK(key1C.IsCompressed() == true);
    CKey key2C = bsecret2C.GetKey();
    BOOST_CHECK(key2C.IsCompressed() == true);

    CPubKey pubkey1  = key1. GetPubKey();
    CPubKey pubkey2  = key2. GetPubKey();
    CPubKey pubkey1C = key1C.GetPubKey();
    CPubKey pubkey2C = key2C.GetPubKey();

    BOOST_CHECK(key1.VerifyPubKey(pubkey1));
    BOOST_CHECK(!key1.VerifyPubKey(pubkey1C));
    BOOST_CHECK(!key1.VerifyPubKey(pubkey2));
    BOOST_CHECK(!key1.VerifyPubKey(pubkey2C));

    BOOST_CHECK(!key1C.VerifyPubKey(pubkey1));
    BOOST_CHECK(key1C.VerifyPubKey(pubkey1C));
    BOOST_CHECK(!key1C.VerifyPubKey(pubkey2));
    BOOST_CHECK(!key1C.VerifyPubKey(pubkey2C));

    BOOST_CHECK(!key2.VerifyPubKey(pubkey1));
    BOOST_CHECK(!key2.VerifyPubKey(pubkey1C));
    BOOST_CHECK(key2.VerifyPubKey(pubkey2));
    BOOST_CHECK(!key2.VerifyPubKey(pubkey2C));

    BOOST_CHECK(!key2C.VerifyPubKey(pubkey1));
    BOOST_CHECK(!key2C.VerifyPubKey(pubkey1C));
    BOOST_CHECK(!key2C.VerifyPubKey(pubkey2));
    BOOST_CHECK(key2C.VerifyPubKey(pubkey2C));

    BOOST_CHECK(addr1.Get()  == CTxDestination(pubkey1.GetID()));
    BOOST_CHECK(addr2.Get()  == CTxDestination(pubkey2.GetID()));
    BOOST_CHECK(addr1C.Get() == CTxDestination(pubkey1C.GetID()));
    BOOST_CHECK(addr2C.Get() == CTxDestination(pubkey2C.GetID()));

    for (int n=0; n<16; n++)
    {
        string strMsg = strprintf("Very secret message %i: 11", n);
        uint256 hashMsg = Hash(strMsg.begin(), strMsg.end());

        // normal signatures

        vector<unsigned char> sign1, sign2, sign1C, sign2C;

        BOOST_CHECK(key1.Sign (hashMsg, sign1));
        BOOST_CHECK(key2.Sign (hashMsg, sign2));
        BOOST_CHECK(key1C.Sign(hashMsg, sign1C));
        BOOST_CHECK(key2C.Sign(hashMsg, sign2C));

        BOOST_CHECK( pubkey1.Verify(hashMsg, sign1));
        BOOST_CHECK(!pubkey1.Verify(hashMsg, sign2));
        BOOST_CHECK( pubkey1.Verify(hashMsg, sign1C));
        BOOST_CHECK(!pubkey1.Verify(hashMsg, sign2C));

        BOOST_CHECK(!pubkey2.Verify(hashMsg, sign1));
        BOOST_CHECK( pubkey2.Verify(hashMsg, sign2));
        BOOST_CHECK(!pubkey2.Verify(hashMsg, sign1C));
        BOOST_CHECK( pubkey2.Verify(hashMsg, sign2C));

        BOOST_CHECK( pubkey1C.Verify(hashMsg, sign1));
        BOOST_CHECK(!pubkey1C.Verify(hashMsg, sign2));
        BOOST_CHECK( pubkey1C.Verify(hashMsg, sign1C));
        BOOST_CHECK(!pubkey1C.Verify(hashMsg, sign2C));

        BOOST_CHECK(!pubkey2C.Verify(hashMsg, sign1));
        BOOST_CHECK( pubkey2C.Verify(hashMsg, sign2));
        BOOST_CHECK(!pubkey2C.Verify(hashMsg, sign1C));
        BOOST_CHECK( pubkey2C.Verify(hashMsg, sign2C));

        // compact signatures (with key recovery)

        vector<unsigned char> csign1, csign2, csign1C, csign2C;

        BOOST_CHECK(key1.SignCompact (hashMsg, csign1));
        BOOST_CHECK(key2.SignCompact (hashMsg, csign2));
        BOOST_CHECK(key1C.SignCompact(hashMsg, csign1C));
        BOOST_CHECK(key2C.SignCompact(hashMsg, csign2C));

        CPubKey rkey1, rkey2, rkey1C, rkey2C;

        BOOST_CHECK(rkey1.RecoverCompact (hashMsg, csign1));
        BOOST_CHECK(rkey2.RecoverCompact (hashMsg, csign2));
        BOOST_CHECK(rkey1C.RecoverCompact(hashMsg, csign1C));
        BOOST_CHECK(rkey2C.RecoverCompact(hashMsg, csign2C));

        BOOST_CHECK(rkey1  == pubkey1);
        BOOST_CHECK(rkey2  == pubkey2);
        BOOST_CHECK(rkey1C == pubkey1C);
        BOOST_CHECK(rkey2C == pubkey2C);
    }

    // test deterministic signing

    std::vector<unsigned char> detsig, detsigc;
    string strMsg = "Very deterministic message";
    uint256 hashMsg = Hash(strMsg.begin(), strMsg.end());
    BOOST_CHECK(key1.Sign(hashMsg, detsig));
    BOOST_CHECK(key1C.Sign(hashMsg, detsigc));
    BOOST_CHECK(detsig == detsigc);
    BOOST_CHECK(detsig == ParseHex("3044022035979c9d3680ffc13d6c137e4c1058549268073868da6f1ac2cde99dc6c4fd1a02206499e3daabc050539b1c78d66183fb25b7c47759622c4870a0c0cb71d1d7db93"));
    BOOST_CHECK(key2.Sign(hashMsg, detsig));
    BOOST_CHECK(key2C.Sign(hashMsg, detsigc));
    BOOST_CHECK(detsig == detsigc);
    BOOST_CHECK(detsig == ParseHex("3045022100bb85cceb64ad65a586ecd14fca2c4a3d572e5a4fe619fa2bd4e661d902e581c902202dec07eea5b8c7ceadf558aebd04bdaebbf05af985fb6547f41e124db7b541f9"));
    BOOST_CHECK(key1.SignCompact(hashMsg, detsig));
    BOOST_CHECK(key1C.SignCompact(hashMsg, detsigc));
    BOOST_CHECK(detsig == ParseHex("1b35979c9d3680ffc13d6c137e4c1058549268073868da6f1ac2cde99dc6c4fd1a6499e3daabc050539b1c78d66183fb25b7c47759622c4870a0c0cb71d1d7db93"));
    BOOST_CHECK(detsigc == ParseHex("1f35979c9d3680ffc13d6c137e4c1058549268073868da6f1ac2cde99dc6c4fd1a6499e3daabc050539b1c78d66183fb25b7c47759622c4870a0c0cb71d1d7db93"));
    BOOST_CHECK(key2.SignCompact(hashMsg, detsig));
    BOOST_CHECK(key2C.SignCompact(hashMsg, detsigc));
    BOOST_CHECK(detsig == ParseHex("1bbb85cceb64ad65a586ecd14fca2c4a3d572e5a4fe619fa2bd4e661d902e581c92dec07eea5b8c7ceadf558aebd04bdaebbf05af985fb6547f41e124db7b541f9"));
    BOOST_CHECK(detsigc == ParseHex("1fbb85cceb64ad65a586ecd14fca2c4a3d572e5a4fe619fa2bd4e661d902e581c92dec07eea5b8c7ceadf558aebd04bdaebbf05af985fb6547f41e124db7b541f9"));
}

BOOST_AUTO_TEST_SUITE_END()
