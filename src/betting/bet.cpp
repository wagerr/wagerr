// Copyright (c) 2018-2020 The Wagerr developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "bet.h"

#include "uint256.h"
#include "wallet/wallet.h"

#define BTX_FORMAT_VERSION 0x01
#define BTX_HEX_PREFIX "42"

// String lengths for all currently supported op codes.
#define PE_OP_STRLEN     74
#define PB_OP_STRLEN     16
#define PPB_OP_STRMINLEN 18
#define PR_OP_STRLEN     24
#define PUO_OP_STRLEN    38
#define CGE_OP_STRLEN    14
#define CGB_OP_STRLEN    10
#define CGR_OP_STRLEN    10
#define PSE_OP_STRLEN    34
#define PTE_OP_STRLEN    34
#define PEP_OP_STRLEN    22
#define CGB_OP_STRMINLEN 10

CBettingsView* bettingsView = nullptr;

namespace
{
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

    pe.fLegacyInitialHomeFavorite = pe.nHomeOdds < pe.nAwayOdds ? true : false;

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
 * Convert vector of CPeerlessBet objects to OpCode string
 * of parlay bet
 *
 * @param legs   The ref to vector of CPeerlessBet objects
 * @param opCode The Parlay Bet OpCode string
 * @return       Bool
 */
bool CPeerlessBet::ParlayToOpCode(const std::vector<CPeerlessBet>& legs, std::string& opCode)
{
    CDataStream ss(SER_NETWORK, CLIENT_VERSION);
    ss << 'B' << (uint8_t) BTX_FORMAT_VERSION << (uint8_t) plParlayBetTxType << legs;
    opCode = HexStr(ss.begin(), ss.end());

    return true;
}

/**
 * Split a OpCode string into byte components and store in vector of peerless
 * bet object.
 *
 * @param opCode The Parlay Bet OpCode string
 * @param legs   The ref to vector of CPeerlessBet objects
 * @return       Bool
 */
bool CPeerlessBet::ParlayFromOpCode(const std::string& opCode, std::vector<CPeerlessBet>& legs)
{
    if (opCode.size() < PPB_OP_STRMINLEN) return false;
    CDataStream ss(ParseHex(opCode), SER_NETWORK, CLIENT_VERSION);
    uint8_t byte;
    legs.clear();
    // get BTX prefix
    ss >> byte;
    if (byte != 'B') return false;
    // get BTX format version
    ss >> byte;
    if (byte != (uint8_t) BTX_FORMAT_VERSION) return false;
    // get parlay bet tx type
    ss >> byte;
    if (byte != (uint8_t) plParlayBetTxType) return false;
    // get legs
    ss >> legs;
    if (legs.size() > Params().MaxParlayLegs()) return false;

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

bool CChainGamesResult::FromScript(CScript script)
{
    // LogPrintf("%s - %s\n", __func__, script.ToString());

    CScript::const_iterator pc = script.begin();
    std::vector<unsigned char> data;
    opcodetype opcode;

    // Check that we are parsing an OP_RETURN script
    if (!script.GetOp(pc, opcode, data)) return false;
    if (opcode != OP_RETURN) return false;

    // Get data block
    if (!script.GetOp(pc, opcode, data)) return false;

    if (data.size() < 5) return false;
    if (data[0] != 'B') return false;
    if (data[1] != BTX_FORMAT_VERSION) return false;
    if (data[2] != cgResultTxType) return false;

    nEventId = *((uint16_t*)&data[3]);

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
    pse.nVersion = ReadBTXFormatVersion(opCode);
    if (pse.nVersion == 1) {
        pse.nEventId   = FromChars(opCode[3], opCode[4], opCode[5], opCode[6]);
        pse.nPoints    = FromChars(opCode[7], opCode[8]);
        pse.nHomeOdds  = FromChars(opCode[9], opCode[10], opCode[11], opCode[12]);
        pse.nAwayOdds  = FromChars(opCode[13], opCode[14], opCode[15], opCode[16]);
    } else if (pse.nVersion == 2) {
        pse.nEventId   = FromChars(opCode[3], opCode[4], opCode[5], opCode[6]);
        pse.nPoints    = (int16_t)(FromChars(opCode[7], opCode[8]) & 0xffff);
        pse.nHomeOdds  = FromChars(opCode[9], opCode[10], opCode[11], opCode[12]);
        pse.nAwayOdds  = FromChars(opCode[13], opCode[14], opCode[15], opCode[16]);
    } else {
        // TODO - add proper error handling
        return false;
    }


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
 * Split a CPeerlessEventPatch OpCode string into byte components and update a peerless
 * event object.
 *
 * @param opCode  The CPeerlessEventPatch OpCode string
 * @param pe      The CPeerlessEventPatch object
 * @return        Bool
 */
bool CPeerlessEventPatch::FromOpCode(std::string opCode, CPeerlessEventPatch &pe)
{
    // Ensure PeerlessEventPatch OpCode string is the correct length.
    if (opCode.length() != PEP_OP_STRLEN / 2) {
        // TODO - add proper error handling
        return false;
    }

    // Ensure the PeerlessEventPatch transaction type is correct.
    if (opCode[2] != plEventPatchTxType) {
        // TODO - add proper error handling
        return false;
    }

    // Ensure the PeerlessEventPatch OpCode has the correct BTX format version number.
    if (ReadBTXFormatVersion(opCode) != BTX_FORMAT_VERSION) {
        // TODO - add proper error handling
        return false;
    }

    // Parse the OPCODE hex data.
    pe.nEventId    = FromChars(opCode[3], opCode[4], opCode[5], opCode[6]);
    pe.nStartTime  = FromChars(opCode[7], opCode[8], opCode[9], opCode[10]);

    return true;
}

/**
 * Convert CPeerlessEventPatch object data into hex OPCode string.
 *
 * @param pe     The CPeerlessEventPatch object
 * @param opCode The CPeerlessEventPatch OpCode string
 * @return       Bool
 */
bool CPeerlessEventPatch::ToOpCode(CPeerlessEventPatch pe, std::string &opCode)
{
    std::string sEventId    = ToHex(pe.nEventId, 8);
    std::string sStartTime  = ToHex(pe.nStartTime,  8);

    opCode = BTX_HEX_PREFIX "010b" + sEventId + sStartTime;

    // Ensure PeerlessEventPatch OpCode string is the correct length.
    if (opCode.length() != PEP_OP_STRLEN) {
        // TODO - add proper error handling
        return false;
    }

    return true;
}

MappingTypes CMapping::GetType() const
{
    return static_cast<MappingTypes>(nMType);
}

std::string CMapping::ToTypeName(MappingTypes type)
{
    switch (type) {
    case sportMapping:
        return "sports";
    case roundMapping:
        return "rounds";
    case teamMapping:
        return "teamnames";
    case tournamentMapping:
        return "tournaments";
    }
    return "";
}

MappingTypes CMapping::FromTypeName(const std::string& name)
{
    if (name == ToTypeName(sportMapping)) {
        return sportMapping;
    }
    if (name == ToTypeName(roundMapping)) {
        return roundMapping;
    }
    if (name == ToTypeName(teamMapping)) {
        return teamMapping;
    }
    if (name == ToTypeName(tournamentMapping)) {
        return tournamentMapping;
    }
    return static_cast<MappingTypes>(-1);
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

bool CQuickGamesTxBet::ToOpCode(CQuickGamesTxBet& bet, std::string &opCode)
{
    CDataStream ss(SER_NETWORK, CLIENT_VERSION);
    ss << 'B' << (uint8_t) BTX_FORMAT_VERSION << (uint8_t) qgBetTxType << bet;
    opCode = HexStr(ss.begin(), ss.end());

    return true;
}

bool CQuickGamesTxBet::FromOpCode(std::string opCode, CQuickGamesTxBet &bet)
{
    if (opCode.size() < CGB_OP_STRMINLEN) return false;
    CDataStream ss(ParseHex(opCode), SER_NETWORK, CLIENT_VERSION);
    uint8_t byte;
    // get BTX prefix
    ss >> byte;
    if (byte != 'B') return false;
    // get BTX format version
    ss >> byte;
    if (byte != (uint8_t) BTX_FORMAT_VERSION) return false;
    // get quick games bet tx type
    ss >> byte;
    if (byte != (uint8_t) qgBetTxType) return false;
    // get bet data
    ss >> bet;

    return true;
}

/**
 * Validate the transaction to ensure it has been posted by an oracle node.
 *
 * @param txin  TX vin input hash.
 * @return      Bool
 */
bool IsValidOracleTx(const CTxIn &txin)
{
    COutPoint prevout = txin.prevout;
    std::vector<std::string> oracleAddrs = Params().OracleWalletAddrs();

    uint256 hashBlock;
    CTransaction txPrev;
    if (GetTransaction(prevout.hash, txPrev, hashBlock, true)) {

        const CTxOut &prevTxOut = txPrev.vout[prevout.n];
        std::string scriptPubKey = prevTxOut.scriptPubKey.ToString();

        txnouttype type;
        std::vector<CTxDestination> prevAddrs;
        int nRequired;

        if (ExtractDestinations(prevTxOut.scriptPubKey, type, prevAddrs, nRequired)) {
            for (const CTxDestination &prevAddr : prevAddrs) {
                if (std::find(oracleAddrs.begin(), oracleAddrs.end(), CBitcoinAddress(prevAddr).ToString()) != oracleAddrs.end()) {
                    return true;
                }
            }
        }
    }

    return false;
}

bool ExtractPayouts(const CBlock& block, std::vector<CTxOut>& vFoundPayouts, uint32_t& nPayoutOffset, uint32_t& nWinnerPayments, const CAmount& nExpectedMint, const CAmount& nExpectedMNReward)
{
    const CTransaction &tx = block.vtx[1];

    // Get the vin staking value so we can use it to find out how many staking TX in the vouts.
    COutPoint prevout         = tx.vin[0].prevout;
    CAmount stakeAmount       = 0;
    bool fStakesFound         = false;

    nPayoutOffset = 0;
    nWinnerPayments = 0;

    uint256 hashBlock;
    CTransaction txPrev;

    if (GetTransaction(prevout.hash, txPrev, hashBlock, true)) {
        stakeAmount = txPrev.vout[prevout.n].nValue + nExpectedMint;
    } else {
        return false;
    }

    // Set the OMNO and Dev reward addresses
    CScript devPayoutScript = GetScriptForDestination(CBitcoinAddress(Params().DevPayoutAddr()).Get());
    CScript OMNOPayoutScript = GetScriptForDestination(CBitcoinAddress(Params().OMNOPayoutAddr()).Get());

    // Count the coinbase and staking vouts in the current block TX.
    CAmount totalStakeAcc = 0;
    CAmount mnValue = 0;
    const int txVoutSize = tx.vout.size();

    int nMaxVoutI = txVoutSize;
    CAmount nMNReward = 0;
    if (txVoutSize > 2 && tx.vout[txVoutSize - 1].nValue == nExpectedMNReward) {
        nMaxVoutI--;
        nMNReward = nExpectedMNReward;
    }

    for (unsigned int i = 0; i < nMaxVoutI; i++) {
        const CTxOut &txout = tx.vout[i];
        CAmount voutValue   = txout.nValue;
        CScript voutScript = txout.scriptPubKey;
        if (fStakesFound) {
            if (voutScript != devPayoutScript && voutScript != OMNOPayoutScript) {
                nWinnerPayments++;
            }
            if (voutValue > 0) vFoundPayouts.emplace_back(voutValue, voutScript);
        } else {
            nPayoutOffset++;
            totalStakeAcc += voutValue;

            // Needs some slack?
            if (totalStakeAcc + nMNReward == stakeAmount) {
                fStakesFound = true;
            }
        }
    }
    return fStakesFound || (nWinnerPayments == 0 && totalStakeAcc + nMNReward < stakeAmount);
}

bool IsBlockPayoutsValid(CBettingsView &bettingsViewCache, const std::multimap<CPayoutInfo, CBetOut>& mExpectedPayoutsIn, const CBlock& block, const int nBlockHeight, const CAmount& nExpectedMint, const CAmount& nExpectedMNReward)
{
    const CTransaction &tx = block.vtx[1];
    std::multimap<CPayoutInfo, CBetOut> mExpectedPayouts = mExpectedPayoutsIn;

    std::vector<CTxOut> vFoundPayouts;
    std::multiset<CTxOut> setFoundPayouts;
    std::multiset<CTxOut> setExpectedPayouts;

    uint32_t nPayoutOffset = 0;
    uint32_t nWinnerPayments = 0; // unused

    // If we have payouts to validate. Note: bets can only happen in blocks with MN payments.
    if (!ExtractPayouts(block, vFoundPayouts, nPayoutOffset, nWinnerPayments, nExpectedMint, nExpectedMNReward)) {
        LogPrintf("%s - Not all payouts found - %s\n", __func__, block.GetHash().ToString());
        return false;
    }
    setFoundPayouts = std::multiset<CTxOut>(vFoundPayouts.begin(), vFoundPayouts.end());

    for (auto expectedPayout : mExpectedPayouts)
    {
        setExpectedPayouts.insert(expectedPayout.second);
    }

    if (setExpectedPayouts != setFoundPayouts) {
        LogPrintf("%s - Expected payouts:\n", __func__);
        for (auto expectedPayout : setExpectedPayouts) {
            LogPrintf("%s %d %d\n", expectedPayout.nRounds, expectedPayout.nValue, expectedPayout.scriptPubKey.ToString());
        }
        LogPrintf("%s - Found payouts:\n", __func__);
        for (auto foundPayouts : setFoundPayouts) {
            LogPrintf("%s %d %d\n", foundPayouts.nRounds, foundPayouts.nValue, foundPayouts.scriptPubKey.ToString());
        }
        LogPrintf("%s - Not all payouts validate - %s\n", __func__, block.GetHash().ToString());
        return false;
    }

    // Lookup txid+voutnr and store in database cache
    for (uint32_t i = 0; i < vFoundPayouts.size(); i++)
    {
        auto expectedPayout = std::find_if(mExpectedPayouts.begin(), mExpectedPayouts.end(),
                        [vFoundPayouts, i](std::pair<const CPayoutInfo, CBetOut> eP) {
                            return eP.second.nValue == vFoundPayouts[i].nValue && eP.second.scriptPubKey == vFoundPayouts[i].scriptPubKey;
                        });
        if (expectedPayout != mExpectedPayouts.end()) {
            PayoutInfoKey payoutInfoKey{static_cast<uint32_t>(nBlockHeight), COutPoint{tx.GetHash(), i+nPayoutOffset}};
            bettingsViewCache.payoutsInfo->Write(payoutInfoKey, expectedPayout->first);

            mExpectedPayouts.erase(expectedPayout);
        } else {
            // Redundant: should not happen after previous test 'setExpectedPayouts != setFoundPayouts'
            LogPrintf("%s - Could not find expected payout - %s\n", __func__, block.GetHash().ToString());
            return false;
        }
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

    bool fMultipleResultsAllowed = (height >= Params().WagerrProtocolV3StartHeight());

    // Get the current block so we can look for any results in it.
    CBlockIndex *resultsBocksIndex = NULL;
    resultsBocksIndex = chainActive[height];

    CBlock block;
    ReadBlockFromDisk(block, resultsBocksIndex);

    for (CTransaction& tx : block.vtx) {
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
                    std::vector<unsigned char> v = ParseHex(scriptPubKey.substr(9, std::string::npos));
                    std::string opCode(v.begin(), v.end());

                    CPeerlessResult plResult;
                    if (!CPeerlessResult::FromOpCode(opCode, plResult)) {
                        continue;
                    }

                    LogPrintf("Result for event %lu was found...\n", plResult.nEventId);

                    // Store the result if its a valid result OP CODE.
                    results.push_back(plResult);
                    if (!fMultipleResultsAllowed) return results;
                }
            }
        }
    }

    return results;
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

    for (CTransaction& tx : block.vtx) {
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
 * Undo only bet payout mark as completed in DB.
 * But coin tx outs were undid early in native bitcoin core.
 * @return
 */
bool UndoBetPayouts(CBettingsView &bettingsViewCache, int height)
{
    int nCurrentHeight = chainActive.Height();
    // Get all the results posted in the previous block.
    std::vector<CPeerlessResult> results = getEventResults(height - 1);

    LogPrintf("Start undo payouts...\n");

    for (auto result : results) {

        // look bets at last 14 days
        uint32_t startHeight = nCurrentHeight >= Params().BetBlocksIndexTimespan() ? nCurrentHeight - Params().BetBlocksIndexTimespan() : 0;

        auto it = bettingsViewCache.bets->NewIterator();
        std::vector<std::pair<UniversalBetKey, CUniversalBet>> vEntriesToUpdate;
        for (it->Seek(CBettingDB::DbTypeToBytes(UniversalBetKey{startHeight, COutPoint()})); it->Valid(); it->Next()) {
            UniversalBetKey uniBetKey;
            CUniversalBet uniBet;
            CBettingDB::BytesToDbType(it->Key(), uniBetKey);
            CBettingDB::BytesToDbType(it->Value(), uniBet);
            // skip if bet is uncompleted
            if (!uniBet.IsCompleted()) continue;

            bool needUndo = false;

            // parlay bet
            if (uniBet.legs.size() > 1) {
                bool resultFound = false;
                for (auto leg : uniBet.legs) {
                    // if we found one result for parlay - check each other legs
                    if (leg.nEventId == result.nEventId) {
                        resultFound = true;
                    }
                }
                if (resultFound) {
                    // make assumption that parlay is handled
                    needUndo = true;
                    // find all results for all legs
                    for (uint32_t idx = 0; idx < uniBet.legs.size(); idx++) {
                        CPeerlessBet &leg = uniBet.legs[idx];
                        // skip this bet if incompleted (can't find one result)
                        CPeerlessResult res;
                        if (!bettingsViewCache.results->Read(ResultKey{leg.nEventId}, res)) {
                            needUndo = false;
                        }
                    }
                }
            }
            // single bet
            else if (uniBet.legs.size() == 1) {
                CPeerlessBet &singleBet = uniBet.legs[0];
                if (singleBet.nEventId == result.nEventId) {
                    needUndo = true;
                }
            }

            if (needUndo) {
                uniBet.SetUncompleted();
                uniBet.resultType = BetResultType::betResultUnknown;
                uniBet.payout = 0;
                vEntriesToUpdate.emplace_back(std::pair<UniversalBetKey, CUniversalBet>{uniBetKey, uniBet});
            }
        }
        for (auto pair : vEntriesToUpdate) {
            bettingsViewCache.bets->Update(pair.first, pair.second);
        }
    }
    return true;
}

bool CheckBettingTx(CBettingsView& bettingsViewCache, const CTransaction& tx, const int height)
{
    // if is not hardfork for parlays - do not check tx
    if (height < Params().BetStartHeight()) return true;

    // Search for any new bets
    for (unsigned int i = 0; i < tx.vout.size(); i++) {
        const CTxOut& txout = tx.vout[i];
        std::string s = txout.scriptPubKey.ToString();

        if (0 == strncmp(s.c_str(), "OP_RETURN", 9)) {
            std::vector<unsigned char> v = ParseHex(s.substr(9, std::string::npos));
            std::string opCode(v.begin(), v.end());
            std::string opCodeHexStr = s.substr(10);

            CAmount betAmount{txout.nValue};
            // Get player address
            const CTxIn& txin{tx.vin[0]};
            uint256 hashBlock;
            CTransaction txPrev;
            CBitcoinAddress address;
            CTxDestination prevAddr;
            // if we cant extract playerAddress - drop tx
            if ((GetTransaction(txin.prevout.hash, txPrev, hashBlock, true),
                    !ExtractDestination(txPrev.vout[txin.prevout.n].scriptPubKey, prevAddr))) {
                return error("CheckBettingTX: Couldn't extract destination!");
            }
            address = CBitcoinAddress(prevAddr);

            std::vector<CPeerlessBet> legs;
            std::vector<CPeerlessEvent> lockedEvents;
            if (CPeerlessBet::ParlayFromOpCode(opCodeHexStr, legs)) {
                // Validate parlay bet amount so its between 25 - 4000 WGR inclusive.
                if (betAmount < (Params().MinBetPayoutRange()  * COIN ) || betAmount > (Params().MaxParlayBetPayoutRange() * COIN)) {
                    return error("CheckBettingTX: Bet placed with invalid amount %lu!", betAmount);
                }
                // delete duplicated legs
                std::sort(legs.begin(), legs.end());
                legs.erase(std::unique(legs.begin(), legs.end()), legs.end());

                std::vector<CBettingUndo> vUndos;
                for (auto it = legs.begin(); it != legs.end(); it++) {
                    CPeerlessBet &bet = *it;
                    CPeerlessEvent plEvent;
                    // Find the event in DB
                    if (bettingsViewCache.events->Read(EventKey{bet.nEventId}, plEvent)) {
                        if (bettingsViewCache.results->Exists(ResultKey{bet.nEventId})) {
                            return error("CheckBettingTX: Bet placed to resulted event %lu!", bet.nEventId);
                        }
                    }
                    else {
                        return error("CheckBettingTX: Failed to find event %lu!", bet.nEventId);
                    }
                }
            }

            CPeerlessBet plBet;
            if (CPeerlessBet::FromOpCode(opCode, plBet)) {
                // Validate bet amount so its between 25 - 10000 WGR inclusive.
                if (betAmount < (Params().MinBetPayoutRange()  * COIN ) || betAmount > (Params().MaxBetPayoutRange() * COIN)) {
                    return error("CheckBettingTX: Bet placed with invalid amount %lu!", betAmount);
                }
                CPeerlessEvent plEvent;
                // Find the event in DB
                if (bettingsViewCache.events->Read(EventKey{plBet.nEventId}, plEvent)) {
                    if (bettingsViewCache.results->Exists(ResultKey{plBet.nEventId})) {
                        return error("CheckBettingTX: Bet placed to resulted event %lu!", plBet.nEventId);
                    }
                }
                else {
                    return error("CheckBettingTX: Failed to find event %lu!", plBet.nEventId);
                }
            }

            CQuickGamesTxBet qgTxBet;
            if (CQuickGamesTxBet::FromOpCode(opCodeHexStr, qgTxBet)) {
                // Validate quick game bet amount so its between 25 - 10000 WGR inclusive.
                if (betAmount < (Params().MinBetPayoutRange()  * COIN ) || betAmount > (Params().MaxBetPayoutRange() * COIN)) {
                    return error("CheckBettingTX: Bet placed with invalid amount %lu!", betAmount);
                }
            }
        }
    }
    return true;
}

void ParseBettingTx(CBettingsView& bettingsViewCache, const CTransaction& tx, const int height, const int64_t blockTime, const bool fBetProtocolV3)
{
    // Ensure the event TX has come from Oracle wallet.
    const CTxIn& txin{tx.vin[0]};
    const bool validOracleTx{IsValidOracleTx(txin)};

    LogPrintf("ParseBettingTx: start, time: %lu, tx hash: %s\n", blockTime, tx.GetHash().GetHex());

    bool parlayBetsAvaible = height >= Params().WagerrProtocolV3StartHeight();

    // Search for any new bets
    for (unsigned int i = 0; i < tx.vout.size(); i++) {
        const CTxOut& txout = tx.vout[i];
        std::string s = txout.scriptPubKey.ToString();

        COutPoint out(tx.GetHash(), i);
        const uint256 undoId = SerializeHash(out);

        if (0 == strncmp(s.c_str(), "OP_RETURN", 9)) {
            std::vector<unsigned char> v = ParseHex(s.substr(9, std::string::npos));
            std::string opCode(v.begin(), v.end());
            std::string opCodeHexStr = s.substr(10);

            CAmount betAmount{txout.nValue};
            // Get player address
            uint256 hashBlock;
            CTransaction txPrev;
            CBitcoinAddress address;
            CTxDestination prevAddr;
            // if we cant extract playerAddress - skip vout
            if ((GetTransaction(txin.prevout.hash, txPrev, hashBlock, true),
                    !ExtractDestination(txPrev.vout[txin.prevout.n].scriptPubKey, prevAddr))) {
                LogPrintf("Couldn't extract destination!");
                continue;
            }
            address = CBitcoinAddress(prevAddr);

            std::vector<CPeerlessBet> legs;
            std::vector<CPeerlessEvent> lockedEvents;
            if (parlayBetsAvaible && CPeerlessBet::ParlayFromOpCode(opCodeHexStr, legs)) {
                LogPrintf("ParlayBet: legs: ");
                for (auto leg : legs) {
                    LogPrintf("(id: %lu, outcome: %lu), ", leg.nEventId, leg.nOutcome);
                }
                LogPrintf("\n");
                // delete duplicated legs
                std::sort(legs.begin(), legs.end());
                legs.erase(std::unique(legs.begin(), legs.end()), legs.end());

                std::vector<CBettingUndo> vUndos;
                for (auto it = legs.begin(); it != legs.end();) {
                    CPeerlessBet &bet = *it;
                    CPeerlessEvent plEvent, lockedEvent;
                    EventKey eventKey{bet.nEventId};
                    // Find the event in DB
                    // get locked event from previous block for getting correct odds
                    if (bettingsView->events->Read(eventKey, lockedEvent) &&
                            bettingsViewCache.events->Read(eventKey, plEvent)) {
                        vUndos.emplace_back(BettingUndoVariant{plEvent}, (uint32_t)height);
                        switch (bet.nOutcome) {
                            case moneyLineHomeWin:
                                plEvent.nMoneyLineHomeBets += 1;
                                break;
                            case moneyLineAwayWin:
                                plEvent.nMoneyLineAwayBets += 1;
                                break;
                            case moneyLineDraw:
                                plEvent.nMoneyLineDrawBets += 1;
                                break;
                            case spreadHome:
                                plEvent.nSpreadHomeBets += 1;
                                plEvent.nSpreadPushBets += 1;
                                break;
                            case spreadAway:
                                plEvent.nSpreadAwayBets += 1;
                                plEvent.nSpreadPushBets += 1;
                                break;
                            case totalOver:
                                plEvent.nTotalOverBets += 1;
                                plEvent.nTotalPushBets += 1;
                                break;
                            case totalUnder:
                                plEvent.nTotalUnderBets += 1;
                                plEvent.nTotalPushBets += 1;
                                break;
                            default:
                                std::runtime_error("Unknown bet outcome type!");
                        }
                        lockedEvents.emplace_back(lockedEvent);
                        it++;
                        bettingsViewCache.events->Update(eventKey, plEvent);
                    }
                    else {
                        LogPrintf("Failed to find event!\n");
                        continue;
                    }
                }
                if (!legs.empty()) {
                    // save prev event state to undo
                    bettingsViewCache.SaveBettingUndo(undoId, vUndos);
                    bettingsViewCache.bets->Write(UniversalBetKey{static_cast<uint32_t>(height), out}, CUniversalBet(betAmount, address, legs, lockedEvents, out, blockTime));
                }
                continue;
            }

            CPeerlessBet plBet;
            if (CPeerlessBet::FromOpCode(opCode, plBet)) {
                CPeerlessEvent plEvent, lockedEvent;
                EventKey eventKey{plBet.nEventId};

                LogPrintf("CPeerlessBet: id: %lu, outcome: %lu\n", plBet.nEventId, plBet.nOutcome);
                // Find the event in DB
                // get locked event from previous block for getting correct odds
                if (bettingsView->events->Read(eventKey, lockedEvent) &&
                        bettingsViewCache.events->Read(eventKey, plEvent)) {
                    CAmount payout = 0 * COIN;
                    CAmount burn = 0;

                    // save prev event state to undo
                    bettingsViewCache.SaveBettingUndo(undoId, {CBettingUndo{BettingUndoVariant{plEvent}, (uint32_t)height}});
                    // Check which outcome the bet was placed on and add to accumulators
                    switch (plBet.nOutcome) {
                        case moneyLineHomeWin:
                            CalculatePayoutBurnAmounts(betAmount, plEvent.nHomeOdds, payout, burn);

                            plEvent.nMoneyLineHomePotentialLiability += payout / COIN ;
                            plEvent.nMoneyLineHomeBets += 1;
                            break;
                        case moneyLineAwayWin:
                            CalculatePayoutBurnAmounts(betAmount, plEvent.nAwayOdds, payout, burn);

                            plEvent.nMoneyLineAwayPotentialLiability += payout / COIN ;
                            plEvent.nMoneyLineAwayBets += 1;
                            break;
                        case moneyLineDraw:
                            CalculatePayoutBurnAmounts(betAmount, plEvent.nDrawOdds, payout, burn);

                            plEvent.nMoneyLineDrawPotentialLiability += payout / COIN ;
                            plEvent.nMoneyLineDrawBets += 1;
                            break;
                        case spreadHome:
                            CalculatePayoutBurnAmounts(betAmount, plEvent.nSpreadHomeOdds, payout, burn);

                            plEvent.nSpreadHomePotentialLiability += payout / COIN ;
                            plEvent.nSpreadPushPotentialLiability += betAmount / COIN;
                            plEvent.nSpreadHomeBets += 1;
                            plEvent.nSpreadPushBets += 1;
                            break;
                        case spreadAway:
                            CalculatePayoutBurnAmounts(betAmount, plEvent.nSpreadAwayOdds, payout, burn);

                            plEvent.nSpreadAwayPotentialLiability += payout / COIN ;
                            plEvent.nSpreadPushPotentialLiability += betAmount / COIN;
                            plEvent.nSpreadAwayBets += 1;
                            plEvent.nSpreadPushBets += 1;
                            break;
                        case totalOver:
                            CalculatePayoutBurnAmounts(betAmount, plEvent.nTotalOverOdds, payout, burn);

                            plEvent.nTotalOverPotentialLiability += payout / COIN ;
                            plEvent.nTotalPushPotentialLiability += betAmount / COIN;
                            plEvent.nTotalOverBets += 1;
                            plEvent.nTotalPushBets += 1;
                            break;
                        case totalUnder:
                            CalculatePayoutBurnAmounts(betAmount, plEvent.nTotalUnderOdds, payout, burn);

                            plEvent.nTotalUnderPotentialLiability += payout / COIN;
                            plEvent.nTotalPushPotentialLiability += betAmount / COIN;
                            plEvent.nTotalUnderBets += 1;
                            plEvent.nTotalPushBets += 1;
                            break;
                        default:
                            std::runtime_error("Unknown bet outcome type!");
                            break;
                    }
                    if (!bettingsViewCache.events->Update(eventKey, plEvent)) {
                        LogPrintf("Failed to update event!\n");
                        continue;
                    }

                    // add single bet into vector
                    legs.emplace_back(plBet.nEventId, plBet.nOutcome);
                    lockedEvents.emplace_back(lockedEvent);
                    bettingsViewCache.bets->Write(UniversalBetKey{static_cast<uint32_t>(height), out}, CUniversalBet(betAmount, address, legs, lockedEvents, out, blockTime));
                }
                else {
                    LogPrintf("Failed to find event!\n");
                }
                continue;
            }

            CQuickGamesTxBet qgTxBet;
            if (CQuickGamesTxBet::FromOpCode(opCodeHexStr, qgTxBet)) {
                LogPrintf("CQuickGamesTxBet: gameType: %d, betInfo: %s", qgTxBet.gameType, std::string(qgTxBet.vBetInfo.begin(), qgTxBet.vBetInfo.end()));
                bettingsViewCache.quickGamesBets->Write(
                    QuickGamesBetKey{static_cast<uint32_t>(height), out},
                    CQuickGamesBet{qgTxBet.gameType, qgTxBet.vBetInfo, betAmount, address, blockTime});
                continue;
            }
        }
    }
    // If a valid OMNO transaction.
    if (validOracleTx) {

        for (unsigned int i = 0; i < tx.vout.size(); i++) {
            const CTxOut& txout = tx.vout[i];
            std::string s = txout.scriptPubKey.ToString();

            COutPoint out(tx.GetHash(), i);
            const uint256 undoId = SerializeHash(out);

            if (0 == strncmp(s.c_str(), "OP_RETURN", 9)) {
                std::vector<unsigned char> v = ParseHex(s.substr(9, std::string::npos));
                std::string opCode(v.begin(), v.end());

                // If mapping found then add it to the relating map index and write the map index to disk.
                CMapping mapping{};
                if (CMapping::FromOpCode(opCode, mapping)) {
                    LogPrintf("CMapping: type: %lu, id: %lu, name: %s\n", mapping.nMType, mapping.nId, mapping.sName);
                    MappingKey mappingKey{mapping.nMType, mapping.nId};
                    if (!bettingsViewCache.mappings->Write(mappingKey, mapping))
                        LogPrintf("Failed to write new mapping!\n");
                    continue;
                }

                // If events found in block add them to the events index.
                CPeerlessEvent plEvent{};
                if (CPeerlessEvent::FromOpCode(opCode, plEvent)) {
                    if (!fBetProtocolV3)
                    {
                        plEvent.nEventCreationHeight = height;
                    }
                    LogPrintf("CPeerlessEvent: id: %lu, sport: %lu, tournament: %lu, stage: %lu,\n\t\t\thome: %lu, away: %lu, homeOdds: %lu, awayOdds: %lu, drawOdds: %lu\n",
                        plEvent.nEventId,
                        plEvent.nSport,
                        plEvent.nTournament,
                        plEvent.nStage,
                        plEvent.nHomeTeam,
                        plEvent.nAwayTeam,
                        plEvent.nHomeOdds,
                        plEvent.nAwayOdds,
                        plEvent.nDrawOdds);
                    EventKey eventKey{plEvent.nEventId};
                    if (!bettingsViewCache.events->Write(eventKey, plEvent)) {
                        if (!fBetProtocolV3) {
                            LogPrintf("CPeerlessEvent - Legacy - try to patch with new event data: id: %lu, time: %lu\n", plEvent.nEventId, plEvent.nStartTime);
                            CPeerlessEvent plEventToPatch;
                            EventKey eventKey{plEvent.nEventId};
                            // First check a peerless event exists in DB
                            if (bettingsViewCache.events->Read(eventKey, plEventToPatch)) {
                                // save prev event state to undo
                                bettingsViewCache.SaveBettingUndo(undoId, {CBettingUndo{BettingUndoVariant{plEventToPatch}, (uint32_t)height}});

                                plEventToPatch.nStartTime = plEvent.nStartTime;
                                plEventToPatch.nSport = plEvent.nSport;
                                plEventToPatch.nTournament = plEvent.nTournament;
                                plEventToPatch.nStage = plEvent.nStage;
                                plEventToPatch.nHomeTeam = plEvent.nHomeTeam;
                                plEventToPatch.nAwayTeam = plEvent.nAwayTeam;
                                plEventToPatch.nHomeOdds = plEvent.nHomeOdds;
                                plEventToPatch.nAwayOdds = plEvent.nAwayOdds;
                                plEventToPatch.nDrawOdds = plEvent.nDrawOdds;

                                if (!bettingsViewCache.events->Update(eventKey, plEventToPatch)) {
                                    LogPrintf("Failed to update event!\n");
                                }
                            }
                        } else {
                            LogPrintf("Failed to write new event!\n");
                        }
                    }
                    continue;
                }

                // If event patch found in block apply them to the events.
                CPeerlessEventPatch plEventPatch{};
                if (CPeerlessEventPatch::FromOpCode(opCode, plEventPatch)) {
                    LogPrintf("CPeerlessEventPatch: id: %lu, time: %lu\n", plEventPatch.nEventId, plEventPatch.nStartTime);
                    CPeerlessEvent plEvent;
                    EventKey eventKey{plEventPatch.nEventId};
                    // First check a peerless event exists in DB
                    if (bettingsViewCache.events->Read(eventKey, plEvent)) {
                        // save prev event state to undo
                        bettingsViewCache.SaveBettingUndo(undoId, {CBettingUndo{BettingUndoVariant{plEvent}, (uint32_t)height}});

                        plEvent.nStartTime = plEventPatch.nStartTime;

                        if (!bettingsViewCache.events->Update(eventKey, plEvent))
                            LogPrintf("Failed to update event!\n");
                    }
                    else {
                        LogPrintf("Failed to find event!\n");
                    }
                    continue;
                }

                // If results found in block add result to result index.
                CPeerlessResult plResult{};
                if (CPeerlessResult::FromOpCode(opCode, plResult)) {
                    LogPrintf("CPeerlessResult: id: %lu, resultType: %lu, homeScore: %lu, awayScore: %lu\n", plResult.nEventId, plResult.nResultType, plResult.nHomeScore, plResult.nAwayScore);
                    ResultKey resultKey{plResult.nEventId};
                    if (!bettingsViewCache.results->Write(resultKey, plResult))
                        LogPrintf("Failed to write result!\n");
                    continue;
                }

                // If update money line odds TX found in block, update the event index.
                CPeerlessUpdateOdds puo{};
                if (CPeerlessUpdateOdds::FromOpCode(opCode, puo)) {
                    LogPrintf("CPeerlessUpdateOdds: id: %lu, homeOdds: %lu, awayOdds: %lu, drawOdds: %lu\n", puo.nEventId, puo.nHomeOdds, puo.nAwayOdds, puo.nDrawOdds);
                    CPeerlessEvent plEvent;
                    EventKey eventKey{puo.nEventId};
                    // First check a peerless event exists in DB.
                    if (bettingsViewCache.events->Read(eventKey, plEvent)) {
                        // save prev event state to undo
                        bettingsViewCache.SaveBettingUndo(undoId, {CBettingUndo{BettingUndoVariant{plEvent}, (uint32_t)height}});

                        plEvent.nHomeOdds = puo.nHomeOdds;
                        plEvent.nAwayOdds = puo.nAwayOdds;
                        plEvent.nDrawOdds = puo.nDrawOdds;

                        // Update the event in the DB.
                        if (!bettingsViewCache.events->Update(eventKey, plEvent))
                            LogPrintf("Failed to update event!\n");
                    }
                    else {
                        LogPrintf("Failed to find event!\n");
                    }
                    continue;
                }

                // If spread odds TX found then update the spread odds for that event object.
                CPeerlessSpreadsEvent spreadEvent{};
                if (CPeerlessSpreadsEvent::FromOpCode(opCode, spreadEvent)) {
                    LogPrintf("CPeerlessSpreadsEvent: id: %lu, spreadPoints: %lu, homeOdds: %lu, awayOdds: %lu\n", spreadEvent.nEventId, spreadEvent.nPoints, spreadEvent.nHomeOdds, spreadEvent.nAwayOdds);
                    CPeerlessEvent plEvent;
                    EventKey eventKey{spreadEvent.nEventId};
                    // First check a peerless event exists in the event index.
                    if (bettingsViewCache.events->Read(eventKey, plEvent)) {
                        // save prev event state to undo
                        bettingsViewCache.SaveBettingUndo(undoId, {CBettingUndo{BettingUndoVariant{plEvent}, (uint32_t)height}});

                        plEvent.nSpreadPoints    = spreadEvent.nPoints;
                        plEvent.nSpreadHomeOdds  = spreadEvent.nHomeOdds;
                        plEvent.nSpreadAwayOdds  = spreadEvent.nAwayOdds;
                        // Update the event in the DB.
                        if (!bettingsViewCache.events->Update(eventKey, plEvent))
                            LogPrintf("Failed to update event!\n");
                    }
                    else {
                        LogPrintf("Failed to find event!\n");
                    }
                    continue;
                }

                // If total odds TX found then update the total odds for that event object.
                CPeerlessTotalsEvent totalsEvent{};
                if (CPeerlessTotalsEvent::FromOpCode(opCode, totalsEvent)) {
                    LogPrintf("CPeerlessTotalsEvent: id: %lu, totalPoints: %lu, overOdds: %lu, underOdds: %lu\n", totalsEvent.nEventId, totalsEvent.nPoints, totalsEvent.nOverOdds, totalsEvent.nUnderOdds);
                    CPeerlessEvent plEvent;
                    EventKey eventKey{totalsEvent.nEventId};
                    // First check a peerless event exists in the event index.
                    if (bettingsViewCache.events->Read(eventKey, plEvent)) {
                        // save prev event state to undo
                        bettingsViewCache.SaveBettingUndo(undoId, {CBettingUndo{BettingUndoVariant{plEvent}, (uint32_t)height}});

                        plEvent.nTotalPoints    = totalsEvent.nPoints;
                        plEvent.nTotalOverOdds  = totalsEvent.nOverOdds;
                        plEvent.nTotalUnderOdds = totalsEvent.nUnderOdds;

                        // Update the event in the DB.
                        if (!bettingsViewCache.events->Update(eventKey, plEvent))
                            LogPrintf("Failed to update event!\n");
                    }
                    else {
                        LogPrintf("Failed to find event!\n");
                    }
                    continue;
                }
            }
        }
    }

    LogPrintf("ParseBettingTx: end\n");
}

int GetActiveChainHeight(const bool lockHeld)
{
    if (lockHeld) {
        AssertLockHeld(cs_main);
        return chainActive.Height();
    }

    LOCK(cs_main);
    return chainActive.Height();
}

bool RecoveryBettingDB(boost::signals2::signal<void(const std::string&)> & progress)
{
    return true;
}

bool UndoEventChanges(CBettingsView& bettingsViewCache, const BettingUndoKey& undoKey, const uint32_t height)
{
    std::vector<CBettingUndo> vUndos = bettingsViewCache.GetBettingUndo(undoKey);
    for (auto undo : vUndos) {
        // undo data is inconsistent
        if (!undo.Inited() || undo.Get().which() != UndoPeerlessEvent || undo.height != height) {
            std::runtime_error("Invalid undo state!");
        }
        else {
            CPeerlessEvent event = boost::get<CPeerlessEvent>(undo.Get());
            LogPrintf("UndoEventChanges: CPeerlessEvent: id: %lu, sport: %lu, tournament: %lu, stage: %lu,\n\t\t\thome: %lu, away: %lu, homeOdds: %lu, awayOdds: %lu, drawOdds: %lu\n",
                            event.nEventId,
                            event.nSport,
                            event.nTournament,
                            event.nStage,
                            event.nHomeTeam,
                            event.nAwayTeam,
                            event.nHomeOdds,
                            event.nAwayOdds,
                            event.nDrawOdds);
            LogPrintf("%s - Favorite: %s\n", __func__, event.fLegacyInitialHomeFavorite ? "home" : "away");

            if (!bettingsViewCache.events->Update(EventKey{event.nEventId}, event))
                std::runtime_error("Couldn't revert event when undo!");
        }
    }

    return bettingsViewCache.EraseBettingUndo(undoKey);
}

bool UndoBettingTx(CBettingsView& bettingsViewCache, const CTransaction& tx, const uint32_t height, const int64_t blockTime)
{
    // Ensure the event TX has come from Oracle wallet.
    const CTxIn& txin{tx.vin[0]};
    const bool validOracleTx{IsValidOracleTx(txin)};

    LogPrintf("UndoBettingTx: start undo, block heigth %lu, tx hash %s\n", height, tx.GetHash().GetHex());

    bool parlayBetsAvaible = height >= Params().WagerrProtocolV3StartHeight();

    // First revert OMNO transactions
    if (validOracleTx) {
        for (unsigned int i = 0; i < tx.vout.size(); i++) {
            const CTxOut& txout = tx.vout[i];
            std::string s = txout.scriptPubKey.ToString();

            COutPoint out(tx.GetHash(), i);
            const uint256 undoId = SerializeHash(out);

            if (0 == strncmp(s.c_str(), "OP_RETURN", 9)) {
                std::vector<unsigned char> v = ParseHex(s.substr(9, std::string::npos));
                std::string opCode(v.begin(), v.end());
                std::string opCodeHexStr = s.substr(10);
                // If mapping - just remove
                CMapping mapping{};
                if (CMapping::FromOpCode(opCode, mapping)) {
                    LogPrintf("CMapping: type: %lu, id: %lu, name: %s\n", mapping.nMType, mapping.nId, mapping.sName);
                    MappingKey mappingKey{mapping.nMType, mapping.nId};
                    if (!bettingsViewCache.mappings->Erase(mappingKey)) {
                        LogPrintf("Revert failed!\n");
                        return false;
                    }
                    continue;
                }

                // If events - just remove
                CPeerlessEvent plEvent{};
                if (CPeerlessEvent::FromOpCode(opCode, plEvent)) {
                    LogPrintf("CPeerlessEvent: id: %lu, sport: %lu, tournament: %lu, stage: %lu,\n\t\t\thome: %lu, away: %lu, homeOdds: %lu, awayOdds: %lu, drawOdds: %lu\n",
                        plEvent.nEventId,
                        plEvent.nSport,
                        plEvent.nTournament,
                        plEvent.nStage,
                        plEvent.nHomeTeam,
                        plEvent.nAwayTeam,
                        plEvent.nHomeOdds,
                        plEvent.nAwayOdds,
                        plEvent.nDrawOdds);
                    EventKey eventKey{plEvent.nEventId};
                    if (!bettingsViewCache.events->Erase(eventKey)) {
                        LogPrintf("Revert failed!\n");
                        return false;
                    }
                    continue;
                }

                // If event patch - find event undo and revert changes
                CPeerlessEventPatch plEventPatch{};
                if (CPeerlessEventPatch::FromOpCode(opCode, plEventPatch) &&
                        bettingsViewCache.events->Exists(EventKey{plEventPatch.nEventId})) {
                    LogPrintf("CPeerlessEventPatch: id: %lu, time: %lu\n", plEventPatch.nEventId, plEventPatch.nStartTime);
                    if (!UndoEventChanges(bettingsViewCache, undoId, height)) {
                        LogPrintf("Revert failed!\n");
                        return false;
                    }
                    continue;
                }

                // If results - just remove
                CPeerlessResult plResult{};
                if (CPeerlessResult::FromOpCode(opCode, plResult)) {
                    ResultKey resultKey{plResult.nEventId};
                    LogPrintf("CPeerlessResult: id: %lu, resultType: %lu, homeScore: %lu, awayScore: %lu\n", plResult.nEventId, plResult.nResultType, plResult.nHomeScore, plResult.nAwayScore);
                    if (!bettingsViewCache.results->Erase(resultKey)) {
                        LogPrintf("Revert failed!\n");
                        return false;
                    }
                    continue;
                }

                // If update money line odds - find event undo and revert changes
                CPeerlessUpdateOdds puo{};
                if (CPeerlessUpdateOdds::FromOpCode(opCode, puo) &&
                        bettingsViewCache.events->Exists(EventKey{puo.nEventId})) {
                    LogPrintf("CPeerlessUpdateOdds: id: %lu, homeOdds: %lu, awayOdds: %lu, drawOdds: %lu\n", puo.nEventId, puo.nHomeOdds, puo.nAwayOdds, puo.nDrawOdds);
                    if (!UndoEventChanges(bettingsViewCache, undoId, height)) {
                        LogPrintf("Revert failed!\n");
                        return false;
                    }
                    continue;
                }

                // If spread odds - find event undo and revert changes
                CPeerlessSpreadsEvent spreadEvent{};
                if (CPeerlessSpreadsEvent::FromOpCode(opCode, spreadEvent) &&
                        bettingsViewCache.events->Exists(EventKey{spreadEvent.nEventId})) {
                    LogPrintf("CPeerlessSpreadsEvent: id: %lu, spreadPoints: %lu, homeOdds: %lu, awayOdds: %lu\n", spreadEvent.nEventId, spreadEvent.nPoints, spreadEvent.nHomeOdds, spreadEvent.nAwayOdds);
                    if (!UndoEventChanges(bettingsViewCache, undoId, height)) {
                        LogPrintf("Revert failed!\n");
                        return false;
                    }
                    continue;
                }

                // If total odds - find event undo and revert changes
                CPeerlessTotalsEvent totalsEvent{};
                if (CPeerlessTotalsEvent::FromOpCode(opCode, totalsEvent) &&
                        bettingsViewCache.events->Exists(EventKey{totalsEvent.nEventId})) {
                    LogPrintf("CPeerlessTotalsEvent: id: %lu, totalPoints: %lu, overOdds: %lu, underOdds: %lu\n", totalsEvent.nEventId, totalsEvent.nPoints, totalsEvent.nOverOdds, totalsEvent.nUnderOdds);
                    if (!UndoEventChanges(bettingsViewCache, undoId, height)) {
                        LogPrintf("Revert failed!\n");
                        return false;
                    }
                    continue;
                }
            }
        }
    }

    // Search for any bets
    for (unsigned int i = 0; i < tx.vout.size(); i++) {
        const CTxOut& txout = tx.vout[i];
        std::string s = txout.scriptPubKey.ToString();

        COutPoint out(tx.GetHash(), i);
        const uint256 undoId = SerializeHash(out);

        if (0 == strncmp(s.c_str(), "OP_RETURN", 9)) {
            std::vector<unsigned char> v = ParseHex(s.substr(9, std::string::npos));
            std::string opCode(v.begin(), v.end());
            std::string opCodeHexStr = s.substr(10);

            CPeerlessBet plBet;
            // If bet - try to find event undo and revert changes
            if (CPeerlessBet::FromOpCode(opCode, plBet) &&
                    bettingsViewCache.events->Exists(EventKey{plBet.nEventId})) {
                LogPrintf("CPeerlessBet: id: %lu, outcome: %lu\n", plBet.nEventId, plBet.nOutcome);
                if (!UndoEventChanges(bettingsViewCache, undoId, height)) {
                    LogPrintf("Revert failed!\n");
                    return false;
                }
                UniversalBetKey key{static_cast<uint32_t>(height), out};
                bettingsViewCache.bets->Erase(key);
                continue;
            }
            std::vector<CPeerlessBet> legs;
            if (parlayBetsAvaible && CPeerlessBet::ParlayFromOpCode(opCodeHexStr, legs)) {
                 LogPrintf("ParlayBet: legs: ");
                 for (auto leg : legs) {
                     LogPrintf("(id: %lu, outcome: %lu), ", leg.nEventId, leg.nOutcome);
                 }
                 LogPrintf("\n");
                // delete duplicated legs
                std::sort(legs.begin(), legs.end());
                legs.erase(std::unique(legs.begin(), legs.end()), legs.end());

                bool allEventsExist = true;

                for (auto leg : legs) {
                    if (!bettingsViewCache.events->Exists(EventKey{leg.nEventId}))
                        allEventsExist = false;
                }

                if (!legs.empty() && allEventsExist) {
                    if (!UndoEventChanges(bettingsViewCache, undoId, height)) {
                        LogPrintf("Revert failed!\n");
                        return false;
                    }
                    UniversalBetKey key{static_cast<uint32_t>(height), out};
                    bettingsViewCache.bets->Erase(key);
                }
                continue;
            }
            CQuickGamesTxBet qgTxBet;
            if (CQuickGamesTxBet::FromOpCode(opCodeHexStr, qgTxBet)) {
                LogPrintf("CQuickGamesTxBet: gameType: %d, betInfo: %s", qgTxBet.gameType, std::string(qgTxBet.vBetInfo.begin(), qgTxBet.vBetInfo.end()));
                if (!bettingsViewCache.quickGamesBets->Erase(QuickGamesBetKey{static_cast<uint32_t>(height), out})) {
                    LogPrintf("Revert failed!\n");
                    return false;
                }
                continue;
            }
        }
    }
    LogPrintf("UndoBettingTx: end\n");
    return true;
}

/* Revert payouts info from DB */
bool UndoPayoutsInfo(CBettingsView &bettingsViewCache, int height)
{
    // we should save array of entries to delete because
    // changing (add/delete) of flushable DB when iterating is not allowed
    std::vector<PayoutInfoKey> entriesToDelete;
    auto it = bettingsViewCache.payoutsInfo->NewIterator();
    for (it->Seek(CBettingDB::DbTypeToBytes(PayoutInfoKey{static_cast<uint32_t>(height), COutPoint()})); it->Valid(); it->Next()) {
        PayoutInfoKey key;
        CBettingDB::BytesToDbType(it->Key(), key);
        if (key.blockHeight != height)
            break;
        else
            entriesToDelete.emplace_back(key);
    }

    // delete all entries with height of disconnected block
    for (auto&& key : entriesToDelete) {
        if (!bettingsViewCache.payoutsInfo->Erase(key))
            return false;
    }

    return true;
}
