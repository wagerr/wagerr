// Copyright (c) 2018 The Wagerr developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "bet.h"
#include <boost/filesystem.hpp>

#include "wallet/wallet.h"

#define BTX_FORMAT_VERSION 0x01
#define BTX_HEX_PREFIX "42"

// String lengths for all currently supported op codes.
#define PE_OP_STRLEN  74
#define PB_OP_STRLEN  16
#define PR_OP_STRLEN  24
#define PUO_OP_STRLEN 38
#define CGE_OP_STRLEN 14
#define CGB_OP_STRLEN 10
#define CGR_OP_STRLEN 10
#define PSE_OP_STRLEN 34
#define PTE_OP_STRLEN 34


/**
 * Validate the transaction to ensure it has been posted by an oracle node.
 *
 * @param txin  TX vin input hash.
 * @return      Bool
 */
bool IsValidOracleTx(const CTxIn &txin)
{
    COutPoint prevout = txin.prevout;
    std::vector<string> oracleAddrs = Params().OracleWalletAddrs();

    uint256 hashBlock;
    CTransaction txPrev;
    if (GetTransaction(prevout.hash, txPrev, hashBlock, true)) {

        const CTxOut &prevTxOut = txPrev.vout[prevout.n];
        std::string scriptPubKey = prevTxOut.scriptPubKey.ToString();

        txnouttype type;
        vector<CTxDestination> prevAddrs;
        int nRequired;

        if (ExtractDestinations(prevTxOut.scriptPubKey, type, prevAddrs, nRequired)) {
            BOOST_FOREACH (const CTxDestination &prevAddr, prevAddrs) {
                if (std::find(oracleAddrs.begin(), oracleAddrs.end(), CBitcoinAddress(prevAddr).ToString()) != oracleAddrs.end()) {
                    return true;
                }
            }
        }
    }

    return false;
}

/**
 * Takes a payout vector and aggregates the total WGR that is required to pay out all bets.
 * We also calculate and add the OMNO and dev fund rewards.
 *
 * @param vExpectedPayouts  A vector containing all the winning bets that need to be paid out.
 * @param nMNBetReward  The Oracle masternode reward.
 * @return
 */
int64_t GetBlockPayouts(std::vector<CBetOut>& vExpectedPayouts, CAmount& nMNBetReward)
{
    CAmount profitAcc = 0;
    CAmount nPayout = 0;
    CAmount totalAmountBet = 0;

    // Set the OMNO and Dev reward addresses
    std::string devPayoutAddr  = Params().DevPayoutAddr();
    std::string OMNOPayoutAddr = Params().OMNOPayoutAddr();

    // Loop over the payout vector and aggregate values.
    for (unsigned i = 0; i < vExpectedPayouts.size(); i++) {
        CAmount betValue = vExpectedPayouts[i].nBetValue;
        CAmount payValue = vExpectedPayouts[i].nValue;

        totalAmountBet += betValue;
        profitAcc += payValue - betValue;
        nPayout += payValue;
    }

    if (vExpectedPayouts.size() > 0) {
        // Calculate the OMNO reward and the Dev reward.
        CAmount nOMNOReward = (CAmount)(profitAcc * Params().OMNORewardPermille() / (1000.0 - Params().BetXPermille()));
        CAmount nDevReward  = (CAmount)(profitAcc * Params().DevRewardPermille() / (1000.0 - Params().BetXPermille()));

        // Add both reward payouts to the payout vector.
        vExpectedPayouts.emplace_back(nDevReward, GetScriptForDestination(CBitcoinAddress(devPayoutAddr).Get()));
        vExpectedPayouts.emplace_back(nOMNOReward, GetScriptForDestination(CBitcoinAddress(OMNOPayoutAddr).Get()));

        nPayout += nDevReward + nOMNOReward;
    }

    return  nPayout;
}

/**
 * Takes a payout vector and aggregates the total WGR that is required to pay out all CGLotto bets.
 *
 * @param vexpectedCGPayouts  A vector containing all the winning bets that need to be paid out.
 * @param nMNBetReward  The Oracle masternode reward.
 * @return
 */
int64_t GetCGBlockPayouts(std::vector<CBetOut>& vexpectedCGPayouts, CAmount& nMNBetReward)
{
    CAmount nPayout = 0;

    for (unsigned i = 0; i < vexpectedCGPayouts.size(); i++) {
        CAmount payValue = vexpectedCGPayouts[i].nValue;
        nPayout += payValue;
    }

    return  nPayout;
}

/**
 * Validates the payout block to ensure all bet payout amounts and payout addresses match their expected values.
 *
 * @param vExpectedPayouts -  The bet payout vector.
 * @param nHeight - The current chain height.
 * @return
 */
bool IsBlockPayoutsValid(std::vector<CBetOut> vExpectedPayouts, CBlock block)
{
    unsigned long size = vExpectedPayouts.size();

    // If we have payouts to validate.
    if (size > 0) {

        CTransaction &tx = block.vtx[1];

        // Get the vin staking value so we can use it to find out how many staking TX in the vouts.
        const CTxIn &txin         = tx.vin[0];
        COutPoint prevout         = txin.prevout;
        unsigned int numStakingTx = 0;
        CAmount stakeAmount       = 0;

        uint256 hashBlock;
        CTransaction txPrev;

        if (GetTransaction(prevout.hash, txPrev, hashBlock, true)) {
            const CTxOut &prevTxOut = txPrev.vout[prevout.n];
            stakeAmount = prevTxOut.nValue;
        }

        // Count the coinbase and staking vouts in the current block TX.
        CAmount totalStakeAcc = 0;
        for (unsigned int i = 0; i < tx.vout.size(); i++) {
            const CTxOut &txout = tx.vout[i];
            CAmount voutValue   = txout.nValue;

            if (totalStakeAcc < stakeAmount) {
                numStakingTx++;
            }
            else {
                break;
            }

            totalStakeAcc += voutValue;
        }

        // Validate the payout block against the expected payouts vector. If all payout amounts and payout addresses match then we have a valid payout block.
        for (unsigned int i = numStakingTx; i < tx.vout.size() - 1; i++) {
            const CTxOut &txout = tx.vout[i];
            CAmount voutValue   = txout.nValue;
            CAmount vExpected   = vExpectedPayouts[i - numStakingTx].nValue;

            LogPrintf("Bet Amount %li  - Expected Bet Amount: %li \n", voutValue, vExpected);

            // Get the bet payout address.
            CTxDestination betAddr;
            ExtractDestination(tx.vout[i].scriptPubKey, betAddr);
            std::string betAddrS = CBitcoinAddress(betAddr).ToString();

            // Get the expected payout address.
            CTxDestination expectedAddr;
            ExtractDestination(vExpectedPayouts[i - numStakingTx].scriptPubKey, expectedAddr);
            std::string expectedAddrS = CBitcoinAddress(expectedAddr).ToString();

            LogPrintf("Bet Address %s  - Expected Bet Address: %s \n", betAddrS.c_str(), expectedAddrS.c_str());

            if (vExpected != voutValue && betAddrS != expectedAddrS) {
                LogPrintf("Validation failed! \n");
                return false;
            }
        }
    }

    return true;
}

/**
 * `ReadBTXFormatVersion` returns -1 if the `opCode` doesn't begin with a valid "BTX" prefix.
 *
 * @param opCode The OpCode as a string
 * @return       The protocal version number
 */
int ReadBTXFormatVersion(std::string opCode)
{
    // Check the first three bytes match the "BTX" format specification.
    if (opCode[0] != 'B') {
        return -1;
    }

    // Check the BTX protocol version number is in range.
    int v = opCode[1];

    // Versions outside the range [1, 254] are not supported.
    return v < 1 || v > 254 ? -1 : v;
}

/**
 * Convert the hex chars for 4 bytes of opCode into uint32_t integer value.
 *
 * @param a First hex char
 * @param b Second hex char
 * @param c Third hex char
 * @param d Fourth hex char
 * @return  32 bit unsigned integer
 */
uint32_t FromChars(unsigned char a, unsigned char b, unsigned char c, unsigned char d)
{
    uint32_t n = d;
    n <<= 8;
    n += c;
    n <<= 8;
    n += b;
    n <<= 8;
    n += a;

    return n;
}

/**
 * Convert the hex chars for 2 bytes of opCode into uint32_t integer value.
 *
 * @param a First hex char
 * @param b Second hex char
 * @return  32 bit unsigned integer
 */
uint32_t FromChars(unsigned char a, unsigned char b)
{
    uint32_t n = b;
    n <<= 8;
    n += a;

    return n;
}

/**
 * Convert the hex chars for 1 byte of opCode into uint32_t integer value.
 *
 * @param a First hex char
 * @return  32 bit unsigned integer
 */
uint32_t FromChars(unsigned char a)
{
    uint32_t n = a;

    return n;
}

inline uint16_t swap_endianness_16(uint16_t x)
{
    return (x >> 8) | (x << 8);
}
inline uint32_t swap_endianness_32(uint32_t x)
{
    return (((x & 0xff000000U) >> 24) | ((x & 0x00ff0000U) >>  8) |
            ((x & 0x0000ff00U) <<  8) | ((x & 0x000000ffU) << 24));
}
/**
 * Convert a unsigned 32 bit integer into its hex equivalent with the
 * amount of zero padding given as argument length.
 *
 * @param value  The integer value
 * @param length The size in nr of hex characters
 * @return       Hex string
 */
std::string ToHex(uint32_t value, int length)
{
    std::stringstream strBuffer;
    if (length == 2){
        strBuffer << std::hex << std::setw(length) << std::setfill('0') << value;
    } else if (length == 4){
        uint16_t be_value = (uint16_t)value;
        uint16_t le_value = swap_endianness_16(be_value);
        strBuffer << std::hex << std::setw(length) << std::setfill('0') << le_value;
    } else if (length == 8){
        uint32_t le_value = swap_endianness_32(value);
        strBuffer << std::hex << std::setw(length) << std::setfill('0') << le_value;
    }
    return strBuffer.str();
}

/**
 * Split a CPeerlessEvent OpCode string into byte components and store in peerless
 * event object.
 *
 * @param opCode  The CPeerlessEvent OpCode string
 * @param pe      The CPeerlessEvent object
 * @return        Bool
 */
