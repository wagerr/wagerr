// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin developers
// Copyright (c) 2014-2015 The Dash developers
// Copyright (c) 2015-2017 The PIVX developers
// Copyright (c) 2018 The Wagerr developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "libzerocoin/Params.h"
#include "chainparams.h"
#include "random.h"
#include "util.h"
#include "utilstrencodings.h"

#include <assert.h>

#include <boost/assign/list_of.hpp>

using namespace std;
using namespace boost::assign;

struct SeedSpec6 {
    uint8_t addr[16];
    uint16_t port;
};

#include "chainparamsseeds.h"

/**
 * Main network
 */

//! Convert the pnSeeds6 array into usable address objects.
static void convertSeed6(std::vector<CAddress>& vSeedsOut, const SeedSpec6* data, unsigned int count)
{
    // It'll only connect to one or two seed nodes because once it connects,
    // it'll get a pile of addresses with newer timestamps.
    // Seed nodes are given a random 'last seen time' of between one and two
    // weeks ago.
    const int64_t nOneWeek = 7 * 24 * 60 * 60;
    for (unsigned int i = 0; i < count; i++) {
        struct in6_addr ip;
        memcpy(&ip, data[i].addr, sizeof(ip));
        CAddress addr(CService(ip, data[i].port));
        addr.nTime = GetTime() - GetRand(nOneWeek) - nOneWeek;
        vSeedsOut.push_back(addr);
    }
}

//   What makes a good checkpoint block?
// + Is surrounded by blocks with reasonable timestamps
//   (no blocks before with a timestamp after, none after with
//    timestamp before)
// + Contains no strange transactions
static Checkpoints::MapCheckpoints mapCheckpoints =
    boost::assign::map_list_of
    
    // WagerrDevs - RELEASE CHANGE - Checkpoins, timestamp of last checkpoint, total nr. of transactions
    (1, uint256("000001364c4ed20f1b240810b5aa91fee23ae9b64b6e746b594b611cf6d8c87b"))             // First PoW premine block
    (101, uint256("0000005e89a1fab52bf996e7eb7d653962a0eb064c16c09887504797deb7feaf"))           // Last premine block
    (1001, uint256("0000002a314058a8f61293e18ddbef5664a2097ac0178005f593444549dd5b8c"))          // Last PoW block
    (4559, uint256("f26ac8c87c21ab26e34340c54b2a16c0f1b1c1cce2202e0eb6e79ca9579e2022"))          // 
    (5530, uint256("b3a8e6eb90085394c1af916d5690fd5b83d53c43cf60c7b6dd1e904e0ede8e88"))          // Block on which switch off happened, 5531, 5532 differed
    (6160, uint256("7e7e688ae130d6b0bdfd3f059c6be93ba5e59c0bc28eb13f09e8158092151ad4"));         // Last block
static const Checkpoints::CCheckpointData data = {
    &mapCheckpoints,
    1519155533, // * UNIX timestamp of last checkpoint block
    11985,      // * total number of transactions between genesis and last checkpoint
                //   (the tx=... number in the SetBestChain debug.log lines)
    2000        // * estimated number of transactions per day after checkpoint
};

static Checkpoints::MapCheckpoints mapCheckpointsTestnet =
    boost::assign::map_list_of
    (1, uint256("0x00000385558ec1b9af7f939e1626a3116b9fb988c86c2f915e6451e8efcd0521"))          // First PoW block
    (200, uint256("0x0000015bef4f6677a5055b7b35fa0791d4df50c2a887c39aaf6ec42e2aab13fa"))
    (300, uint256("0x00000020e9647debfcf5a2c2b0b43d24eef32646a7b1b3e78d82879080a07366"))        // Last PoW block
    (941, uint256("0xb40633bf2a3b4c4ecc5a784233287ba7a74a17b58dc40c29d3886ef34c10181a"));  
static const Checkpoints::CCheckpointData dataTestnet = {
    &mapCheckpointsTestnet,
    1519060029,
    1609,
    250};

