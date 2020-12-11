// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin developers
// Copyright (c) 2014-2015 The Dash developers
// Copyright (c) 2015-2018 The PIVX developers
// Copyright (c) 2018 The Wagerr developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "libzerocoin/Params.h"
#include "betting/quickgames/dice.h"
#include "betting/quickgames/qgview.h"
#include "chainparams.h"
#include "consensus/merkle.h"
#include "random.h"
#include "util.h"
#include "utilstrencodings.h"

#include <assert.h>

#include <boost/assign/list_of.hpp>

#include <clientversion.h>

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
    (       1, uint256("000001364c4ed20f1b240810b5aa91fee23ae9b64b6e746b594b611cf6d8c87b"))     // First PoW premine block
    (     101, uint256("0000005e89a1fab52bf996e7eb7d653962a0eb064c16c09887504797deb7feaf"))     // Last premine block
    (    1001, uint256("0000002a314058a8f61293e18ddbef5664a2097ac0178005f593444549dd5b8c"))     // Last PoW block
    (    5530, uint256("b3a8e6eb90085394c1af916d5690fd5b83d53c43cf60c7b6dd1e904e0ede8e88"))     // Block on which switch off happened, 5531, 5532 differed
    (   14374, uint256("61dc2dbb225de3146bc59ab96dedf48047ece84d004acaf8f386ae7a7d074983"))
    (   70450, uint256("ea83266a9dfd7cf92a96aa07f86bdf60d45850bd47c175745e71a1aaf60b4091"))
    (  257142, uint256("eca635870323e7c0785fec1e663f4cb8645b7e84b5df4511ba4c189e580bfafd"))
    (  290000, uint256("5a70e614a2e6035be0fa1dd1a67bd6caa0a78e396e889aac42bbbc08e11cdabd"))
    (  294400, uint256("01be3c3c84fd6063ba27080996d346318242d5335efec936408c1e1ae3fdb4a1"))
    (  320000, uint256("9060f8d44058c539653f37eaac4c53de7397e457dda264c5ee1be94293e9f6bb"))
    (  695857, uint256("680a170b5363f308cc0698a53ab6a83209dab06c138c98f91110f9e11e273778"))
    (  720000, uint256("63fc356380b3b8791e83a9d63d059ccc8d0e65dab703575ef4ca070e26e02fc7"))
    (  732900, uint256("5d832b3de9b207e03366fb8d4da6265d52015f5d1bd8951a656b5d4508a1da8e"))
    (  891270, uint256("eedb1794ca9267fb0ef88aff27afdd376ac93a54491a7b812cbad4b6c2e28d25"))
    ( 1427000, uint256("2ee16722a21094f4ae8e371021c28d19268d6058de42e37ea0d4c90273c6a42e"));    // 3693972 1605485238
static const Checkpoints::CCheckpointData data = {
    &mapCheckpoints,
    1605485238,    // * UNIX timestamp of last checkpoint block
    3693972,       // * total number of transactions between genesis and last checkpoint
                   //   (the tx=... number in the SetBestChain debug.log lines)
    4400           // * estimated number of transactions per day after checkpoint
};

static Checkpoints::MapCheckpoints mapCheckpointsTestnet =
    boost::assign::map_list_of
    (       0, uint256("00000fdc268f54ff1368703792dc046b1356e60914c2b5b6348032144bcb2de5"))
    (       1, uint256("0000098cc93ece2804776d2e9eda2d01e2ff830d80bab22500821361259f8aa3"))
    (     450, uint256("3cec3911fdf321a22b8109ca95ca28913e6b51f0d80cc6d2b2e30e1f2a6115c0"))
    (     469, uint256("d69d843cd63d333cfa3ff4dc0675fa320d6ef8cab7ab1a73bf8a1482210f93ce"))
    (    1100, uint256("fa462709a1f3cf81d699ffbd45440204aa4d38de84c2da1fc8b3ff15c3c7a95f"))  // 1588780440
    (    2000, uint256("a5aab45e4e2345715adf79774d661a5bb9b2a2efd001c339df5678418fb51409")); // 1588834261
static const Checkpoints::CCheckpointData dataTestnet = {
    &mapCheckpointsTestnet,
    1588834261,
    3724,
    1000};

static Checkpoints::MapCheckpoints mapCheckpointsRegtest =
    boost::assign::map_list_of(0, uint256("0x671d0510c128608897d98d1819d26b40810c8b7e4901447a909c87a9edc2f5ec"));
static const Checkpoints::CCheckpointData dataRegtest = {
    &mapCheckpointsRegtest,
    1518696183,
    0,
    100};