bool CPeerlessEvent::FromOpCode(std::string opCode, CPeerlessEvent &pe)
{
    // Ensure peerless event OpCode string is the correct length.
    if (opCode.length() != PE_OP_STRLEN / 2) {
        // TODO - add proper error handling
        return false;
    }

    // Ensure the peerless event transaction type is correct.
    if (opCode[2] != plEventTxType) {
        // TODO - add proper error handling
        return false;
    }

    // Ensure the peerless event OpCode has the correct BTX format version number.
    if (ReadBTXFormatVersion(opCode) != BTX_FORMAT_VERSION) {
        // TODO - add proper error handling
        return false;
    }

    // Parse the OPCODE hex data.
    pe.nEventId    = FromChars(opCode[3], opCode[4], opCode[5], opCode[6]);
    pe.nStartTime  = FromChars(opCode[7], opCode[8], opCode[9], opCode[10]);
    pe.nSport      = FromChars(opCode[11], opCode[12]);
    pe.nTournament = FromChars(opCode[13], opCode[14]);
    pe.nStage      = FromChars(opCode[15], opCode[16]);
    pe.nHomeTeam   = FromChars(opCode[17], opCode[18], opCode[19], opCode[20]);
    pe.nAwayTeam   = FromChars(opCode[21], opCode[22], opCode[23], opCode[24]);
    pe.nHomeOdds   = FromChars(opCode[25], opCode[26], opCode[27], opCode[28]);
    pe.nAwayOdds   = FromChars(opCode[29], opCode[30], opCode[31], opCode[32]);
    pe.nDrawOdds   = FromChars(opCode[33], opCode[34], opCode[35], opCode[36]);

    // Set default values for the spread and total market odds as we don't know them yet.
    pe.nSpreadPoints    = 0;
    pe.nSpreadHomeOdds  = 0;
    pe.nSpreadAwayOdds  = 0;
    pe.nTotalPoints     = 0;
    pe.nTotalOverOdds   = 0;
    pe.nTotalUnderOdds  = 0;

    // Set default values for the spread, moneyline and totals potantial liability accumulators
    pe.nMoneyLineHomePotentialLiability = 0;
    pe.nMoneyLineAwayPotentialLiability = 0;
    pe.nMoneyLineDrawPotentialLiability = 0;
    pe.nSpreadHomePotentialLiability    = 0;
    pe.nSpreadAwayPotentialLiability    = 0;
    pe.nSpreadPushPotentialLiability    = 0;
    pe.nTotalOverPotentialLiability     = 0;
    pe.nTotalUnderPotentialLiability    = 0;
    pe.nTotalPushPotentialLiability     = 0;

    // Set default values for the spread, moneyline and totals bet accumulators
    pe.nMoneyLineHomeBets = 0;
    pe.nMoneyLineAwayBets = 0;
    pe.nMoneyLineDrawBets = 0;
    pe.nSpreadHomeBets    = 0;
    pe.nSpreadAwayBets    = 0;
    pe.nSpreadPushBets    = 0;
    pe.nTotalOverBets     = 0;
    pe.nTotalUnderBets    = 0;
    pe.nTotalPushBets     = 0;

    return true;
}

/**
 * Convert CPeerlessEvent object data into hex OPCode string.
 *
 * @param pe     The CPeerlessEvent object
 * @param opCode The CPeerlessEvent OpCode string
 * @return       Bool
 */
bool CPeerlessEvent::ToOpCode(CPeerlessEvent pe, std::string &opCode)
{
    std::string sEventId    = ToHex(pe.nEventId, 8);
    std::string sStartTime  = ToHex(pe.nStartTime,  8);
    std::string sSport      = ToHex(pe.nSport, 4);
    std::string sTournament = ToHex(pe.nTournament, 4);
    std::string sStage      = ToHex(pe.nStage, 4);
    std::string sHomeTeam   = ToHex(pe.nHomeTeam, 8);
    std::string sAwayTeam   = ToHex(pe.nAwayTeam, 8);
    std::string sHomeOdds   = ToHex(pe.nHomeOdds, 8);
    std::string sAwayOdds   = ToHex(pe.nAwayOdds, 8);
    std::string sDrawOdds   = ToHex(pe.nDrawOdds, 8);

    opCode = BTX_HEX_PREFIX "0102" + sEventId + sStartTime + sSport + sTournament +
             sStage + sHomeTeam + sAwayTeam + sHomeOdds + sAwayOdds + sDrawOdds;

    // Ensure peerless Event OpCode string is the correct length.
    if (opCode.length() != PE_OP_STRLEN) {
        // TODO - add proper error handling
        return false;
    }

    return true;
}

/**
 * Split a CPeerlessBet OpCode string into byte components and store in peerless
 * bet object.
 *
 * @param opCode The CPeerlessBet OpCode string
 * @param pe     The CPeerlessBet object
 * @return       Bool
 */
bool CPeerlessBet::FromOpCode(std::string opCode, CPeerlessBet &pb)
{
    // Ensure peerless bet OpCode string is the correct length.
    if (opCode.length() != PB_OP_STRLEN / 2) {
        // TODO - add proper error handling
        return false;
    }

    // Ensure the peerless bet transaction type is correct.
    if (opCode[2] != plBetTxType) {
        // TODO - add proper error handling
        return false;
    }

    // Ensure the peerless bet OpCode has the correct BTX format version number.
    if (ReadBTXFormatVersion(opCode) != BTX_FORMAT_VERSION) {
        // TODO - add proper error handling
        return false;
    }

    pb.nEventId = FromChars(opCode[3], opCode[4], opCode[5], opCode[6]);
    pb.nOutcome = (OutcomeType) opCode[7];

    return true;
}

/**
 * Convert CPeerlessBet object data into hex OPCode string.
 *
 * @param pb     The CPeerlessBet object
 * @param opCode The CPeerlessBet OpCode string
 * @return       Bool
 */
bool CPeerlessBet::ToOpCode(CPeerlessBet pb, std::string &opCode)
{
    std::string sEventId = ToHex(pb.nEventId, 8);
    std::string sOutcome = ToHex(pb.nOutcome, 2);

    opCode = BTX_HEX_PREFIX "0103" + sEventId + sOutcome;

    // Ensure peerless bet OpCode string is the correct length.
    if (opCode.length() != PB_OP_STRLEN) {
        // TODO - add proper error handling
        return false;
    }

    return true;
}

/**
 * Split a PeerlessResult OpCode string into byte components and store in peerless
 * result object.
 *
 * @param opCode The CPeerlessResult OpCode string
 * @param pr     The CPeerlessResult object
 * @return       Bool
 */
bool CPeerlessResult::FromOpCode(std::string opCode, CPeerlessResult &pr)
{
    // Ensure peerless result OpCode string is the correct length.
    if (opCode.length() != PR_OP_STRLEN / 2) {
        // TODO - add proper error handling
        return false;
    }

    // Ensure the peerless result transaction type is correct.
    if (opCode[2] != plResultTxType) {
        // TODO - add proper error handling
        return false;
    }

    // Ensure the peerless result OpCode has the correct BTX format version number.
    if (ReadBTXFormatVersion(opCode) != BTX_FORMAT_VERSION) {
        // TODO - add proper error handling
        return false;
    }

    pr.nEventId         = FromChars(opCode[3], opCode[4], opCode[5], opCode[6]);
    pr.nResultType      = FromChars (opCode[7]);
    pr.nHomeScore       = FromChars (opCode[8], opCode[9]);
    pr.nAwayScore       = FromChars (opCode[10], opCode[11]);

    return true;
}

/**
 * Convert CPeerlessResult object data into hex OPCode string.
 *
 * @param pr     The CPeerlessResult Object
 * @param opCode The CPeerlessResult OpCode string
 * @return       Bool
 */
bool CPeerlessResult::ToOpCode(CPeerlessResult pr, std::string &opCode)
{
    std::string sEventId        = ToHex(pr.nEventId, 8);
    std::string sResultType     = ToHex(pr.nResultType, 2);
    std::string sHomeScore      = ToHex(pr.nHomeScore, 4);
    std::string sAwayScore      = ToHex(pr.nAwayScore, 4);


    opCode = BTX_HEX_PREFIX "0104" + sEventId + sResultType + sHomeScore + sAwayScore;

    // Ensure peerless result OpCode string is the correct length.
    if (opCode.length() != PR_OP_STRLEN) {
        // TODO - add proper error handling
        return false;
    }

    return true;
}

/**
 * Split a PeerlessUpdateOdds OpCode string into byte components and store in peerless
 * update odds object.
 *
 * @param opCode The CPeerlessUpdateOdds OpCode string
 * @param puo    The CPeerlessUpdateOdds object
 * @return       Bool
 */
bool CPeerlessUpdateOdds::FromOpCode(std::string opCode, CPeerlessUpdateOdds &puo)
{
    // Ensure peerless update odds OpCode string is the correct length.
    if (opCode.length() != PUO_OP_STRLEN / 2) {
        // TODO - add proper error handling
        return false;
    }

    // Ensure the peerless update odds transaction type is correct.
    if (opCode[2] != plUpdateOddsTxType) {
        // TODO - add proper error handling
        return false;
    }

    // Ensure the peerless update odds OpCode has the correct BTX format version number.
    if (ReadBTXFormatVersion(opCode) != BTX_FORMAT_VERSION) {
        // TODO - add proper error handling
        return false;
    }

    puo.nEventId  = FromChars(opCode[3], opCode[4], opCode[5], opCode[6]);
    puo.nHomeOdds = FromChars(opCode[7], opCode[8], opCode[9], opCode[10]);
    puo.nAwayOdds = FromChars(opCode[11], opCode[12], opCode[13], opCode[14]);
    puo.nDrawOdds = FromChars(opCode[15], opCode[16], opCode[17], opCode[18]);

    return true;
}

/**
 * Convert CPeerlessUpdateOdds object data into hex OPCode string.
 *
 * @param pup    The CPeerlessUpdateOdds Object
 * @param opCode The CPeerlessUpdateOdds OpCode string
 * @return       Bool
 */
bool CPeerlessUpdateOdds::ToOpCode(CPeerlessUpdateOdds puo, std::string &opCode)
{
    std::string sEventId  = ToHex(puo.nEventId, 8);
    std::string sHomeOdds = ToHex(puo.nHomeOdds, 8);
    std::string sAwayOdds = ToHex(puo.nAwayOdds, 8);
    std::string sDrawOdds = ToHex(puo.nDrawOdds, 8);

    opCode = BTX_HEX_PREFIX "0105" + sEventId + sHomeOdds +  sAwayOdds + sDrawOdds;

    // Ensure peerless update odds OpCode string is the correct length.
    if (opCode.length() != PUO_OP_STRLEN) {
        // TODO - add proper error handling
        return false;
    }

    return true;
}