static Checkpoints::MapCheckpoints mapCheckpointsRegtest =
    boost::assign::map_list_of(0, uint256("0x671d0510c128608897d98d1819d26b40810c8b7e4901447a909c87a9edc2f5ec"));
static const Checkpoints::CCheckpointData dataRegtest = {
    &mapCheckpointsRegtest,
    1518696183,
    0,
    100};

libzerocoin::ZerocoinParams* CChainParams::Zerocoin_Params() const
{
    assert(this);
    static CBigNum bnTrustedModulus(zerocoinModulus);
    static libzerocoin::ZerocoinParams ZCParams = libzerocoin::ZerocoinParams(bnTrustedModulus);

    return &ZCParams;
}

class CMainParams : public CChainParams
{
public:
    CMainParams()
    {
        networkID = CBaseChainParams::MAIN;
        strNetworkID = "main";
        /**
         * The message start string is designed to be unlikely to occur in normal data.
         * The characters are rarely used upper ASCII, not valid as UTF-8, and produce
         * a large 4-byte int at any alignment.
         */ 
        /* 842d61fd */
        pchMessageStart[0] = 0x84;              
        pchMessageStart[1] = 0x2d;              
        pchMessageStart[2] = 0x61;              
        pchMessageStart[3] = 0xfd;              
        vAlertPubKey = ParseHex("04a6254e2ff57f433b3afa2764d8fffec82899a22fd14de054123f5f6d5dc4c9d011d6b03e35f9b093f1529cfe8324cf516838ff90fa8824be6ceea8fdf856bd64");
        nDefaultPort = 55002;                   
        bnProofOfWorkLimit = ~uint256(0) >> 20; // Wagerr starting difficulty is 1 / 2^12
        nSubsidyHalvingInterval = 210000;       // Halving interval
        nMaxReorganizationDepth = 100;          
        nEnforceBlockUpgradeMajority = 750;     
        nRejectBlockOutdatedMajority = 950;     
        nToCheckBlockUpgradeMajority = 1000;    
        nMinerThreads = 0;                      // Obsolete (**TODO**)
        nTargetTimespan = 1 * 60;               // Wagerr: 1 day
        nTargetSpacing = 1 * 60;                // Wagerr: 1 minute
        nMaturity = 100;                        // Block maturity
        nMasternodeCountDrift = 20;             
        nMaxMoneyOut = 398360470 * COIN;        // Max. Money is calculated by 2x premine value

        /** Height or Time Based Activations **/
        nLastPOWBlock = 1001;                   // PoW Phase 3 End
        nModifierUpdateBlock = 1100;            // Modify block on height
        nZerocoinStartHeight = 1500;            // Zerocoin start height
        nZerocoinStartTime = 1518696182;        // GMT: Thursday, 15. February 2018 12:03:02
        nBlockEnforceSerialRange = 1;           // Enforce serial range starting this block
        nBlockRecalculateAccumulators = 100000; // Trigger a recalculation of accumulators
        nBlockFirstFraudulent = 1110;           // 1110; //First block that bad serials emerged (currently we do not have any) *** TODO ***
        nBlockLastGoodCheckpoint = 1001;        // Last valid accumulator checkpoint (currently we do not have any) *** TODO ***
        nBlockEnforceInvalidUTXO = 1110;        // Start enforcing the invalid UTXO's
        
        /**
         * Build the genesis block. Note that the output of the genesis coinbase cannot
         * be spent as it did not originally exist in the database.
         *
         * CBlock(hash=00000ffd590b14, ver=1, hashPrevBlock=00000000000000, hashMerkleRoot=e0028e, nTime=1390095618, nBits=1e0ffff0, nNonce=28917698, vtx=1)
         *   CTransaction(hash=e0028e, ver=1, vin.size=1, vout.size=1, nLockTime=0)
         *     CTxIn(COutPoint(000000, -1), coinbase 04ffff001d01044c5957697265642030392f4a616e2f3230313420546865204772616e64204578706572696d656e7420476f6573204c6976653a204f76657273746f636b2e636f6d204973204e6f7720416363657074696e6720426974636f696e73)
         *     CTxOut(nValue=50.00000000, scriptPubKey=0xA9037BAC7050C479B121CF)
         *   vMerkleTree: e0028e
         */
        const char* pszTimestamp = "RT 15/Feb/2018 12.03 GMT - Soros brands bitcoin nest egg for dictators, but still invests in it";
        CMutableTransaction txNew;
        txNew.vin.resize(1);
        txNew.vout.resize(1);
        txNew.vin[0].scriptSig = CScript() << 486604799 << CScriptNum(4) << vector<unsigned char>((const unsigned char*)pszTimestamp, (const unsigned char*)pszTimestamp + strlen(pszTimestamp));
        txNew.vout[0].nValue = 0 * COIN;
        txNew.vout[0].scriptPubKey = CScript() << ParseHex("046013426db3d877adca7cea18ebeca33e88fafc53ab4040e0fe1bd0429712178c10571dfed6b3f1f19bcff0805cdf1c798e7a84ef0f5e0f4459aabd7e94ced9e6") << OP_CHECKSIG;
        genesis.vtx.push_back(txNew);
        genesis.hashPrevBlock = 0;
        genesis.hashMerkleRoot = genesis.BuildMerkleTree();
        genesis.nVersion = 1;
        genesis.nTime = 1518696181;                                         // GMT: Thursday, 15. February 2018 12:03:01
        genesis.nBits = 0x1e0ffff0;
        genesis.nNonce = 96620932;

        hashGenesisBlock = genesis.GetHash();
        assert(hashGenesisBlock == uint256("0x000007b9191bc7a17bfb6cedf96a8dacebb5730b498361bf26d44a9f9dcc1079"));
        assert(genesis.hashMerkleRoot == uint256("0xc4d06cf72583752c23b819fa8d8cededd1dad5733d413ea1f123f98a7db6af13"));

        vSeeds.push_back(CDNSSeedData("1", "main.seederv1.wgr.host"));      // Wagerr's official seed 1
        vSeeds.push_back(CDNSSeedData("2", "main.seederv2.wgr.host"));      // Wagerr's official seed 2
        vSeeds.push_back(CDNSSeedData("3", "main.devseeder1.wgr.host"));    // Wagerr's dev1 testseed
        vSeeds.push_back(CDNSSeedData("4", "main.devseeder2.wgr.host"));    // Wagerr's dev2 testseed

        // Wagerr addresses start with 'W'
        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1, 73);
        // Wagerr script addresses start with '7'
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1, 63);
        // Wagerr private keys start with '7' or 'W'
        base58Prefixes[SECRET_KEY] = std::vector<unsigned char>(1, 199);
        base58Prefixes[EXT_PUBLIC_KEY] = boost::assign::list_of(0x02)(0x2D)(0x25)(0x33).convert_to_container<std::vector<unsigned char> >();
        base58Prefixes[EXT_SECRET_KEY] = boost::assign::list_of(0x02)(0x21)(0x31)(0x2B).convert_to_container<std::vector<unsigned char> >();
        // 	BIP44 coin type is from https://github.com/satoshilabs/slips/blob/master/slip-0044.md
        base58Prefixes[EXT_COIN_TYPE] = boost::assign::list_of(0x80)(0x00)(0x00)(0x77).convert_to_container<std::vector<unsigned char> >();

        convertSeed6(vFixedSeeds, pnSeed6_main, ARRAYLEN(pnSeed6_main));

        fMiningRequiresPeers = true;
        fAllowMinDifficultyBlocks = false;
        fDefaultConsistencyChecks = false;
        fRequireStandard = true;
        fMineBlocksOnDemand = false;
        fSkipProofOfWorkCheck = false;
        fTestnetToBeDeprecatedFieldRPC = false;
        fHeadersFirstSyncingActive = false;

        nPoolMaxTransactions = 3;
        strSporkKey = "040f00b37452d6e7ac00b4a2e2699bab35b5ed3c8d3e1ecaf63317900fd7b52324f4243d11cc70c40dde54bdbc1e9a732ee63b1eec60ca45e6d529ad2b43d4d614";
        strObfuscationPoolDummyAddress = "WWqou25edpCatoZgSxhd3dpNbhn3dxh21D";
        nStartMasternodePayments = 1518696182; // GMT: Thursday, 15. February 2018 12:03:02

        /** Zerocoin */
        zerocoinModulus = "25195908475657893494027183240048398571429282126204032027777137836043662020707595556264018525880784"
            "4069182906412495150821892985591491761845028084891200728449926873928072877767359714183472702618963750149718246911"
            "6507761337985909570009733045974880842840179742910064245869181719511874612151517265463228221686998754918242243363"
            "7259085141865462043576798423387184774447920739934236584823824281198163815010674810451660377306056201619676256133"
            "8441436038339044149526344321901146575444541784240209246165157233507787077498171257724679629263863563732899121548"
            "31438167899885040445364023527381951378636564391212010397122822120720357";
        nMaxZerocoinSpendsPerTransaction = 7; // Assume about 20kb each
        nMinZerocoinMintFee = 1 * CENT; //high fee required for zerocoin mints
        nMintRequiredConfirmations = 20; //the maximum amount of confirmations until accumulated in 19
        nRequiredAccumulation = 1;
        nDefaultSecurityLevel = 100; //full security level for accumulators
        nZerocoinHeaderVersion = 4; //Block headers must be this version once zerocoin is active
        nBudget_Fee_Confirmations = 6; // Number of confirmations for the finalization fee
    }

    const Checkpoints::CCheckpointData& Checkpoints() const
    {
        return data;
    }
};
static CMainParams mainParams;