libzerocoin::ZerocoinParams* CChainParams::Zerocoin_Params(bool useModulusV1) const
{
    assert(this);
    static CBigNum bnHexModulus = 0;
    if (!bnHexModulus)
        bnHexModulus.SetHex(zerocoinModulus);
    static libzerocoin::ZerocoinParams ZCParamsHex = libzerocoin::ZerocoinParams(bnHexModulus);
    static CBigNum bnDecModulus = 0;
    if (!bnDecModulus)
        bnDecModulus.SetDec(zerocoinModulus);
    static libzerocoin::ZerocoinParams ZCParamsDec = libzerocoin::ZerocoinParams(bnDecModulus);

    if (useModulusV1)
        return &ZCParamsHex;

    return &ZCParamsDec;
}

int CChainParams::COINBASE_MATURITY(const int contextHeight) const {
    return contextHeight < nMaturityV2StartHeight ? nMaturityV1 : nMaturityV2;
}

bool CChainParams::HasStakeMinAgeOrDepth(const int contextHeight, const uint32_t contextTime,
        const int utxoFromBlockHeight, const uint32_t utxoFromBlockTime) const
{
    // before stake modifier V2, the age required was 60 * 60 (1 hour). Not required for regtest
    if (!IsStakeModifierV2(contextHeight))
        return NetworkID() == CBaseChainParams::REGTEST || (utxoFromBlockTime + nStakeMinAge <= contextTime);

    // after stake modifier V2, we require the utxo to be nStakeMinDepth deep in the chain
    return (contextHeight - utxoFromBlockHeight >= nStakeMinDepth);
}

int CChainParams::FutureBlockTimeDrift(const int nHeight) const
{
    if (IsTimeProtocolV2(nHeight))
        // PoS (TimeV2): 14 seconds
        return TimeSlotLength() - 1;

    // PoS (TimeV1): 3 minutes
    // PoW: 2 hours
    return (nHeight > LAST_POW_BLOCK()) ? nFutureTimeDriftPoS : nFutureTimeDriftPoW;
}