/**
 * Updates a peerless event object with new money line odds.
 */
void SetEventMLOdds (CPeerlessUpdateOdds puo) {
    CEventDB edb;
    eventIndex_t eventIndex;
    edb.GetEvents(eventIndex);

    // First check a peerless event exists in the event index.
    if (eventIndex.count(puo.nEventId) > 0) {

        // Get the event object from the index and update the money line odds values.
        CPeerlessEvent plEvent = eventIndex.find(puo.nEventId)->second;

        plEvent.nHomeOdds = puo.nHomeOdds;
        plEvent.nAwayOdds = puo.nAwayOdds;
        plEvent.nDrawOdds = puo.nDrawOdds;

        // Update the event in the event index.
        eventIndex[puo.nEventId] = plEvent;
        CEventDB::SetEvents(eventIndex);
    }
}

/**
 * Split a CChainGamesEvent OpCode string into byte components and store in chain games
 * result object.
 *
 * @param opCode The CChainGamesEvent OpCode string
 * @param cge    The CChainGamesEvent object
 * @return       Bool
 */
bool CChainGamesEvent::FromOpCode(std::string opCode, CChainGamesEvent &cge)
{
    // Ensure chain game event OpCode string is the correct length.
    if (opCode.length() != CGE_OP_STRLEN / 2) {
        // TODO - add proper error handling
        return false;
    }

    // Ensure the chain game event transaction type is correct.
    if (opCode[2] != cgEventTxType) {
        // TODO - add proper error handling
        return false;
    }

    // Ensure the chain game event OpCode has the correct BTX format version number.
    if (ReadBTXFormatVersion(opCode) != BTX_FORMAT_VERSION) {
        // TODO - add proper error handling
        return false;
    }

    cge.nEventId  = FromChars(opCode[3], opCode[4]);
    cge.nEntryFee = FromChars(opCode[5], opCode[6]);


    return true;
}

/**
 * Convert CChainGamesEvent object data into hex OPCode string.
 *
 * @param cge    The CChainGamesEvent object
 * @param opCode The CChainGamesEvent OpCode string
 * @return       Bool
 */
bool CChainGamesEvent::ToOpCode(CChainGamesEvent cge, std::string &opCode)
{
    std::string sEventId  = ToHex(cge.nEventId, 4);
    std::string sEntryFee = ToHex(cge.nEntryFee, 4);

    opCode = BTX_HEX_PREFIX "0106" + sEventId + sEntryFee;

    // Ensure Chain Games event OpCode string is the correct length.
    if (opCode.length() != CGE_OP_STRLEN) {
        // TODO - add proper error handling
        return false;
    }

    return true;
}

/**
 * Split a CChainGamesBet OpCode string into byte components and store in chain games
 * bet object.
 *
 * @param opCode The CChainGamesBet OpCode string
 * @param cgb    The CChainGamesBet object
 * @return       Bool
 */
bool CChainGamesBet::FromOpCode(std::string opCode, CChainGamesBet &cgb)
{
    // Ensure chain game bet OpCode string is the correct length.
    if (opCode.length() != CGB_OP_STRLEN / 2) {
        // TODO - add proper error handling
        return false;
    }

    // Ensure the chain game bet transaction type is correct.
    if (opCode[2] != cgBetTxType) {
        // TODO - add proper error handling
        return false;
    }

    // Ensure the chain game bet OpCode has the correct BTX format version number.
    if (ReadBTXFormatVersion(opCode) != BTX_FORMAT_VERSION) {
        // TODO - add proper error handling
        return false;
    }

    cgb.nEventId = FromChars(opCode[3], opCode[4]);

    return true;
}

/**
 * Convert CChainGamesBet object data into hex OPCode string.
 *
 * @param cgb    The CChainGamesBet object
 * @param opCode The CChainGamesBet OpCode string
 * @return       Bool
 */
bool CChainGamesBet::ToOpCode(CChainGamesBet cgb, std::string &opCode)
{
    std::string sEventId  = ToHex(cgb.nEventId, 4);

    opCode = BTX_HEX_PREFIX "0107" + sEventId;

    // Ensure Chain Games bet OpCode string is the correct length.
    if (opCode.length() != CGB_OP_STRLEN) {
        // TODO - add proper error handling
        return false;
    }

    return true;
}

/**
 * Split a CChainGamesResult OpCode string into byte components and store in chain games
 * result object.
 *
 * @param opCode The CChainGamesResult OpCode string
 * @param cgr    The CChainGamesResult object
 * @return       Bool
 */
bool CChainGamesResult::FromOpCode(std::string opCode, CChainGamesResult &cgr)
{
    // Ensure chain game result OpCode string is the correct length.
    if (opCode.length() != CGR_OP_STRLEN / 2) {
        // TODO - add proper error handling
        return false;
    }

    // Ensure the chain game result transaction type is correct.
    if (opCode[2] != cgResultTxType) {
        // TODO - add proper error handling
        return false;
    }

    // Ensure the chain game result OpCode has the correct BTX format version number.
    if (ReadBTXFormatVersion(opCode) != BTX_FORMAT_VERSION) {
        // TODO - add proper error handling
        return false;
    }

    cgr.nEventId = FromChars(opCode[3], opCode[4]);

    return true;
}

/**
 * Convert CChainGamesResult object data into hex OPCode string.
 *
 * @param cgr    The CChainGamesResult object
 * @param opCode The CChainGamesResult OpCode string
 * @return       Bool
 */
bool CChainGamesResult::ToOpCode(CChainGamesResult cgr, std::string &opCode)
{
    std::string sEventId  = ToHex(cgr.nEventId, 4);

    opCode = BTX_HEX_PREFIX "0108" + sEventId;

    // Ensure Chain Games result OpCode string is the correct length.
    if (opCode.length() != CGR_OP_STRLEN) {
        // TODO - add proper error handling
        return false;
    }

    return true;
}


/**
 * Split a CPeerlessSpreadsEvent OpCode string into byte components and store in peerless spreads
 * event object.
 *
 * @param opCode The CPeerlessSpreadsEvent OpCode string
 * @param pse     The CPeerlessSpreadsEvent object
 * @return       Bool
 */
bool CPeerlessSpreadsEvent::FromOpCode(std::string opCode, CPeerlessSpreadsEvent &pse)
{
    // Ensure peerless result OpCode string is the correct length.
    if (opCode.length() != PSE_OP_STRLEN / 2) {
        // TODO - add proper error handling
        return false;
    }

    // Ensure the peerless result transaction type is correct.
    if (opCode[2] != plSpreadsEventTxType) {
        // TODO - add proper error handling
        return false;
    }

    // Ensure the peerless result OpCode has the correct BTX format version number.
    if (ReadBTXFormatVersion(opCode) != BTX_FORMAT_VERSION) {
        // TODO - add proper error handling
        return false;
    }

    pse.nEventId   = FromChars(opCode[3], opCode[4], opCode[5], opCode[6]);
    pse.nPoints    = FromChars(opCode[7], opCode[8]);
    pse.nHomeOdds  = FromChars(opCode[9], opCode[10], opCode[11], opCode[12]);
    pse.nAwayOdds  = FromChars(opCode[13], opCode[14], opCode[15], opCode[16]);

    return true;
}

/**
 * Convert CPeerlessSpreadsEvent object data into hex OPCode string.
 *
 * @param pse     The CPeerlessSpreadsEvent Object
 * @param opCode The CPeerlessSpreadsEvent OpCode string
 * @return       Bool
 */
bool CPeerlessSpreadsEvent::ToOpCode(CPeerlessSpreadsEvent pse, std::string &opCode)
{
    std::string sEventId  = ToHex(pse.nEventId, 8);
    std::string sPoints   = ToHex(pse.nPoints, 4);
    std::string sHomeOdds = ToHex(pse.nHomeOdds, 8);
    std::string sAwayOdds = ToHex(pse.nAwayOdds, 8);

    opCode = BTX_HEX_PREFIX "0109" + sEventId + sPoints + sHomeOdds + sAwayOdds;

    // Ensure peerless result OpCode string is the correct length.
    if (opCode.length() != PSE_OP_STRLEN) {
        // TODO - add proper error handling
        return false;
    }

    return true;
}

/**
 * Updates a peerless event object with spread odds for the given event ID.
 */
void SetEventSpreadOdds (CPeerlessSpreadsEvent spreadEvent) {
    CEventDB edb;
    eventIndex_t eventIndex;
    edb.GetEvents(eventIndex);

    // First check a peerless event exists in the event index.
    if (eventIndex.count(spreadEvent.nEventId) > 0) {

        // Get the event object from the index and update the spread odds values.
        CPeerlessEvent plEvent = eventIndex.find(spreadEvent.nEventId)->second;

        plEvent.nSpreadPoints    = spreadEvent.nPoints;
        plEvent.nSpreadHomeOdds  = spreadEvent.nHomeOdds;
        plEvent.nSpreadAwayOdds  = spreadEvent.nAwayOdds;

        // Update the event in the event index.
        eventIndex[spreadEvent.nEventId] = plEvent;
        CEventDB::SetEvents(eventIndex);
    }
}

/**
 * Split a CPeerlessTotalsEvent OpCode string into byte components and store in peerless totals
 * event object.
 *
 * @param opCode The CPeerlessTotalsEvent OpCode string
 * @param pte    The CPeerlessTotalsEvent object
 * @return       Bool
 */
bool CPeerlessTotalsEvent::FromOpCode(std::string opCode, CPeerlessTotalsEvent &pte)
{
    // Ensure peerless result OpCode string is the correct length.
    if (opCode.length() != PTE_OP_STRLEN / 2) {
        // TODO - add proper error handling
        return false;
    }

    // Ensure the peerless result transaction type is correct.
    if (opCode[2] != plTotalsEventTxType) {
        // TODO - add proper error handling
        return false;
    }

    // Ensure the peerless result OpCode has the correct BTX format version number.
    if (ReadBTXFormatVersion(opCode) != BTX_FORMAT_VERSION) {
        // TODO - add proper error handling
        return false;
    }

    pte.nEventId   = FromChars(opCode[3], opCode[4], opCode[5], opCode[6]);
    pte.nPoints    = FromChars(opCode[7], opCode[8]);
    pte.nOverOdds  = FromChars(opCode[9], opCode[10], opCode[11], opCode[12]);
    pte.nUnderOdds = FromChars(opCode[13], opCode[14], opCode[15], opCode[16]);

    return true;
}