/**
 * Testnet (v3)
 */
class CTestNetParams : public CMainParams
{
public:
    CTestNetParams()
    {
        networkID = CBaseChainParams::TESTNET;
        strNetworkID = "test";
        /* 879ed199 */
        pchMessageStart[0] = 0x87;
        pchMessageStart[1] = 0x9e;
        pchMessageStart[2] = 0xd1;
        pchMessageStart[3] = 0x99;
        vAlertPubKey = ParseHex("04f91dd428ecc5f2bee0aaf073f8a258418a34e6ff42c8454dbb42d02aac6c2596d42e2f63f435c6143f35354fb0f9645c5e158f396091974439184ccdff6e06cd");
        nDefaultPort = 55004;
        nEnforceBlockUpgradeMajority = 51;
        nRejectBlockOutdatedMajority = 75;
        nToCheckBlockUpgradeMajority = 100;
        nMinerThreads = 0;
        nTargetTimespan = 1 * 60; // Wagerr: 1 day
        nTargetSpacing = 1 * 60;  // Wagerr: 1 minute
        nLastPOWBlock = 300;
        nMaturity = 15;
        nMasternodeCountDrift = 4;
        nModifierUpdateBlock = 1; //approx Mon, 17 Apr 2017 04:00:00 GMT
        nMaxMoneyOut = 398360470 * COIN;
        nZerocoinStartHeight = 350;
        nZerocoinStartTime = 1518696183; // GMT: Thursday, 15. February 2018 12:03:03
        nBlockEnforceSerialRange = 1; //Enforce serial range starting this block
        nBlockRecalculateAccumulators = 1500; //Trigger a recalculation of accumulators
        nBlockFirstFraudulent = 891737; //First block that bad serials emerged (currently we do not have any) *** TODO ***
        nBlockLastGoodCheckpoint = 1001; //Last valid accumulator checkpoint (currently we do not have any) *** TODO ***
        nBlockEnforceInvalidUTXO = 1600; //Start enforcing the invalid UTXO's
        
        //! Modify the testnet genesis block so the timestamp is valid for a later start.
        genesis.nTime = 1518696182;
        genesis.nNonce = 75183976;

        hashGenesisBlock = genesis.GetHash();
        assert(hashGenesisBlock == uint256("0x00000fdc268f54ff1368703792dc046b1356e60914c2b5b6348032144bcb2de5"));

        vFixedSeeds.clear();
        vSeeds.clear();
        vSeeds.push_back(CDNSSeedData("1", "test.testseederv1.wgr.host")); // Wagerr's official testseed 1
        vSeeds.push_back(CDNSSeedData("2", "test.testseederv2.wgr.host")); // Wagerr's official testseed 2
        vSeeds.push_back(CDNSSeedData("3", "test.devseeder1.wgr.host"));   // Wagerr's dev1 testseed
        vSeeds.push_back(CDNSSeedData("4", "test.devseeder2.wgr.host"));   // Wagerr's dev2 testseed
        // Testnet Wagerr addresses start with 'y'
        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1, 65);
        // Testnet Wagerr script addresses start with '8' or '9'
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1, 125);
        // Testnet private keys start with '9' or 'c' (Wagerr defaults)
        base58Prefixes[SECRET_KEY] = std::vector<unsigned char>(1, 177); 
        // Testnet wagerr BIP32 pubkeys start with 'DRKV'
        base58Prefixes[EXT_PUBLIC_KEY] = boost::assign::list_of(0x3a)(0x80)(0x61)(0xa0).convert_to_container<std::vector<unsigned char> >();
        // Testnet wagerr BIP32 prvkeys start with 'DRKP'
        base58Prefixes[EXT_SECRET_KEY] = boost::assign::list_of(0x3a)(0x80)(0x58)(0x37).convert_to_container<std::vector<unsigned char> >();
        // Testnet wagerr BIP44 coin type is '1' (All coin's testnet default)
        base58Prefixes[EXT_COIN_TYPE] = boost::assign::list_of(0x80)(0x00)(0x00)(0x01).convert_to_container<std::vector<unsigned char> >();

        convertSeed6(vFixedSeeds, pnSeed6_test, ARRAYLEN(pnSeed6_test));

        fMiningRequiresPeers = true;
        fAllowMinDifficultyBlocks = true;
        fDefaultConsistencyChecks = false;
        fRequireStandard = false;
        fMineBlocksOnDemand = false;
        fTestnetToBeDeprecatedFieldRPC = true;

        nPoolMaxTransactions = 2;
        strSporkKey = "04b2d1b19607edcca2fbf1d3238a0200a434900593f7e5e38102e7681465e5785ddcf1a105ee595c51ef3be1bfc8ea9dc14c8c30b2e0edaa5f5d3f57b77f272046";
        strObfuscationPoolDummyAddress = "TMPUBzcsHZawA32XYYDF9FHQp6icv492CV";
        nStartMasternodePayments = 1518696183; // GMT: Thursday, 15. February 2018 12:03:03
        nBudget_Fee_Confirmations = 3; // Number of confirmations for the finalization fee. We have to make this very short
                                       // here because we only have a 8 block finalization window on testnet
    }
    const Checkpoints::CCheckpointData& Checkpoints() const
    {
        return dataTestnet;
    }
};
static CTestNetParams testNetParams;