bool CChainParams::IsValidBlockTimeStamp(const int64_t nTime, const int nHeight) const
{
    // Before time protocol V2, blocks can have arbitrary timestamps
    if (!IsTimeProtocolV2(nHeight))
        return true;

    // Time protocol v2 requires time in slots
    return (nTime % TimeSlotLength()) == 0;
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
        pchMessageStart[0] = 0x84;
        pchMessageStart[1] = 0x2d;
        pchMessageStart[2] = 0x61;
        pchMessageStart[3] = 0xfd;
        vAlertPubKey = ParseHex("04300ed6502f7210f8864f1facb2b817f085d5dc7ebf1577dfe14f4fc7ab37d851aa54aa3d2d252823063524750faaf24427ede912bf4958f7b3e63c7cce8dd036");
        nDefaultPort = 55002;
        bnProofOfWorkLimit = ~uint256(0) >> 20; // Wagerr starting difficulty is 1 / 2^12
        bnProofOfStakeLimit = ~uint256(0) >> 24;
        bnProofOfStakeLimit_V2 = ~uint256(0) >> 20; // 60/4 = 15 ==> use 2**4 higher limit
        nSubsidyHalvingInterval = 210000;
        nMaxReorganizationDepth = 100;
        nMaxBettingUndoDepth = 101;
        nEnforceBlockUpgradeMajority = 8100; // 75%
        nRejectBlockOutdatedMajority = 10260; // 95%
        nToCheckBlockUpgradeMajority = 10800; // Approximate expected amount of blocks in 7 days (1440*7.5)
        nMinerThreads = 0;
        nTargetSpacing = 1 * 60;                        // 1 minute
        nTargetTimespan = 40 * 60;                      // 40 minutes
        nTimeSlotLength = 15;                           // 15 seconds
        nTargetTimespan_V2 = 2 * nTimeSlotLength * 60;  // 30 minutes
        nMaturityV1 = 100;
        nMaturityV2 = 60;
        nStakeMinAge = 60 * 60;                         // 1 hour
        nStakeMinDepth = 600;
        nFutureTimeDriftPoW = 7200;
        nFutureTimeDriftPoS = 180;
        nMasternodeCountDrift = 20;
        nMaxMoneyOut = 398360470 * COIN;

        /** Height or Time Based Activations **/
        nLastPOWBlock = 1001;                   // PoW Phase 3 End
        nModifierUpdateBlock = 1100;            // Modify block on height
        nZerocoinStartHeight = 1500;            // Zerocoin start height
        nZerocoinAccumulationStartHeight = 1500;// RCP command starts accumulation here - first zerocoin mint occurs at block 87
        nZerocoinStartTime = 1518696182;        // GMT: Thursday, 15. February 2018 12:03:02
        nBlockEnforceSerialRange = 1;           // Enforce serial range starting this block
        nBlockRecalculateAccumulators = 1650;   // Trigger a recalculation of accumulators
        nBlockFirstFraudulent = 99999999;       // 1110; //First block that bad serials emerged (currently we do not have any) *** TODO ***
        nBlockLastGoodCheckpoint = 1648;        // Last valid accumulator checkpoint (currently we do not have any) *** TODO ***
        nBlockEnforceInvalidUTXO = 1500;        // Start enforcing the invalid UTXO's
        nInvalidAmountFiltered = 0*COIN;        //Amount of invalid coins filtered through exchanges, that should be considered valid
        nBlockZerocoinV2 = 298386;              //The block that zerocoin v2 becomes active (estimated at unix time 1536868800 -  (GMT): Thursday, September 13, 2018 6:00:00 PM
        nBlockDoubleAccumulated = 99999999;

        /** Block height at which BIP34 becomes active */
        nBIP34Height = 1;
        // Start enforcing CHECKLOCKTIMEVERIFY (BIP65) rule
        nBIP65Height = 751858;

        nBlockStakeModifierV2 = 891276;
        // Activation height for TimeProtocolV2, Blocks V7 and newMessageSignatures
        nBlockTimeProtocolV2 = std::numeric_limits<int>::max(); // !TODO: change me

        // Public coin spend enforcement
        nPublicZCSpends = 752800;

        // New P2P messages signatures
        nBlockEnforceNewMessageSignatures = nBlockTimeProtocolV2;

        // Blocks v7
        nBlockLastAccumulatorCheckpoint = 556830; // 861f19fc18b7b4f136117582f4b0ef5d855de652228417fa246992c0fa613ea3
        nBlockV7StartHeight = nBlockTimeProtocolV2;

        nZerocoinStartHeight = 700;            // Start accumulation coins here - first zerocoin mint occurs at block

        /** Bet related parameters **/
        nBetBlocksIndexTimespanV2 = 23040;                              // Checking back 2 weeks for events and bets for each result.  (With approx. 2 days buffer).
        nBetBlocksIndexTimespanV3 = 90050;                              // Checking back 2 months for events and bets for each result.  (With approx. 2 days buffer).
        nOMNORewardPermille = 24;                                       // profitAcc / (100-6) * 100 * 0.024 (nMNBetReward = Total Profit * 0.024).
        nDevRewardPermille = 6;                                         // profitAcc / (100-6) * 100 * 0.006 (nDevReward = Total Profit * 0.006).
        nBetBlockPayoutAmount = 1440;                                   // Set the number of blocks we want to look back for results already paid out.
        nMinBetPayoutRange = 25;                                        // Spam filter to prevent malicious actors congesting the chain (Only payout bets that are between 25 - 10000 WRG inclusive).
        nMaxBetPayoutRange = 10000;                                     // Minimizes maximum payout size to avoid unnecessary large numbers (Only payout bets that are between 25 - 10000 WRG inclusive).
        nMaxParlayBetPayoutRange = 4000;                                // Minimizes maximum parlay payout size to avoid unnecessary large numbers (Only payout parlay bets that are between 25 - 4000 WRG inclusive).
        nBetPlaceTimeoutBlocks = 120;                                   // Discard bets placed less than 120 seconds (approx. 2 mins) before event start time
        nMaxParlayLegs = 5;                                             // Minimizes maximum legs in parlay bet
        nWagerrProtocolV1StartHeight = 298386;                          // Betting protocol v1 activation block
        nWagerrProtocolV2StartHeight = 763350;                          // Betting protocol v2 activation block
        nWagerrProtocolV3StartHeight = std::numeric_limits<int>::max(); // Betting protocol v3 activation block
        nQuickGamesEndHeight = nWagerrProtocolV3StartHeight;
        nMaturityV2StartHeight = nWagerrProtocolV3StartHeight;          // Reduced block maturity required for spending coinstakes and betting payouts

        nKeysRotateHeight = std::numeric_limits<int>::max();            // Rotate spork key, oracle keys and fee payout keys
        nEnforceNewSporkKey = 4070908800;       //!> Sporks signed after must use the new spork key (GMT): Thursday, September 13, 2018 6:00:00 PM
        nRejectOldSporkKey = 4070908800;        //!> Fully reject old spork key after (GMT): Sunday, September 16, 2018 8:00:00 PM

        strSporkPubKey = "04a2f260ba521255f30cdd92dc5e3f53b753f2fcd04fb5510c4b7529843a535d05fabf9029bdfa40c5e0bed030f5f9c00766d6c510391ceb537016728cc44ddaef";
        strSporkPubKeyOld = "043cb569d89fb78fc61df67617012e6c33c1ba306f4620bbb89424279a4931adf4a9e238db60aa7f78cd10ef780f21f1fd3b881f014fd0f656db4b6a6a98f0cff2";

        strDevPayoutAddrOld = "Wm5om9hBJTyKqv5FkMSfZ2FDMeGp12fkTe";     // Development fund payout address (old).
        strDevPayoutAddrNew = "Shqrs3mz3i65BiTEKPgnxoqJqMw5b726m5";     // Development fund payout address (new).
        strOMNOPayoutAddrOld = "WRBs8QD22urVNeGGYeAMP765ncxtUA1Rv2";    // OMNO fund payout address (old).
        strOMNOPayoutAddrNew = "SNCNYcDyXPCLHpG9AyyhnPcLNpxCpGZ2X6";    // OMNO fund payout address (new).

        vOracles = {
            { "WcsijutAF46tSLTcojk9mR9zV9wqwUUYpC", strDevPayoutAddrOld, strOMNOPayoutAddrOld, nWagerrProtocolV2StartHeight, nKeysRotateHeight },
            { "Weqz3PFBq3SniYF5HS8kuj72q9FABKzDrP", strDevPayoutAddrOld, strOMNOPayoutAddrOld, nWagerrProtocolV2StartHeight, nKeysRotateHeight },
            { "WdAo2Xk8r1MVx7ZmxARpJJkgzaFeumDcCS", strDevPayoutAddrNew, strOMNOPayoutAddrNew, nKeysRotateHeight, std::numeric_limits<int>::max() },
            { "WhW3dmThz2hWEfpagfbdBQ7hMfqf6MkfHR", strDevPayoutAddrNew, strOMNOPayoutAddrNew, nKeysRotateHeight, std::numeric_limits<int>::max() },
        };

        quickGamesArr.clear();
        quickGamesArr.emplace_back(
            std::string("Dice"), // Game name
            QuickGamesType::qgDice, // game type
            &quickgames::DiceHandler, // game bet handler
            &quickgames::DiceBetInfoParser, // bet info parser
            std::string("Wm5om9hBJTyKqv5FkMSfZ2FDMeGp12fkTe"), // Dev address
            400, // OMNO reward permille (40%)
            100); // Dev reward permille (10%)

        // Fake Serial Attack
        nFakeSerialBlockheightEnd = 556623;
        nSupplyBeforeFakeSerial = 3703597*COIN;   // zerocoin supply at block nFakeSerialBlockheightEnd

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
        txNew.vin[0].scriptSig = CScript() << 486604799 << CScriptNum(4) << std::vector<unsigned char>((const unsigned char*)pszTimestamp, (const unsigned char*)pszTimestamp + strlen(pszTimestamp));
        txNew.vout[0].nValue = 0 * COIN;
        txNew.vout[0].scriptPubKey = CScript() << ParseHex("046013426db3d877adca7cea18ebeca33e88fafc53ab4040e0fe1bd0429712178c10571dfed6b3f1f19bcff0805cdf1c798e7a84ef0f5e0f4459aabd7e94ced9e6") << OP_CHECKSIG;
        genesis.vtx.push_back(txNew);
        genesis.hashPrevBlock = 0;
        genesis.hashMerkleRoot = BlockMerkleRoot(genesis);
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

        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1, 73);   // wagerr addresses start with 'W'
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1, 63);   // wagerr script addresses start with '7'
        base58Prefixes[SECRET_KEY] = std::vector<unsigned char>(1, 199);      // wagerr private keys start with '7' or 'W'
        base58Prefixes[EXT_PUBLIC_KEY] = boost::assign::list_of(0x02)(0x2D)(0x25)(0x33).convert_to_container<std::vector<unsigned char> >();
        base58Prefixes[EXT_SECRET_KEY] = boost::assign::list_of(0x02)(0x21)(0x31)(0x2B).convert_to_container<std::vector<unsigned char> >();
        //     BIP44 coin type is from https://github.com/satoshilabs/slips/blob/master/slip-0044.md
        base58Prefixes[EXT_COIN_TYPE] = boost::assign::list_of(0x80)(0x77)(0x67)(0x72).convert_to_container<std::vector<unsigned char> >(); // wgr in hex: 776772

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
        nBudgetCycleBlocks = 43200; //!< Amount of blocks in a months period of time (using 1 minutes per) = (60*24*30)
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
        nMaxZerocoinPublicSpendsPerTransaction = 637; // Assume about 220 bytes each input
        nMinZerocoinMintFee = 1 * CENT; //high fee required for zerocoin mints
        nMintRequiredConfirmations = 20; //the maximum amount of confirmations until accumulated in 19
        nRequiredAccumulation = 1;
        nDefaultSecurityLevel = 100; //full security level for accumulators
        nZerocoinHeaderVersion = 4; //Block headers must be this version once zerocoin is active
        nZerocoinRequiredStakeDepth = 200; //The required confirmations for a zwgr to be stakable

        nBudget_Fee_Confirmations = 6; // Number of confirmations for the finalization fee
        nProposalEstablishmentTime = 60 * 60 * 24; // Proposals must be at least a day old to make it into a budget
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
        vAlertPubKey = ParseHex("04b5aa7cd76159c35fb3dab3cf3cab8d93ecb592b2cbea519145e63cfe92110fe0f68d0e5205af01482334256358c070f5658f638e4191aa7298fb435b65216767");
        nDefaultPort = 55004;
        nEnforceBlockUpgradeMajority = 4320; // 75%
        nRejectBlockOutdatedMajority = 5472; // 95%
        nToCheckBlockUpgradeMajority = 5760; // 4 days
        nMinerThreads = 0;
        nLastPOWBlock = 300;
        nMaturityV1 = 15;
        nMaturityV2 = 10;
        nStakeMinDepth = 100;
        nMasternodeCountDrift = 4;
        nModifierUpdateBlock = 1; //approx Mon, 17 Apr 2017 04:00:00 GMT
        nMaxMoneyOut = 398360470 * COIN;
        nZerocoinStartHeight = 50;            // Start accumulation coins here - first zerocoin mint occurs at block 87
        nZerocoinAccumulationStartHeight = 350;
        nZerocoinStartTime = 1518696183; // GMT: Thursday, 15. February 2018 12:03:03
        nBlockEnforceSerialRange = 1; //Enforce serial range starting this block
        nBlockRecalculateAccumulators = -1; //Trigger a recalculation of accumulators
        nBlockFirstFraudulent = 99999999; //First block that bad serials emerged (currently we do not have any) *** TODO ***
        nBlockLastGoodCheckpoint = 350; //Last valid accumulator checkpoint (currently we do not have any) *** TODO ***
        nBlockEnforceInvalidUTXO = 350; //Start enforcing the invalid UTXO's
        nInvalidAmountFiltered = 0; //Amount of invalid coins filtered through exchanges, that should be considered valid
        nBlockZerocoinV2 = 600; //The block that zerocoin v2 becomes active

        /** Block height at which BIP34 becomes active */
        nBIP34Height = 3963;
        // Start enforcing CHECKLOCKTIMEVERIFY (BIP65) rule
        nBIP65Height = 600;

        nBlockStakeModifierV2 = 92500;
        // Activation height for TimeProtocolV2, Blocks V7 and newMessageSignatures
        nBlockTimeProtocolV2 = std::numeric_limits<int>::max();

        // Public coin spend enforcement
        nPublicZCSpends = 600;

        // New P2P messages signatures
        nBlockEnforceNewMessageSignatures = nBlockTimeProtocolV2;

        // Blocks v7
        nBlockLastAccumulatorCheckpoint = nPublicZCSpends - 10;
        nBlockV7StartHeight = nBlockTimeProtocolV2;

        /** Bet related parameters **/
        nBetBlocksIndexTimespanV2 = 23040;                              // Checking back 2 weeks for events and bets for each result.  (With approx. 2 days buffer).
        nBetBlocksIndexTimespanV3 = 90050;                              // Checking back 2 months for events and bets for each result.  (With approx. 2 days buffer).
        nOMNORewardPermille = 24;                                       // profitAcc / (100-6) * 100 * 0.024 (nMNBetReward = Total Profit * 0.024).
        nDevRewardPermille = 6;                                         // profitAcc / (100-6) * 100 * 0.006 (nDevReward = Total Profit * 0.006).
        nBetBlockPayoutAmount = 1440;                                   // Set the number of blocks we want to look back for results already paid out.
        nMinBetPayoutRange = 25;                                        // Spam filter to prevent malicious actors congesting the chain (Only payout bets that are between 25 - 10000 WRG inclusive).
        nMaxBetPayoutRange = 10000;                                     // Minimizes maximum payout size to avoid unnecessary large numbers (Only payout bets that are between 25 - 10000 WRG inclusive).
        nMaxParlayBetPayoutRange = 4000;                                // Minimizes maximum parlay payout size to avoid unnecessary large numbers (Only payout parlay bets that are between 25 - 4000 WRG inclusive).
        nBetPlaceTimeoutBlocks = 120;                                   // Discard bets placed less than 120 seconds (approx. 2 mins) before event start time,
        nMaxParlayLegs = 5;                                             // Minimizes maximum legs in parlay bet
        nWagerrProtocolV1StartHeight = 1100;                            // Betting protocol v1 activation block
        nWagerrProtocolV2StartHeight = 1100;                            // Betting protocol v2 activation block
        nWagerrProtocolV3StartHeight = 2000;                            // Betting protocol v3 activation block
        nQuickGamesEndHeight = 101650;
        nMaturityV2StartHeight = 38000;                                 // Reduced block maturity required for spending coinstakes and betting payouts

        nKeysRotateHeight = 102000;                                     // Rotate spork key, oracle keys and fee payout keys
        nEnforceNewSporkKey = 1605790800; //!> Sporks signed after Thursday, 19-Nov-20 13:00:00 UTC
        nRejectOldSporkKey = 1605963600; //!> Reject old spork key after Saturday, 21-Nov-20 13:00:00 UTC

        strSporkPubKey = "04d23d4179050244bfeff9f03ab4117e79a8835a9c0aba21b6df8d9e31042cc3b76bcb323a6e3a0e87b801ba2beef2c1db3a2a93d62bdb2e10192d8807f27e6f33";
        strSporkPubKeyOld = "0466223434350e5754c7379008e82954820a4bcc17335c42b915a0223c486e8bbbf87ba6281777d19ec73dc0b43416b33df432e3f4685770e56f9688afec7c2e3c";

        strDevPayoutAddrOld = "TLceyDrdPLBu8DK6UZjKu4vCDUQBGPybcY";     // Development fund payout address (Testnet).
        strDevPayoutAddrNew = "sUihJctn8P4wDVRU3SgSYbJkG8ajV68kmx";     // Development fund payout address (Testnet).
        strOMNOPayoutAddrOld = "TDunmyDASGDjYwhTF3SeDLsnDweyEBpfnP";    // OMNO fund payout address (Testnet).
        strOMNOPayoutAddrNew = "sMF9ejP1QMcoQnzURrSenRrFMznCfQfWgd";    // OMNO fund payout address (Testnet).

        vOracles = {
            { "TGFKr64W3tTMLZrKBhMAou9wnQmdNMrSG2", strDevPayoutAddrOld, strOMNOPayoutAddrOld, nWagerrProtocolV2StartHeight, nKeysRotateHeight },
            { "TWM5BQzfjDkBLGbcDtydfuNcuPfzPVSEhc", strDevPayoutAddrOld, strOMNOPayoutAddrOld, nWagerrProtocolV2StartHeight, nKeysRotateHeight },
            { "TRNjH67Qfpfuhn3TFonqm2DNqDwwUsJ24T", strDevPayoutAddrNew, strOMNOPayoutAddrNew, nKeysRotateHeight, std::numeric_limits<int>::max() },
            { "TYijVoyFnJ8dt1SGHtMtn2wa34CEs8EVZq", strDevPayoutAddrNew, strOMNOPayoutAddrNew, nKeysRotateHeight, std::numeric_limits<int>::max() },
        };

        quickGamesArr.clear();
        quickGamesArr.emplace_back(
            std::string("Dice"), // Game name
            QuickGamesType::qgDice, // game type
            &quickgames::DiceHandler, // game bet handler
            &quickgames::DiceBetInfoParser, // bet info parser
            std::string("TLceyDrdPLBu8DK6UZjKu4vCDUQBGPybcY"), // Dev address
            400, // OMNO reward permille (40%)
            100); // Dev reward permille (10%)

        // Fake Serial Attack
        nFakeSerialBlockheightEnd = -1;
        nSupplyBeforeFakeSerial = 0;

        // workarond fixes
        nSkipBetValidationStart = 5577;
        nSkipBetValidationEnd = 35619;

        //! Modify the testnet genesis block so the timestamp is valid for a later start.
        genesis.nTime = 1518696182;
        genesis.nNonce = 75183976;

        hashGenesisBlock = genesis.GetHash();
        assert(hashGenesisBlock == uint256("0x00000fdc268f54ff1368703792dc046b1356e60914c2b5b6348032144bcb2de5"));

        vFixedSeeds.clear();
        vSeeds.clear();
        vSeeds.push_back(CDNSSeedData("testnet-seeder-01.wgr.host", "testnet.testnet-seeder-01.wgr.host"));
        vSeeds.push_back(CDNSSeedData("testnet-seeder-02.wgr.host", "testnet.testnet-seeder-02.wgr.host"));

        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1, 65);   // Testnet wagerr addresses start with 'y'
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1, 125);  // Testnet wagerr script addresses start with '8' or '9'
        base58Prefixes[SECRET_KEY] = std::vector<unsigned char>(1, 177);     // Testnet private keys start with '9' or 'c' (Bitcoin defaults)
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
        fRequireStandard = true;
        fMineBlocksOnDemand = false;
        fTestnetToBeDeprecatedFieldRPC = true;

        nPoolMaxTransactions = 2;
        nBudgetCycleBlocks = 144; //!< Ten cycles per day on testnet
        strObfuscationPoolDummyAddress = "TMPUBzcsHZawA32XYYDF9FHQp6icv492CV";
        nStartMasternodePayments = 1518696183; // GMT: Thursday, 15. February 2018 12:03:03
        nBudget_Fee_Confirmations = 3; // Number of confirmations for the finalization fee. We have to make this very short
                                       // here because we only have a 8 block finalization window on testnet
        nProposalEstablishmentTime = 60 * 5; // Proposals must be at least 5 mns old to make it into a test budget
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
        pchMessageStart[0] = 0x12;
        pchMessageStart[1] = 0x76;
        pchMessageStart[2] = 0xa1;
        pchMessageStart[3] = 0xfa;
        nDefaultPort = 55006;
        nSubsidyHalvingInterval = 150;
        nEnforceBlockUpgradeMajority = 750;
        nRejectBlockOutdatedMajority = 950;
        nToCheckBlockUpgradeMajority = 1000;
        nMinerThreads = 1;
        bnProofOfWorkLimit = ~uint256(0) >> 1;
        nLastPOWBlock = 250;
        nMaturityV1 = 100;
        nMaturityV2 = 60;
        nStakeMinAge = 0;
        nStakeMinDepth = 0;
        nMasternodeCountDrift = 4;
        nModifierUpdateBlock = 0;
        nMaxMoneyOut = 398360470 * COIN;
        nZerocoinStartHeight = 300;
        nBlockZerocoinV2 = 300;
        nZerocoinStartTime = 1518696283;
        nBlockEnforceSerialRange = 1;               //Enforce serial range starting this block
        nBlockRecalculateAccumulators = 999999999;  //Trigger a recalculation of accumulators
        nBlockFirstFraudulent = 999999999;          //First block that bad serials emerged
        nBlockLastGoodCheckpoint = 999999999;       //Last valid accumulator checkpoint

        /** Block height at which BIP34 becomes active */
        nBIP34Height = 1;
        // Start enforcing CHECKLOCKTIMEVERIFY (BIP65) rule
        nBIP65Height = 1;

        nBlockStakeModifierV2 = 400;
        nBlockTimeProtocolV2 = 500;

        nMintRequiredConfirmations = 10;
        nZerocoinRequiredStakeDepth = nMintRequiredConfirmations;

        // Public coin spend enforcement
        nPublicZCSpends = 350;

        // Blocks v7
        nBlockV7StartHeight = nBlockZerocoinV2;
        nBlockLastAccumulatorCheckpoint = nBlockZerocoinV2-1; // no accumul. checkpoints check on regtest

        // New P2P messages signatures
        nBlockEnforceNewMessageSignatures = 1;

        /** Bet related parameters **/
        nBetBlocksIndexTimespanV2 = 2880;                               // Checking back 2 days for events and bets for each result.
        nBetBlocksIndexTimespanV3 = 23040;                              // Checking back 2 weeks for events and bets for each result.  (With approx. 2 days buffer).
        nOMNORewardPermille = 24;                                       // profitAcc / (100-6) * 100 * 0.024 (nMNBetReward = Total Profit * 0.024).
        nDevRewardPermille = 6;                                         // profitAcc / (100-6) * 100 * 0.006 (nDevReward = Total Profit * 0.006).
        nBetBlockPayoutAmount = 1440;                                   // Set the number of blocks we want to look back for results already paid out.
        nMinBetPayoutRange = 25;                                        // Spam filter to prevent malicious actors congesting the chain (Only payout bets that are between 25 - 10000 WRG inclusive).
        nMaxBetPayoutRange = 10000;                                     // Minimizes maximum payout size to avoid unnecessary large numbers (Only payout bets that are between 25 - 10000 WRG inclusive).
        nMaxParlayBetPayoutRange = 4000;                                // Minimizes maximum parlay payout size to avoid unnecessary large numbers (Only payout parlay bets that are between 25 - 4000 WRG inclusive).
        nBetPlaceTimeoutBlocks = 120;                                   // Discard bets placed less than 120 seconds (approx. 2 mins) before event start time,
        nMaxParlayLegs = 5;                                             // Minimizes maximum legs in parlay bet
        nWagerrProtocolV1StartHeight = 251;                             // Betting protocol v1 activation block
        nWagerrProtocolV2StartHeight = 251;                             // Betting protocol v2 activation block
        nWagerrProtocolV3StartHeight = 300;                             // Betting protocol v3 activation block
        nQuickGamesEndHeight = nWagerrProtocolV3StartHeight;
        nMaturityV2StartHeight = nWagerrProtocolV3StartHeight;          // Reduced block maturity required for spending coinstakes and betting payouts

        nKeysRotateHeight = 270;                                        // Rotate spork key, oracle keys and fee payout keys

        /* Spork Key for RegTest:
        WIF private key: 6xLZdACFRA53uyxz8gKDLcgVrm5kUUEu2B3BUzWUxHqa2W7irbH
        private key hex: a792662ff7b4cca1603fb9b67a4bce9e8ffb9718887977a5a0b2a522e3eab97e
        */
        strSporkPubKey = "04fb58d71ffcf7d5385d85020045f108637a5296d1552f56ce561a29f78834101519c306be7d9c328a82b77726038b0eab01b8f3187c76656f9996257bf616e77f";
        strSporkPubKeyOld = "04fb58d71ffcf7d5385d85020045f108637a5296d1552f56ce561a29f78834101519c306be7d9c328a82b77726038b0eab01b8f3187c76656f9996257bf616e77f";

        strDevPayoutAddrOld = "TLuTVND9QbZURHmtuqD5ESECrGuB9jLZTs";     // Development fund payout address (Regtest).
        strDevPayoutAddrNew = "TLuTVND9QbZURHmtuqD5ESECrGuB9jLZTs";     // Development fund payout address (Regtest).
        strOMNOPayoutAddrOld = "THofaueWReDjeZQZEECiySqV9GP4byP3qr";    // OMNO fund payout address (Regtest).
        strOMNOPayoutAddrNew = "THofaueWReDjeZQZEECiySqV9GP4byP3qr";    // OMNO fund payout address (Regtest).

        vOracles = {
            { "TXuoB9DNEuZx1RCfKw3Hsv7jNUHTt4sVG1", strDevPayoutAddrOld, strOMNOPayoutAddrOld, nWagerrProtocolV2StartHeight, nKeysRotateHeight },
            { "TFvZVYGdrxxNunQLzSnRSC58BSRA7si6zu", strDevPayoutAddrOld, strOMNOPayoutAddrOld, nWagerrProtocolV2StartHeight, nKeysRotateHeight },
            { "TXuoB9DNEuZx1RCfKw3Hsv7jNUHTt4sVG1", strDevPayoutAddrNew, strOMNOPayoutAddrNew, nKeysRotateHeight, std::numeric_limits<int>::max() },
            { "TFvZVYGdrxxNunQLzSnRSC58BSRA7si6zu", strDevPayoutAddrNew, strOMNOPayoutAddrNew, nKeysRotateHeight, std::numeric_limits<int>::max() },
        };

        quickGamesArr.clear();
        quickGamesArr.emplace_back(
            std::string("Dice"), // Game name
            QuickGamesType::qgDice, // game type
            &quickgames::DiceHandler, // game bet handler
            &quickgames::DiceBetInfoParser, // bet info parser
            std::string("TLuTVND9QbZURHmtuqD5ESECrGuB9jLZTs"), // Dev address
            400, // OMNO reward permille (40%)
            100); // Dev reward permille (10%)

        // Fake Serial Attack
        nFakeSerialBlockheightEnd = -1;

        //! Modify the regtest genesis block so the timestamp is valid for a later start.
        genesis.nTime = 1518696183;             // GMT: Thursday, 15. February 2018 12:03:03
        genesis.nBits = 0x207fffff;
        genesis.nNonce = 574752;                // hex 57 47 52 in text = WG

        hashGenesisBlock = genesis.GetHash();
        assert(hashGenesisBlock == uint256("0x671d0510c128608897d98d1819d26b40810c8b7e4901447a909c87a9edc2f5ec"));

        vFixedSeeds.clear(); //! Testnet mode doesn't have any fixed seeds.
        vSeeds.clear();      //! Testnet mode doesn't have any DNS seeds.

        fMiningRequiresPeers = false;
        fAllowMinDifficultyBlocks = true;
        fDefaultConsistencyChecks = true;
        fRequireStandard = false;
        fMineBlocksOnDemand = true;
        fSkipProofOfWorkCheck = true;
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
        nDefaultPort = 55008;
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