/**
 * Convert CPeerlessCPeerlessTotalsEventSpreadEvent object data into hex OPCode string.
 *
 * @param pte    The CPeerlessTotalsEvent Object
 * @param opCode The CPeerlessTotalsEvent OpCode string
 * @return       Bool
 */
bool CPeerlessTotalsEvent::ToOpCode(CPeerlessTotalsEvent pte, std::string &opCode)
{
    std::string sEventId   = ToHex(pte.nEventId, 8);
    std::string sPoints    = ToHex(pte.nPoints, 4);
    std::string sOverOdds  = ToHex(pte.nOverOdds, 8);
    std::string sUnderOdds = ToHex(pte.nUnderOdds, 8);

    opCode = BTX_HEX_PREFIX "010a" + sEventId + sPoints + sOverOdds + sUnderOdds;

    // Ensure peerless result OpCode string is the correct length.
    if (opCode.length() != PTE_OP_STRLEN) {
        // TODO - add proper error handling
        return false;
    }

    return true;
}

/**
 * Updates a peerless event object with totals odds.
 */
void SetEventTotalOdds (CPeerlessTotalsEvent totalsEvent) {
    CEventDB edb;
    eventIndex_t eventIndex;
    edb.GetEvents(eventIndex);

    // First check a peerless event exists in the event index.
    if (eventIndex.count(totalsEvent.nEventId) > 0) {

        // Get the event object from the index and update the totals odds values.
        CPeerlessEvent plEvent = eventIndex.find(totalsEvent.nEventId)->second;

        plEvent.nTotalPoints    = totalsEvent.nPoints;
        plEvent.nTotalOverOdds  = totalsEvent.nOverOdds;
        plEvent.nTotalUnderOdds = totalsEvent.nUnderOdds;

        // Update the event in the event index.
        eventIndex[totalsEvent.nEventId] = plEvent;
        CEventDB::SetEvents(eventIndex);
    }
}


/**
 * Updates a peerless event object with total bet accumulators.
 */
void SetEventAccummulators (CPeerlessBet plBet, CAmount betAmount) {

    CEventDB edb;
    eventIndex_t eventsIndex;
    edb.GetEvents(eventsIndex);

    uint64_t oddsDivisor  = Params().OddsDivisor();
    uint64_t betXPermille = Params().BetXPermille();

    // Check the events index actually has events
    if (eventsIndex.size() > 0) {

        CPeerlessEvent pe = eventsIndex.find(plBet.nEventId)->second;
        CAmount payout = 0 * COIN;
        CAmount burn = 0;
        CAmount winnings = 0;

        // Check which outcome the bet was placed on and add to accumulators
        if (plBet.nOutcome == moneyLineWin){
            winnings = betAmount * pe.nHomeOdds;
            // To avoid internal overflow issues, first divide and then multiply.
            // This will not cause inaccuracy, because the Odds (and thus the winnings) are scaled by a
            // factor 10000 (the oddsDivisor)
            burn = (winnings - betAmount * oddsDivisor) / 2000 * betXPermille;
            payout = winnings - burn;
            pe.nMoneyLineHomePotentialLiability += payout / COIN ;
            pe.nMoneyLineHomeBets += 1;

        }else if (plBet.nOutcome == moneyLineLose){
            winnings = betAmount * pe.nAwayOdds;
            burn = (winnings - betAmount*oddsDivisor) / 2000 * betXPermille;
            payout = winnings - burn;
            pe.nMoneyLineAwayPotentialLiability += payout / COIN ;
            pe.nMoneyLineAwayBets += 1;

        }else if (plBet.nOutcome == moneyLineDraw){
            winnings = betAmount * pe.nDrawOdds;
            burn = (winnings - betAmount*oddsDivisor) / 2000 * betXPermille;
            payout = winnings - burn;
            pe.nMoneyLineDrawPotentialLiability += payout / COIN ;
            pe.nMoneyLineDrawBets += 1;

        }else if (plBet.nOutcome == spreadHome){
            winnings = betAmount * pe.nSpreadHomeOdds;
            burn = (winnings - betAmount*oddsDivisor) / 2000 * betXPermille;
            payout = winnings - burn;

            pe.nSpreadHomePotentialLiability += payout / COIN ;
            pe.nSpreadPushPotentialLiability += betAmount / COIN;
            pe.nSpreadHomeBets += 1;
            pe.nSpreadPushBets += 1;

        }else if (plBet.nOutcome == spreadAway){
            winnings = betAmount * pe.nSpreadAwayOdds;
            burn = (winnings - betAmount*oddsDivisor) / 2000 * betXPermille;
            payout = winnings - burn;

            pe.nSpreadAwayPotentialLiability += payout / COIN ;
            pe.nSpreadPushPotentialLiability += betAmount / COIN;
            pe.nSpreadAwayBets += 1;
            pe.nSpreadPushBets += 1;

        }else if (plBet.nOutcome == totalOver){
            winnings = betAmount * pe.nTotalOverOdds;
            burn = (winnings - betAmount*oddsDivisor) / 2000 * betXPermille;
            payout = winnings - burn;

            pe.nTotalOverPotentialLiability += payout / COIN ;
            pe.nTotalPushPotentialLiability += betAmount / COIN;
            pe.nTotalOverBets += 1;
            pe.nTotalPushBets += 1;

        }else if (plBet.nOutcome == totalUnder){
            winnings = betAmount * pe.nTotalUnderOdds;
            burn = (winnings - betAmount*oddsDivisor) / 2000 * betXPermille;
            payout = winnings - burn;

            pe.nTotalUnderPotentialLiability += payout / COIN;
            pe.nTotalPushPotentialLiability += betAmount / COIN;
            pe.nTotalUnderBets += 1;
            pe.nTotalPushBets += 1;

        }

        eventsIndex[plBet.nEventId] = pe;
        CEventDB::SetEvents(eventsIndex);
    }

}

/**
 * Split a CMapping OpCode string into byte components and store in CMapping object.
 *
 * @param opCode The CMapping op code string.
 * @param cm     The CMapping object.
 * @return       Bool
 */
bool CMapping::FromOpCode(std::string opCode, CMapping &cm)
{
    // Ensure the mapping transaction type is correct.
    if (opCode[2] != mappingTxType) {
        // TODO - add proper error handling
        return false;
    }

    // Ensure the mapping OpCode has the correct BTX format version number.
    if (ReadBTXFormatVersion(opCode) != BTX_FORMAT_VERSION) {
        // TODO - add proper error handling
        return false;
    }

    cm.nMType = (unsigned char) opCode[3];
    int nextOpIndex = 0;

    // If mapping op code is either a sport, tournament or round.
    if (opCode[3] == sportMapping || opCode[3] == roundMapping || opCode[3] == tournamentMapping) {
        cm.nId    = FromChars(opCode[4], opCode[5]);
        nextOpIndex = 6;
    }
    else { // Mapping is a team name mapping.
        cm.nId    = FromChars(opCode[4], opCode[5], opCode[6], opCode[7]);
        nextOpIndex = 8;
    }

    // Decode the the rest of the mapping OP Code to get the name.
    std::string name;

    while (opCode[nextOpIndex]) {
        unsigned char chr = opCode[nextOpIndex];
        name.push_back(chr);
        nextOpIndex++;
    }

    cm.sName = name;

    return true;
}

/**
 * Constructor for the CMapping database object.
 */
CMappingDB::CMappingDB(std::string fileName)
{
    mDBFileName = fileName;
    mFilePath   = GetDataDir() / fileName;
}

/** Global mapping indexes to store sports, rounds, team names and tournaments. **/
mappingIndex_t CMappingDB::mSportsIndex;
mappingIndex_t CMappingDB::mRoundsIndex;
mappingIndex_t CMappingDB::mTeamsIndex;
mappingIndex_t CMappingDB::mTournamentsIndex;

CCriticalSection CMappingDB::cs_setSports;
CCriticalSection CMappingDB::cs_setRounds;
CCriticalSection CMappingDB::cs_setTeams;
CCriticalSection CMappingDB::cs_setTournaments;

/**
 * Returns then global sports index.
 *
 * @param sportsIndex
 */
void CMappingDB::GetSports(mappingIndex_t &sportsIndex)
{
    LOCK(cs_setSports);
    sportsIndex = mSportsIndex;
}

/**
 * Set the current sports index.
 *
 * @param sportsIndex
 */
void CMappingDB::SetSports(const mappingIndex_t &sportsIndex)
{
    LOCK(cs_setSports);
    mSportsIndex = sportsIndex;
}

/**
 * Add a sport to the sports index.
 *
 * @param sm  Sport mapping object.
 */
void CMappingDB::AddSport(const CMapping sm)
{
    LOCK(cs_setSports);
    mSportsIndex.insert(make_pair(sm.nId, sm));
}

/**
 * Return the current rounds index.
 *
 * @param roundsIndex  Rounds mapping index.
 */
void CMappingDB::GetRounds(mappingIndex_t &roundsIndex)
{
    LOCK(cs_setRounds);
    roundsIndex = mRoundsIndex;
}

/**
 * Set the current rounds index.
 *
 * @param roundsIndex  Rounds mapping index.
 */
void CMappingDB::SetRounds(const mappingIndex_t &roundsIndex)
{
    LOCK(cs_setRounds);
    mRoundsIndex = roundsIndex;
}

/**
 * Add a round object to the rounds index.
 *
 * @param rm  Rounds mapping object.
 */
void CMappingDB::AddRound(const CMapping rm)
{
    LOCK(cs_setRounds);
    mRoundsIndex.insert(make_pair(rm.nId, rm));
}

/**
 * Returns the current teams index.
 *
 * @param teamsIndex  Teams mapping index.
 */
void CMappingDB::GetTeams(mappingIndex_t &teamsIndex)
{
    LOCK(cs_setTeams);
    teamsIndex = mTeamsIndex;
}

/**
 * Set the current teams index.
 *
 * @param teamsIndex  Teams mapping object.
 */
void CMappingDB::SetTeams(const mappingIndex_t &teamsIndex)
{
    LOCK(cs_setTeams);
    mTeamsIndex = teamsIndex;
}

/**
 * Add a team object to the teams index.
 *
 * @param tm Teams mapping object.
 */
void CMappingDB::AddTeam(const CMapping tm)
{
    LOCK(cs_setTeams);
    mTeamsIndex.insert(make_pair(tm.nId, tm));
}