/**
 * Regression test
 */
class CRegTestParams : public CTestNetParams
{
public:
    CRegTestParams()
    {
        networkID = CBaseChainParams::REGTEST;
        strNetworkID = "regtest";
        strNetworkID = "regtest";
        /* 1276a1fa */
        pchMessageStart[0] = 0x12;
        pchMessageStart[1] = 0x76;
        pchMessageStart[2] = 0xa1;
        pchMessageStart[3] = 0xfa;
        nSubsidyHalvingInterval = 150;
        nEnforceBlockUpgradeMajority = 750;
        nRejectBlockOutdatedMajority = 950;
        nToCheckBlockUpgradeMajority = 1000;
        nMinerThreads = 1;
        nTargetTimespan = 24 * 60 * 60;         // Wagerr: 1 day
        nTargetSpacing = 1 * 60;                // Wagerr: 1 minutes
        bnProofOfWorkLimit = ~uint256(0) >> 1;
        genesis.nTime = 1518696183;             // GMT: Thursday, 15. February 2018 12:03:03
        genesis.nBits = 0x207fffff;
        genesis.nNonce = 574752;                // hex 57 47 52 in text = WGR

        hashGenesisBlock = genesis.GetHash();
        nDefaultPort = 55006;
        assert(hashGenesisBlock == uint256("0x671d0510c128608897d98d1819d26b40810c8b7e4901447a909c87a9edc2f5ec"));

        vFixedSeeds.clear();                    //! Testnet mode doesn't have any fixed seeds.
        vSeeds.clear();                         //! Testnet mode doesn't have any DNS seeds.

        fMiningRequiresPeers = false;
        fAllowMinDifficultyBlocks = true;
        fDefaultConsistencyChecks = true;
        fRequireStandard = false;
        fMineBlocksOnDemand = true;
        fTestnetToBeDeprecatedFieldRPC = false;
    }
    const Checkpoints::CCheckpointData& Checkpoints() const
    {
        return dataRegtest;
    }
};
static CRegTestParams regTestParams;