/**
 * Return the current tournaments index.
 *
 * @param tournamentsIndex  Tournaments mapping index.
 */
void CMappingDB::GetTournaments(mappingIndex_t &tournamentsIndex)
{
    LOCK(cs_setTournaments);
    tournamentsIndex = mTournamentsIndex;
}

/**
 * Set the current tournaments index.
 *
 * @param tournamentsIndex  Tournaments mapping index.
 */
void CMappingDB::SetTournaments(const mappingIndex_t &tournamentsIndex)
{
    LOCK(cs_setTournaments);
    mTournamentsIndex = tournamentsIndex;
}

/**
 * Adds a tournament object to the tournaments index.
 *
 * @param tm Tournament mapping object.
 */
void CMappingDB::AddTournament(const CMapping tm)
{
    LOCK(cs_setTournaments);
    mTournamentsIndex.insert(make_pair(tm.nId, tm));
}

/**
 * Serialise a mapping index map into binary format and write to the related .dat file.
 *
 * @param mappingIndex    The index map which contains Wagerr mappings.
 * @param latestBlockHash The latest block hash which we can use a reference as to when data was last saved to the file.
 * @return                Bool
 */
bool CMappingDB::Write(const mappingIndex_t& mappingIndex, uint256 latestBlockHash)
{
    // Generate random temporary filename.
    unsigned short randv = 0;
    GetRandBytes((unsigned char*)&randv, sizeof(randv));
    std::string tmpfn = strprintf( mDBFileName + ".%04x", randv);

    // Serialize map index object and latest block hash as a reference.
    CDataStream ssMappings(SER_DISK, CLIENT_VERSION);
    ssMappings << latestBlockHash;
    ssMappings << mappingIndex;

    // Checksum added for verification purposes.
    uint256 hash = Hash(ssMappings.begin(), ssMappings.end());
    ssMappings << hash;

    // Open output file, and associate with CAutoFile.
    boost::filesystem::path pathTemp = GetDataDir() / tmpfn;
    FILE* file = fopen(pathTemp.string().c_str(), "wb");
    CAutoFile fileout(file, SER_DISK, CLIENT_VERSION);

    if (fileout.IsNull())
        return error("%s : Failed to open file %s", __func__, pathTemp.string());

    // Write and commit data.
    try {
        fileout << ssMappings;
    }
    catch (std::exception& e) {
        return error("%s : Serialize or I/O error - %s", __func__, e.what());
    }

    FileCommit(fileout.Get());
    fileout.fclose();

    // Replace existing .dat, if any, with new .dat.XXXX
    if (!RenameOver(pathTemp, mFilePath))
        return error("%s: Rename-into-place failed", __func__);

    return true;
}

/**
 * Reads a .dat file and deserializes the data to recreate an index map object.
 *
 * @param mappingIndex The index map which will be populated with data from the file.
 * @return             Bool
 */
bool CMappingDB::Read(mappingIndex_t& mappingIndex, uint256& lastBlockHash)
{
    // Open input file, and associate with CAutoFile.
    FILE* file = fopen(mFilePath.string().c_str(), "rb");
    CAutoFile filein(file, SER_DISK, CLIENT_VERSION);

    if (filein.IsNull())
        return error("%s : Failed to open file %s", __func__, mFilePath.string());

    // Use file size to size memory buffer.
    uint64_t fileSize = boost::filesystem::file_size(mFilePath);
    uint64_t dataSize = fileSize - sizeof(uint256);

    // Don't try to resize to a negative number if file is small.
    if (fileSize >= sizeof(uint256))
        dataSize = fileSize - sizeof(uint256);

    vector<unsigned char> vchData;
    vchData.resize(dataSize);
    uint256 hashIn;

    // Read data and checksum from file.
    try {
        filein.read((char*)&vchData[0], dataSize);
        filein >> hashIn;
    }
    catch (std::exception& e) {
        return error("%s : Deserialize or I/O error - %s", __func__, e.what());
    }

    filein.fclose();
    CDataStream ssMappings(vchData, SER_DISK, CLIENT_VERSION);

    // Verify stored checksum matches input data.
    uint256 hashTmp = Hash(ssMappings.begin(), ssMappings.end());
    if (hashIn != hashTmp)
        return error("%s : Checksum mismatch, data corrupted", __func__);

    try {
        ssMappings >> lastBlockHash;
        ssMappings >> mappingIndex;
    }
    catch (std::exception& e) {
        return error("%s : Deserialize or I/O error - %s", __func__, e.what());
    }

    return true;
}

/**
 * Constructor for the events database object.
 */
CEventDB::CEventDB()
{
    pathEvents = GetDataDir() / "events.dat";
}

/** The global events index. **/
eventIndex_t CEventDB::eventsIndex;
CCriticalSection CEventDB::cs_setEvents;

/**
 * Returns the current events list.
 *
 * @param eventIndex
 */
void CEventDB::GetEvents(eventIndex_t &eventIndex)
{
    LOCK(cs_setEvents);
    eventIndex = eventsIndex;
}

/**
 * Set the events list.
 *
 * @param eventIndex
 */
void CEventDB::SetEvents(const eventIndex_t &eventIndex)
{
    LOCK(cs_setEvents);
    eventsIndex = eventIndex;
}

/**
 * Add a new event to the event index.
 *
 * @param pe CPeerless Event object.
 */
void CEventDB::AddEvent(CPeerlessEvent pe)
{
    if (eventsIndex.count(pe.nEventId) > 0) {
        CPeerlessEvent saved_pe = eventsIndex.find(pe.nEventId)->second;
        saved_pe.nStartTime = pe.nStartTime;
        saved_pe.nHomeOdds = pe.nHomeOdds;
        saved_pe.nAwayOdds = pe.nAwayOdds;
        saved_pe.nDrawOdds = pe.nDrawOdds;
        eventsIndex[saved_pe.nEventId] = saved_pe;
        CEventDB::SetEvents(eventsIndex); 
    } else {
        LOCK(cs_setEvents);
        eventsIndex.insert(make_pair(pe.nEventId, pe));
    }
}

/**
 * Remove and event from the event index.
 *
 * @param pe
 */
void CEventDB::RemoveEvent(CPeerlessResult pr)
{
    LOCK(cs_setEvents);

    if (eventsIndex.count(pr.nEventId)) {
        eventsIndex.erase(pr.nEventId);
    }
}

/**
 * Serialises the event index map into binary format and writes to the events.dat file.
 *
 * @param eventIndex       The events index map which contains the current live events.
 * @param latestBlockHash  The latest block hash which we can use a reference as to when data was last saved to the file.
 * @return                 Bool
 */
bool CEventDB::Write(const eventIndex_t& eventIndex, uint256 latestBlockHash)
{
    // Generate random temporary filename.
    unsigned short randv = 0;
    GetRandBytes((unsigned char*)&randv, sizeof(randv));
    std::string tmpfn = strprintf("events.dat.%04x", randv);

    // Serialize event index object and latest block hash as a reference.
    CDataStream ssEvents(SER_DISK, CLIENT_VERSION);
    ssEvents << latestBlockHash;
    ssEvents << eventIndex;

    // Checksum added for verification purposes.
    uint256 hash = Hash(ssEvents.begin(), ssEvents.end());
    ssEvents << hash;

    // Open output file, and associate with CAutoFile.
    boost::filesystem::path pathTemp = GetDataDir() / tmpfn;
    FILE* file = fopen(pathTemp.string().c_str(), "wb");
    CAutoFile fileout(file, SER_DISK, CLIENT_VERSION);

    if (fileout.IsNull())
        return error("%s : Failed to open file %s", __func__, pathTemp.string());

    // Write and commit data.
    try {
        fileout << ssEvents;
    }
    catch (std::exception& e) {
        return error("%s : Serialize or I/O error - %s", __func__, e.what());
    }

    FileCommit(fileout.Get());
    fileout.fclose();

    // Replace existing events.dat, if any, with new events.dat.XXXX
    if (!RenameOver(pathTemp, pathEvents))
        return error("%s: Rename-into-place failed", __func__);

    return true;
}

/**
 * Reads the events.dat file and deserializes the data to recreate event index map object as well as the last
 * block hash before file was written to.
 *
 * @param eventIndex The event index map which will be populated with data from the file.
 * @return           Bool
 */
bool CEventDB::Read(eventIndex_t& eventIndex, uint256& lastBlockHash)
{
    // Open input file, and associate with CAutoFile.
    FILE* file = fopen(pathEvents.string().c_str(), "rb");
    CAutoFile filein(file, SER_DISK, CLIENT_VERSION);

    if (filein.IsNull())
        return error("%s : Failed to open file %s", __func__, pathEvents.string());

    // Use file size to size memory buffer.
    uint64_t fileSize = boost::filesystem::file_size(pathEvents);
    uint64_t dataSize = fileSize - sizeof(uint256);

    // Don't try to resize to a negative number if file is small.
    if (fileSize >= sizeof(uint256))
        dataSize = fileSize - sizeof(uint256);

    vector<unsigned char> vchData;
    vchData.resize(dataSize);
    uint256 hashIn;

    // Read data and checksum from file.
    try {
        filein.read((char*)&vchData[0], dataSize);
        filein >> hashIn;
    }
    catch (std::exception& e) {
        return error("%s : Deserialize or I/O error - %s", __func__, e.what());
    }

    filein.fclose();
    CDataStream ssEvents(vchData, SER_DISK, CLIENT_VERSION);

    // Verify stored checksum matches input data.
    uint256 hashTmp = Hash(ssEvents.begin(), ssEvents.end());
    if (hashIn != hashTmp)
        return error("%s : Checksum mismatch, data corrupted", __func__);

    try {
        ssEvents >> lastBlockHash;
        ssEvents >> eventIndex;
    }
    catch (std::exception& e) {
        return error("%s : Deserialize or I/O error - %s", __func__, e.what());
    }

    return true;
}


/**
 * Constructor for the results database object.
 */
CResultDB::CResultDB()
{
    pathResults = GetDataDir() / "results.dat";
}

/** The global results index. **/
resultsIndex_t CResultDB::resultsIndex;
CCriticalSection CResultDB::cs_setResults;

/**
 * Returns the current results.
 *
 * @param resultsIndex
 */
void CResultDB::GetResults(resultsIndex_t &resultIndex)
{
    LOCK(cs_setResults);
    resultIndex = resultsIndex;
}

/**
 * Set the results list.
 *
 * @param resultsIndex
 */
void CResultDB::SetResults(const resultsIndex_t &resultIndex)
{
    LOCK(cs_setResults);
    resultsIndex = resultIndex;
}

/**
 * Add a new result to the result index.
 *
 * @param pr CPeerlessResult object.
 */
void CResultDB::AddResult(CPeerlessResult pr)
{
    // If result already exists then update it
    if (resultsIndex.count(pr.nEventId) > 0) {
        resultsIndex[pr.nEventId] = pr;
        CResultDB::SetResults(resultsIndex);
    }
    // Else save the new result.
    else {
        LOCK(cs_setResults);
        resultsIndex.insert(make_pair(pr.nEventId, pr));
    }
}

/**
 * Remove and event from the event index.
 *
 * @param pe
 */
void CResultDB::RemoveResult(CPeerlessResult pr)
{
    LOCK(cs_setResults);
    resultsIndex.erase(pr.nEventId);
}

/**
 * Serialises the event index map into binary format and writes to the events.dat file.
 *
 * @param eventIndex       The events index map which contains the current live events.
 * @param latestBlockHash  The latest block hash which we can use a reference as to when data was last saved to the file.
 * @return                 Bool
 */
bool CResultDB::Write(const resultsIndex_t& resultsIndex, uint256 latestBlockHash)
{
    // Generate random temporary filename.
    unsigned short randv = 0;
    GetRandBytes((unsigned char*)&randv, sizeof(randv));
    std::string tmpfn = strprintf("results.dat.%04x", randv);

    // Serialize event index object and latest block hash as a reference.
    CDataStream ssResults(SER_DISK, CLIENT_VERSION);
    ssResults << latestBlockHash;
    ssResults << resultsIndex;

    // Checksum added for verification purposes.
    uint256 hash = Hash(ssResults.begin(), ssResults.end());
    ssResults << hash;

    // Open output file, and associate with CAutoFile.
    boost::filesystem::path pathTemp = GetDataDir() / tmpfn;
    FILE* file = fopen(pathTemp.string().c_str(), "wb");
    CAutoFile fileout(file, SER_DISK, CLIENT_VERSION);

    if (fileout.IsNull())
        return error("%s : Failed to open file %s", __func__, pathTemp.string());

    // Write and commit data.
    try {
        fileout << ssResults;
    }
    catch (std::exception& e) {
        return error("%s : Serialize or I/O error - %s", __func__, e.what());
    }

    FileCommit(fileout.Get());
    fileout.fclose();

    // Replace existing events.dat, if any, with new events.dat.XXXX
    if (!RenameOver(pathTemp, pathResults))
        return error("%s: Rename-into-place failed", __func__);

    return true;
}

/**
 * Reads the events.dat file and deserializes the data to recreate event index map object as well as the last
 * block hash before file was written to.
 *
 * @param eventIndex The event index map which will be populated with data from the file.
 * @return           Bool
 */
bool CResultDB::Read(resultsIndex_t& resultsIndex, uint256& lastBlockHash)
{
    // Open input file, and associate with CAutoFile.
    FILE* file = fopen(pathResults.string().c_str(), "rb");
    CAutoFile filein(file, SER_DISK, CLIENT_VERSION);

    if (filein.IsNull())
        return error("%s : Failed to open file %s", __func__, pathResults.string());

    // Use file size to size memory buffer.
    uint64_t fileSize = boost::filesystem::file_size(pathResults);
    uint64_t dataSize = fileSize - sizeof(uint256);

    // Don't try to resize to a negative number if file is small.
    if (fileSize >= sizeof(uint256))
        dataSize = fileSize - sizeof(uint256);

    vector<unsigned char> vchData;
    vchData.resize(dataSize);
    uint256 hashIn;

    // Read data and checksum from file.
    try {
        filein.read((char*)&vchData[0], dataSize);
        filein >> hashIn;
    }
    catch (std::exception& e) {
        return error("%s : Deserialize or I/O error - %s", __func__, e.what());
    }

    filein.fclose();
    CDataStream ssResults(vchData, SER_DISK, CLIENT_VERSION);

    // Verify stored checksum matches input data.
    uint256 hashTmp = Hash(ssResults.begin(), ssResults.end());
    if (hashIn != hashTmp)
        return error("%s : Checksum mismatch, data corrupted", __func__);

    try {
        ssResults >> lastBlockHash;
        ssResults >> resultsIndex;
    }
    catch (std::exception& e) {
        return error("%s : Deserialize or I/O error - %s", __func__, e.what());
    }

    return true;
}

/**
 * Check a given block to see if it contains a Peerless result TX.
 *
 * @return results vector.
 */
std::vector<CPeerlessResult> getEventResults( int height )
{
    std::vector<CPeerlessResult> results;

    // Get the current block so we can look for any results in it.
    CBlockIndex *resultsBocksIndex = NULL;
    resultsBocksIndex = chainActive[height];

    CBlock block;
    ReadBlockFromDisk(block, resultsBocksIndex);

    BOOST_FOREACH(CTransaction& tx, block.vtx) {
        // Ensure the result TX has been posted by Oracle wallet.
        const CTxIn &txin  = tx.vin[0];
        bool validResultTx = IsValidOracleTx(txin);

        if (validResultTx) {
            // Look for result OP RETURN code in the tx vouts.
            for (unsigned int i = 0; i < tx.vout.size(); i++) {

                const CTxOut &txout = tx.vout[i];
                std::string scriptPubKey = txout.scriptPubKey.ToString();

                // TODO Remove hard-coded values from this block.
                if (scriptPubKey.length() > 0 && strncmp(scriptPubKey.c_str(), "OP_RETURN", 9) == 0) {

                    // Get OP CODE from transactions.
                    vector<unsigned char> v = ParseHex(scriptPubKey.substr(9, string::npos));
                    std::string opCode(v.begin(), v.end());

                    CPeerlessResult plResult;
                    if (!CPeerlessResult::FromOpCode(opCode, plResult)) {
                        continue;
                    }

                    LogPrintf("Results found...\n");

                    // Store the result if its a valid result OP CODE.
                    results.push_back(plResult);
                }
            }
        }
    }

    return results;
}

// TODO function will need to be refactored and cleaned up at a later stage as we have had to make rapid and frequent code changes.
/**
 * Creates the bet payout vector for all winning CPeerless bets.
 *
 * @return payout vector.
 */