/**
 * Unit test
 */
class CUnitTestParams : public CMainParams, public CModifiableParams
{
public:
    CUnitTestParams()
    {
        networkID = CBaseChainParams::UNITTEST;
        strNetworkID = "unittest";
        nDefaultPort = 51478;
        vFixedSeeds.clear(); //! Unit test mode doesn't have any fixed seeds.
        vSeeds.clear();      //! Unit test mode doesn't have any DNS seeds.

        fMiningRequiresPeers = false;
        fDefaultConsistencyChecks = true;
        fAllowMinDifficultyBlocks = false;
        fMineBlocksOnDemand = true;
    }

    const Checkpoints::CCheckpointData& Checkpoints() const
    {
        // UnitTest share the same checkpoints as MAIN
        return data;
    }

    //! Published setters to allow changing values in unit test cases
    virtual void setSubsidyHalvingInterval(int anSubsidyHalvingInterval) { nSubsidyHalvingInterval = anSubsidyHalvingInterval; }
    virtual void setEnforceBlockUpgradeMajority(int anEnforceBlockUpgradeMajority) { nEnforceBlockUpgradeMajority = anEnforceBlockUpgradeMajority; }
    virtual void setRejectBlockOutdatedMajority(int anRejectBlockOutdatedMajority) { nRejectBlockOutdatedMajority = anRejectBlockOutdatedMajority; }
    virtual void setToCheckBlockUpgradeMajority(int anToCheckBlockUpgradeMajority) { nToCheckBlockUpgradeMajority = anToCheckBlockUpgradeMajority; }
    virtual void setDefaultConsistencyChecks(bool afDefaultConsistencyChecks) { fDefaultConsistencyChecks = afDefaultConsistencyChecks; }
    virtual void setAllowMinDifficultyBlocks(bool afAllowMinDifficultyBlocks) { fAllowMinDifficultyBlocks = afAllowMinDifficultyBlocks; }
    virtual void setSkipProofOfWorkCheck(bool afSkipProofOfWorkCheck) { fSkipProofOfWorkCheck = afSkipProofOfWorkCheck; }
};
static CUnitTestParams unitTestParams;


static CChainParams* pCurrentParams = 0;

CModifiableParams* ModifiableParams()
{
    assert(pCurrentParams);
    assert(pCurrentParams == &unitTestParams);
    return (CModifiableParams*)&unitTestParams;
}

const CChainParams& Params()
{
    assert(pCurrentParams);
    return *pCurrentParams;
}

CChainParams& Params(CBaseChainParams::Network network)
{
    switch (network) {
    case CBaseChainParams::MAIN:
        return mainParams;
    case CBaseChainParams::TESTNET:
        return testNetParams;
    case CBaseChainParams::REGTEST:
        return regTestParams;
    case CBaseChainParams::UNITTEST:
        return unitTestParams;
    default:
        assert(false && "Unimplemented network");
        return mainParams;
    }
}

void SelectParams(CBaseChainParams::Network network)
{
    SelectBaseParams(network);
    pCurrentParams = &Params(network);
}

bool SelectParamsFromCommandLine()
{
    CBaseChainParams::Network network = NetworkIdFromCommandLine();
    if (network == CBaseChainParams::MAX_NETWORK_TYPES)
        return false;

    SelectParams(network);
    return true;
}