std::vector<CBetOut> GetBetPayouts(int height)
{
    std::vector<CBetOut> vExpectedPayouts;
    int nCurrentHeight = chainActive.Height();

    // Get all the results posted in the latest block.
    std::vector<CPeerlessResult> results = getEventResults(height);

    // Traverse the blockchain for an event to match a result and all the bets on a result.
    for (const auto& result : results) {
        // Look back the chain 14 days for any events and bets.
        CBlockIndex *BlocksIndex = NULL;
        BlocksIndex = chainActive[nCurrentHeight - Params().BetBlocksIndexTimespan()];

        uint64_t oddsDivisor  = Params().OddsDivisor();
        uint64_t betXPermille = Params().BetXPermille();

        OutcomeType nMoneylineResult = (OutcomeType) 0;
        std::vector<OutcomeType> vSpreadsResult;
        std::vector<OutcomeType> vTotalsResult;

        uint64_t nMoneylineOdds     = 0;
        uint64_t nSpreadsOdds       = 0;
        uint64_t nTotalsOdds        = 0;
        uint64_t nTotalsPoints      = result.nHomeScore + result.nAwayScore;
        uint64_t nSpreadsDifference = 0;
        bool HomeFavorite               = false;

        // We keep temp values as we can't be sure of the order of the TX's being stored in a block.
        // This can lead to a case were some bets don't
        uint64_t nTempMoneylineOdds = 0;
        uint64_t nTempSpreadsOdds   = 0;
        uint64_t nTempTotalsOdds    = 0;

        bool UpdateMoneyLine = false;
        bool UpdateSpreads   = false;
        bool UpdateTotals    = false;
        uint64_t nSpreadsWinner = 0;
        uint64_t nTotalsWinner  = 0;

        time_t tempEventStartTime   = 0;
        time_t latestEventStartTime = 0;
        bool eventFound = false;
        bool spreadsFound = false;
        bool totalsFound = false;

        // Find MoneyLine outcome (result).
        if (result.nHomeScore > result.nAwayScore) {
            nMoneylineResult = moneyLineWin;
        }
        else if (result.nHomeScore < result.nAwayScore) {
            nMoneylineResult = moneyLineLose;
        }
        else if (result.nHomeScore == result.nAwayScore) {
            nMoneylineResult = moneyLineDraw;
        }
        
        // Traverse the block chain to find events and bets.
        while (BlocksIndex) {
            CBlock block;
            ReadBlockFromDisk(block, BlocksIndex);
            time_t transactionTime = block.nTime;

            BOOST_FOREACH(CTransaction &tx, block.vtx) {
                // Check all TX vouts for an OP RETURN.
                for (unsigned int i = 0; i < tx.vout.size(); i++) {

                    const CTxOut &txout = tx.vout[i];
                    std::string scriptPubKey = txout.scriptPubKey.ToString();
                    CAmount betAmount = txout.nValue;

                    if (scriptPubKey.length() > 0 && 0 == strncmp(scriptPubKey.c_str(), "OP_RETURN", 9)) {

                        // Ensure TX has it been posted by Oracle wallet.
                        const CTxIn &txin = tx.vin[0];
                        bool validOracleTx = IsValidOracleTx(txin);

                        // Get the OP CODE from the transaction scriptPubKey.
                        vector<unsigned char> vOpCode = ParseHex(scriptPubKey.substr(9, string::npos));
                        std::string opCode(vOpCode.begin(), vOpCode.end());

                        // Peerless event OP RETURN transaction.
                        CPeerlessEvent pe;
                        if (validOracleTx && CPeerlessEvent::FromOpCode(opCode, pe)) {

                            // If the current event matches the result we can now set the odds.
                            if (result.nEventId == pe.nEventId) {

                                LogPrintf("EVENT OP CODE - %s \n", opCode.c_str());

                                UpdateMoneyLine    = true;
                                eventFound         = true;
                                tempEventStartTime = pe.nStartTime;

                                // Set the temp moneyline odds.
                                if (nMoneylineResult == moneyLineWin) {
                                    nTempMoneylineOdds = pe.nHomeOdds;
                                }
                                else if (nMoneylineResult == moneyLineLose) {
                                    nTempMoneylineOdds = pe.nAwayOdds;
                                }
                                else if (nMoneylineResult == moneyLineDraw) {
                                    nTempMoneylineOdds = pe.nDrawOdds;
                                }

                                // Set which team is the favorite, used for calculating spreads difference & winner.
                                if (pe.nHomeOdds < pe.nAwayOdds) {
                                    HomeFavorite = true;
                                    if (result.nHomeScore > result.nAwayScore) {
                                        nSpreadsDifference = result.nHomeScore - result.nAwayScore;
                                    }
                                    else{
                                        nSpreadsDifference = 0;
                                    }
                                }
                                else {
                                    HomeFavorite = false;
                                    if (result.nAwayScore > result.nHomeScore) {
                                        nSpreadsDifference = result.nAwayScore - result.nHomeScore;
                                    }

                                    else{
                                        nSpreadsDifference = 0;
                                    }
                                }
                            }
                        }

                        // Peerless update odds OP RETURN transaction.
                        CPeerlessUpdateOdds puo;
                        if (eventFound && validOracleTx && CPeerlessUpdateOdds::FromOpCode(opCode, puo) && result.nEventId == puo.nEventId ) {

                            LogPrintf("PUO EVENT OP CODE - %s \n", opCode.c_str());

                            UpdateMoneyLine = true;

                            // If current event ID matches result ID set the odds.
                            if (nMoneylineResult == moneyLineWin) {
                                nTempMoneylineOdds = puo.nHomeOdds;
                            }
                            else if (nMoneylineResult == moneyLineLose) {
                                nTempMoneylineOdds = puo.nAwayOdds;
                            }
                            else if (nMoneylineResult == moneyLineDraw) {
                                nTempMoneylineOdds = puo.nDrawOdds;
                            }
                        }

                        // Handle PSE, when we find a Spreads event on chain we need to update the Spreads odds.
                        CPeerlessSpreadsEvent pse;
                        if (eventFound && validOracleTx && CPeerlessSpreadsEvent::FromOpCode(opCode, pse) && result.nEventId == pse.nEventId) {

                            LogPrintf("PSE EVENT OP CODE - %s \n", opCode.c_str());

                            UpdateSpreads = true;
                            spreadsFound  = true;

                            // If the home team is the favourite.
                            if (HomeFavorite){
                                //  Choose the spreads winner.
                                if (nSpreadsDifference == 0) {
                                    nSpreadsWinner = WinnerType::awayWin;
                                }
                                else if (pse.nPoints < nSpreadsDifference) {
                                    nSpreadsWinner = WinnerType::homeWin;
                                }
                                else if (pse.nPoints > nSpreadsDifference) {
                                    nSpreadsWinner = WinnerType::awayWin;
                                }
                                else {
                                    nSpreadsWinner = WinnerType::push;
                                }
                            }
                            // If the away team is the favourite.
                            else {
                                // Cho0se the winner.
                                if (nSpreadsDifference == 0) {
                                    nSpreadsWinner = WinnerType::homeWin;
                                }
                                else if (pse.nPoints > nSpreadsDifference) {
                                    nSpreadsWinner = WinnerType::homeWin;
                                }
                                else if (pse.nPoints < nSpreadsDifference) {
                                    nSpreadsWinner = WinnerType::awayWin;
                                }
                                else {
                                    nSpreadsWinner = WinnerType::push;
                                }
                            }

                            // Set the temp spread odds.
                            if (nSpreadsWinner == WinnerType::push) {
                                nTempSpreadsOdds = Params().OddsDivisor();
                            }
                            else if (nSpreadsWinner == WinnerType::awayWin) {
                                nTempSpreadsOdds = pse.nAwayOdds;
                            }
                            else if (nSpreadsWinner == WinnerType::homeWin) {
                                nTempSpreadsOdds = pse.nHomeOdds;
                            }
                        }

                        // Handle PTE, when we find an Totals event on chain we need to update the Totals odds.
                        CPeerlessTotalsEvent pte;
                        if (eventFound && validOracleTx && CPeerlessTotalsEvent::FromOpCode(opCode, pte) && result.nEventId == pte.nEventId) {

                            LogPrintf("PTE EVENT OP CODE - %s \n", opCode.c_str());

                            UpdateTotals = true;
                            totalsFound  = true;

                            // Find totals outcome (result).
                            if (pte.nPoints == nTotalsPoints) {
                                nTotalsWinner = WinnerType::push;
                            }
                            else if (pte.nPoints > nTotalsPoints) {
                                nTotalsWinner = WinnerType::awayWin;
                            }
                            else {
                                nTotalsWinner = WinnerType::homeWin;
                            }

                            // Set the totals temp odds.
                            if (nTotalsWinner == WinnerType::push) {
                                nTempTotalsOdds = Params().OddsDivisor();
                            }
                            else if (nTotalsWinner == WinnerType::awayWin) {
                                nTempTotalsOdds = pte.nUnderOdds;
                            }
                            else if (nTotalsWinner == WinnerType::homeWin) {
                                nTempTotalsOdds = pte.nOverOdds;
                            }
                        }

                        // If we encounter the result after cycling the chain then we dont need go any furture so finish the payout.
                        CPeerlessResult pr;
                        if (eventFound && validOracleTx && CPeerlessResult::FromOpCode(opCode, pr) && result.nEventId == pr.nEventId ) {

                            LogPrintf("Result found ending search \n");

                            return vExpectedPayouts;
                        }

                        // Only payout bets that are between 25 - 10000 WRG inclusive (MaxBetPayoutRange).
                        if (eventFound && betAmount >= (Params().MinBetPayoutRange() * COIN) && betAmount <= (Params().MaxBetPayoutRange() * COIN)) {

                            // Bet OP RETURN transaction.
                            CPeerlessBet pb;
                            if (CPeerlessBet::FromOpCode(opCode, pb)) {

                                CAmount payout = 0 * COIN;

                                // If bet was placed less than 20 mins before event start or after event start discard it.
                                if (latestEventStartTime > 0 && (unsigned int) transactionTime > (latestEventStartTime - Params().BetPlaceTimeoutBlocks())) {
                                    continue;
                                }

                                // Is the bet a winning bet?
                                if (result.nEventId == pb.nEventId) {
                                    CAmount winnings = 0;

                                    // If bet payout result.
                                    if (result.nResultType ==  ResultType::standardResult) {

                                        // Calculate winnings.
                                        if (pb.nOutcome == nMoneylineResult) {
                                            winnings = betAmount * nMoneylineOdds;
                                        }
                                        else if (spreadsFound && (pb.nOutcome == vSpreadsResult.at(0) || pb.nOutcome == vSpreadsResult.at(1))) {
                                            winnings = betAmount * nSpreadsOdds;
                                        }
                                        else if (totalsFound && (pb.nOutcome == vTotalsResult.at(0) || pb.nOutcome == vTotalsResult.at(1))) {
                                           winnings = betAmount * nTotalsOdds;
                                        }

                                        // Calculate the bet winnings for the current bet.
                                        if (winnings > 0) {
                                            payout = (winnings - ((winnings - betAmount*oddsDivisor) / 1000 * betXPermille)) / oddsDivisor;
                                        }
                                        else {
                                            payout = 0;
                                        }
                                    }
                                    // Bet refund result.
                                    else if (result.nResultType ==  ResultType::eventRefund){
                                        payout = betAmount;
                                    } else if (result.nResultType == ResultType::mlRefund){
                                        // Calculate winnings.
                                        if (pb.nOutcome == nMoneylineResult) {
                                            payout = betAmount;
                                        }
                                        else if (spreadsFound && (pb.nOutcome == vSpreadsResult.at(0) || pb.nOutcome == vSpreadsResult.at(1))) {
                                            winnings = betAmount * nSpreadsOdds;

                                            // Calculate the bet winnings for the current bet.
                                            if (winnings > 0) {
                                                payout = (winnings - ((winnings - betAmount*oddsDivisor) / 1000 * betXPermille)) / oddsDivisor;
                                            }
                                            else {
                                                payout = 0;
                                            }
                                        }
                                        else if (totalsFound && (pb.nOutcome == vTotalsResult.at(0) || pb.nOutcome == vTotalsResult.at(1))) {
                                           winnings = betAmount * nTotalsOdds;

                                            // Calculate the bet winnings for the current bet.
                                            if (winnings > 0) {
                                                payout = (winnings - ((winnings - betAmount*oddsDivisor) / 1000 * betXPermille)) / oddsDivisor;
                                            }
                                            else {
                                                payout = 0;
                                            }
                                        }
                                    }

                                    // Get the users payout address from the vin of the bet TX they used to place the bet.
                                    CTxDestination payoutAddress;
                                    const CTxIn &txin = tx.vin[0];
                                    COutPoint prevout = txin.prevout;

                                    uint256 hashBlock;
                                    CTransaction txPrev;
                                    if (GetTransaction(prevout.hash, txPrev, hashBlock, true)) {
                                        ExtractDestination( txPrev.vout[prevout.n].scriptPubKey, payoutAddress );
                                    }

                                    LogPrintf("WINNING PAYOUT :)\n");
                                    LogPrintf("AMOUNT: %li \n", payout);
                                    LogPrintf("ADDRESS: %s \n", CBitcoinAddress( payoutAddress ).ToString().c_str());

                                    // Only add valid payouts to the vector.
                                    if (payout > 0) {
                                        // Add winning bet payout to the bet vector.
                                        vExpectedPayouts.emplace_back(payout, GetScriptForDestination(CBitcoinAddress(payoutAddress).Get()), betAmount);
                                    }
                                }
                            }
                        }
                    }
                }
            }

            // If an update transaction came in on this block, the bool would be set to true and the odds/winners will be updated (below) for the next block
            if (UpdateMoneyLine){
                UpdateMoneyLine      = false;
                nMoneylineOdds       = nTempMoneylineOdds;
                latestEventStartTime = tempEventStartTime;
            }

            // If we need to update the spreads odds using temp values.
            if (UpdateSpreads) {
                UpdateSpreads = false;
                //set the payout odds (using the temp odds)
                nSpreadsOdds = nTempSpreadsOdds;
                //clear the winner vector (used to determine which bets to payout).
                vSpreadsResult.clear();

                //Depending on the calculations above we populate the winner vector (push/away/home)
                if (nSpreadsWinner == WinnerType::homeWin) {
                    vSpreadsResult.emplace_back(spreadHome);
                    vSpreadsResult.emplace_back(spreadHome);
                }
                else if (nSpreadsWinner == WinnerType::awayWin) {
                    vSpreadsResult.emplace_back(spreadAway);
                    vSpreadsResult.emplace_back(spreadAway);
                }
                else if (nSpreadsWinner == WinnerType::push) {
                    vSpreadsResult.emplace_back(spreadHome);
                    vSpreadsResult.emplace_back(spreadAway);
                }

                nSpreadsWinner = 0;
            }

            // If we need to update the totals odds using the temp values.
            if (UpdateTotals) {
                UpdateTotals = false;
                nTotalsOdds  = nTempTotalsOdds;
                vTotalsResult.clear();

                if (nTotalsWinner == WinnerType::homeWin) {
                    vTotalsResult.emplace_back(totalOver);
                    vTotalsResult.emplace_back(totalOver);
                }
                else if (nTotalsWinner == WinnerType::awayWin) {
                    vTotalsResult.emplace_back(totalUnder);
                    vTotalsResult.emplace_back(totalUnder);
                }
                else if (nTotalsWinner == WinnerType::push) {
                    vTotalsResult.emplace_back(totalOver);
                    vTotalsResult.emplace_back(totalUnder);
                }

                nTotalsWinner = 0;
            }

            BlocksIndex = chainActive.Next(BlocksIndex);
        }
    }

    return vExpectedPayouts;
}

bool CChainGamesResult::FromScript(CScript script) {
    if (script.size() < 5) return false;
    if (script[0] != OP_RETURN) return false;
    if (script[1] != BTX_FORMAT_VERSION) return false;
    if (script[2] != cgResultTxType) return false;

    nEventId = script[3] << 8 | script[4];

    return true;
}
/**
 * Checks a given block for any Chain Games results.
 *
 * @param height The block we want to check for the result.
 * @return results array.
 */
std::pair<std::vector<CChainGamesResult>,std::vector<std::string>> getCGLottoEventResults(int height)
{
    std::vector<CChainGamesResult> chainGameResults;
    std::vector<std::string> blockTotalValues;
    CAmount totalBlockValue = 0;

    // Get the current block so we can look for any results in it.
    CBlockIndex *resultsBocksIndex = chainActive[height];

    CBlock block;
    ReadBlockFromDisk(block, resultsBocksIndex);

    int blockTime = block.GetBlockTime();

    BOOST_FOREACH(CTransaction& tx, block.vtx) {
        // Ensure the result TX has been posted by Oracle wallet by looking at the TX vins.
        const CTxIn &txin = tx.vin[0];
        uint256 hashBlock;
        CTransaction txPrev;

        bool validResultTx = IsValidOracleTx(txin);

        if (validResultTx) {
            // Look for result OP RETURN code in the tx vouts.
            for (unsigned int i = 0; i < tx.vout.size(); i++) {
                CScript script = tx.vout[i].scriptPubKey;

                CChainGamesResult cgResult;
                if (cgResult.FromScript(script)) {
                    chainGameResults.push_back(cgResult);
                }
            }
        }
    }

    unsigned long long LGTotal = blockTime + totalBlockValue;
    char strTotal[256];
    sprintf(strTotal, "%lld", LGTotal);

    // If a CGLotto result is found, append total block value to each result
    if (chainGameResults.size() != 0) {
        for (unsigned int i = 0; i < chainGameResults.size(); i++) {
            blockTotalValues.emplace_back(strTotal);
        }
    }

    return std::make_pair(chainGameResults,blockTotalValues);
}

/**
 * Creates the bet payout vector for all winning CGLotto events.
 *
 * @return payout vector.
 */
std::vector<CBetOut> GetCGLottoBetPayouts (int height)
{
    std::vector<CBetOut> vexpectedCGLottoBetPayouts;
    int nCurrentHeight = chainActive.Height();
    long long totalValueOfBlock = 0;

    std::pair<std::vector<CChainGamesResult>,std::vector<std::string>> resultArray = getCGLottoEventResults(height);
    std::vector<CChainGamesResult> allChainGames = resultArray.first;
    std::vector<std::string> blockSizeArray = resultArray.second;

    // Find payout for each CGLotto game
    for (unsigned int currResult = 0; currResult < resultArray.second.size(); currResult++) {

        CChainGamesResult currentChainGame = allChainGames[currResult];
        int currentEventID = currentChainGame.nEventId;
        CAmount eventFee = 0;

        totalValueOfBlock = stoll(blockSizeArray[0]);

        //reset total bet amount and candidate array for this event
        std::vector<std::string> candidates;
        CAmount totalBetAmount = 0 * COIN;

        // Look back the chain 10 days for any events and bets.
        CBlockIndex *BlocksIndex = NULL;
        BlocksIndex = chainActive[nCurrentHeight - 14400];

        time_t eventStart = 0;
        bool eventStartedFlag = false;

        while (BlocksIndex) {

            CBlock block;
            ReadBlockFromDisk(block, BlocksIndex);
            time_t transactionTime = block.nTime;

            BOOST_FOREACH(CTransaction &tx, block.vtx) {

                // Ensure if event TX that has it been posted by Oracle wallet.
                const CTxIn &txin = tx.vin[0];
                COutPoint prevout = txin.prevout;

                uint256 hashBlock;
                CTransaction txPrev;

                bool validTX = IsValidOracleTx(txin);

                // Check all TX vouts for an OP RETURN.
                for (unsigned int i = 0; i < tx.vout.size(); i++) {

                    const CTxOut &txout = tx.vout[i];
                    std::string scriptPubKey = txout.scriptPubKey.ToString();
                    CAmount betAmount = txout.nValue;

                    if (scriptPubKey.length() > 0 && 0 == strncmp(scriptPubKey.c_str(), "OP_RETURN", 9)) {
                        // Get the OP CODE from the transaction scriptPubKey.
                        vector<unsigned char> vOpCode = ParseHex(scriptPubKey.substr(9, string::npos));
                        std::string opCode(vOpCode.begin(), vOpCode.end());

                        // If bet was placed less than 20 mins before event start or after event start discard it.
                        if (eventStart > 0 && transactionTime > (eventStart - Params().BetPlaceTimeoutBlocks())) {
                            eventStartedFlag = true;
                            break;
                        }

                        // Find most recent CGLotto event
                        CChainGamesEvent chainGameEvt;
                        if (validTX && CChainGamesEvent::FromOpCode(opCode, chainGameEvt)) {
                            eventFee = chainGameEvt.nEntryFee * COIN;
                        }

                        // Find most recent CGLotto bet once the event has been found
                        CChainGamesBet chainGamesBet;
                        if (CChainGamesBet::FromOpCode(opCode, chainGamesBet)) {

                            int eventId = chainGamesBet.nEventId;

                            // If current event ID matches result ID add bettor to candidate array
                            if (eventId == currentEventID) {

                                CTxDestination address;
                                ExtractDestination(tx.vout[0].scriptPubKey, address);

                                //Check Entry fee matches the bet amount
                                if (eventFee == betAmount) {

                                    totalBetAmount = totalBetAmount + betAmount;
                                    CTxDestination payoutAddress;

                                    if (GetTransaction(prevout.hash, txPrev, hashBlock, true)) {
                                        ExtractDestination( txPrev.vout[prevout.n].scriptPubKey, payoutAddress );
                                    }

                                    // Add the payout address of each candidate to array
                                    candidates.push_back(CBitcoinAddress( payoutAddress ).ToString().c_str());
                                }
                            }
                        }
                    }
                }

                if (eventStartedFlag) {
                    break;
                }
            }

            if (eventStartedFlag) {
                break;
            }

            BlocksIndex = chainActive.Next(BlocksIndex);
        }

        // Choose winner from candidates who entered the lotto and payout their winnings.
        if (candidates.size() == 1) {
             // Refund the single entrant.
             CAmount noOfBets = candidates.size();
             std::string winnerAddress = candidates[0];
             CAmount entranceFee = eventFee;
             CAmount winnerPayout = eventFee;

 	         LogPrintf("\nCHAIN GAMES PAYOUT. ID: %i \n", allChainGames[currResult].nEventId);
 	         LogPrintf("Total number of bettors: %u , Entrance Fee: %u \n", noOfBets, entranceFee);
 	         LogPrintf("Winner Address: %u \n", winnerAddress);
 	         LogPrintf(" This Lotto was refunded as only one person bought a ticket.\n" );

             // Only add valid payouts to the vector.
             if (winnerPayout > 0) {
                 vexpectedCGLottoBetPayouts.emplace_back(winnerPayout, GetScriptForDestination(CBitcoinAddress(winnerAddress).Get()), entranceFee);
             }
        }
        else if (candidates.size() >= 2) {
            // Use random number to choose winner.
            CAmount noOfBets    = candidates.size();

            uint256 hashProofOfStake = 0;
            unique_ptr<CStakeInput> stake;

            CBlockIndex *winBlockIndex = chainActive[height];
            CBlock winBlock;
            ReadBlockFromDisk(winBlock, winBlockIndex);

            if (!CheckProofOfStake(winBlock, hashProofOfStake, stake)) {
                hashProofOfStake = 0;
                return vexpectedCGLottoBetPayouts;
            }
            uint64_t winnerNr = hashProofOfStake.Get64() % noOfBets;

            // Split the pot and calculate winnings.
            std::string winnerAddress = candidates[winnerNr];
            CAmount entranceFee = eventFee;
            CAmount totalPot = (noOfBets*entranceFee);
            CAmount winnerPayout = totalPot / 10 * 8;
            CAmount fee = totalPot / 50;

            LogPrintf("\nCHAIN GAMES PAYOUT. ID: %i \n", allChainGames[currResult].nEventId);
            LogPrintf("Total number Of bettors: %u , Entrance Fee: %u \n", noOfBets, entranceFee);
            LogPrintf("Winner Address: %u (index no %u) \n", winnerAddress, winnerNr);
            LogPrintf("Total Value of Block: %u \n", totalValueOfBlock);
            LogPrintf("Total Pot: %u, Winnings: %u, Fee: %u \n", totalPot, winnerPayout, fee);

            // Only add valid payouts to the vector.
            if (winnerPayout > 0) {
                vexpectedCGLottoBetPayouts.emplace_back(winnerPayout, GetScriptForDestination(CBitcoinAddress(winnerAddress).Get()), entranceFee);
                vexpectedCGLottoBetPayouts.emplace_back(fee, GetScriptForDestination(CBitcoinAddress(Params().OMNOPayoutAddr()).Get()), entranceFee);
            }
        }
    }

    return vexpectedCGLottoBetPayouts;
}
