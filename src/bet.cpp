// Copyright (c) 2018 The Wagerr developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "bet.h"
#include "main.h"
#include "chainparams.h"

#ifdef ENABLE_WALLET
#include "wallet.h"
#endif

#include <cstring>
#include <vector>
#include <string>
#include <iomanip>

#define BTX_FORMAT_VERSION 0x01
#define BTX_HEX_PREFIX "425458"

/**
 * Validate the transaction to ensure it has been posted by an oracle node.
 *
 * @param txin  TX vin input hash.
 * @return      Bool
 */
bool ValidOracleTx(const CTxIn &txin)
{
    // Ensure if event TX that has it been posted by Oracle wallet.
    COutPoint prevout = txin.prevout;

    uint256 hashBlock;
    CTransaction txPrev;
    if (GetTransaction(prevout.hash, txPrev, hashBlock, true)) {

        const CTxOut &prevTxOut = txPrev.vout[0];
        std::string scriptPubKey = prevTxOut.scriptPubKey.ToString();

        txnouttype type;
        vector<CTxDestination> prevAddrs;
        int nRequired;

        if (ExtractDestinations(prevTxOut.scriptPubKey, type, prevAddrs, nRequired)) {
            BOOST_FOREACH (const CTxDestination &prevAddr, prevAddrs) {
                if (CBitcoinAddress(prevAddr).ToString() == Params().OracleWalletAddr()) {
                    return true;
                }
            }
        }
    }

    return false;
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
    if (opCode[0] != 'B' || opCode[1] != 'T' || opCode[2] != 'X') {
        return -1;
    }

    // Check the BTX protocol version number is in range.
    int v = opCode[3];

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
uint32_t FromChars(char a, char b, char c, char d)
{
    uint32_t n = a;
    n <<= 8;
    n += b;
    n <<= 8;
    n += c;
    n <<= 8;
    n += d;

    return n;
}

/**
 * Convert a unsigned 32 bit integer into its hex equivalent with the
 * amount of zero padding given as argument length.
 *
 * @param value  The integer value
 * @param length The size in bits
 * @return       Hex string
 */
std::string ToHex(uint32_t value, int length)
{
    std::stringstream strBuffer;
    strBuffer << std::hex << std::setw(length) << std::setfill('0') << value;

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
    if (opCode.length() != PEFromOp) {
        // TODO - add proper error handling
        return false;
    }

    // Ensure the peerless event transaction type is correct.
    if (opCode[4] != Params().PLEventTxType()) {
        // TODO - add proper error handling
        return false;
    }

    // Ensure the peerless event OpCode has the correct BTX format version number.
    if (ReadBTXFormatVersion(opCode) != BTX_FORMAT_VERSION) {
        // TODO - add proper error handling
        return false;
    }

    //printf(">2> %d %d %d %d\n", (char) opCode[9], (char) opCode[10], (char) opCode[11], (char) opCode[12]);

    pe.nEventId       = FromChars(opCode[5], opCode[6], opCode[7], opCode[8]);
    uint64_t starting = FromChars(opCode[9], opCode[10], opCode[11], opCode[12]);
    starting          <<= 32;
    starting          +=FromChars(opCode[13], opCode[14], opCode[15], opCode[16]);
    pe.nStartTime     = starting;
    pe.nSport         = FromChars(opCode[17], opCode[18], opCode[19], opCode[20]);
    pe.nTournament    = FromChars(opCode[21], opCode[22], opCode[23], opCode[24]);
    pe.nStage         = FromChars(opCode[25], opCode[26], opCode[27], opCode[28]);
    pe.nHomeTeam      = FromChars(opCode[29], opCode[30], opCode[31], opCode[32]);
    pe.nAwayTeam      = FromChars(opCode[33], opCode[34], opCode[35], opCode[36]);
    pe.nHomeOdds      = FromChars(opCode[37], opCode[38], opCode[39], opCode[40]);
    pe.nAwayOdds      = FromChars(opCode[41], opCode[42], opCode[43], opCode[44]);
    pe.nDrawOdds      = FromChars(opCode[45], opCode[46], opCode[47], opCode[48]);

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
    std::string sStartTime1 = ToHex(pe.nStartTime >> 32, 8);
    std::string sStartTime2 = ToHex(pe.nStartTime , 8);
    std::string sSport      = ToHex(pe.nSport, 8);
    std::string sTournament = ToHex(pe.nTournament, 8);
    std::string sStage      = ToHex(pe.nStage, 8);
    std::string sHomeTeam   = ToHex(pe.nHomeTeam, 8);
    std::string sAwayTeam   = ToHex(pe.nAwayTeam, 8);
    std::string sHomeOdds   = ToHex(pe.nHomeOdds, 8);
    std::string sAwayOdds   = ToHex(pe.nAwayOdds, 8);
    std::string sDrawOdds   = ToHex(pe.nDrawOdds, 8);

    opCode = BTX_HEX_PREFIX "0102" + sEventId + sStartTime1 + sStartTime2 + sSport + sTournament +
             sStage + sHomeTeam + sAwayTeam + sHomeOdds + sAwayOdds + sDrawOdds;

    // Ensure peerless Event OpCode string is the correct length.
    if( opCode.length() != PEToOp){
        // TODO - add proper error handling
        return false;
    }

    //printf( "To Event OpCode %s \n", opCode.c_str() );

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
    if (opCode.length() != PBFromOp) {
        // TODO - add proper error handling
        return false;
    }

    // Ensure the peerless bet transaction type is correct.
    if (opCode[4] != Params().PLBetTxType()) {
        // TODO - add proper error handling
        return false;
    }

    // Ensure the peerless bet OpCode has the correct BTX format version number.
    if (ReadBTXFormatVersion(opCode) != BTX_FORMAT_VERSION) {
        // TODO - add proper error handling
        return false;
    }

    //printf(">2> %d %d %d %d\n", (char) opCode[5], (char) opCode[6], (char) opCode[7], (char) opCode[8]);

    pb.nEventId = FromChars(opCode[5], opCode[6], opCode[7], opCode[8]);
    uint32_t betOutcome = FromChars(opCode[9], opCode[10], opCode[11], opCode[12]);
    pb.nOutcome = (OutcomeType) betOutcome;

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
    std::string sOutcome = ToHex(pb.nOutcome, 8);

    opCode = BTX_HEX_PREFIX "0103" + sEventId + sOutcome;

    // Ensure peerless bet OpCode string is the correct length.
    if (opCode.length() != PBToOp) {
        // TODO - add proper error handling
        return false;
    }

    //printf( "To Bet OpCode %s \n", opCode.c_str() );

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
    if (opCode.length() != PRFromOp) {
        // TODO - add proper error handling
        return false;
    }

    // Ensure the peerless result transaction type is correct.
    if (opCode[4] != Params().PLResultTxType()) {
        // TODO - add proper error handling
        return false;
    }

    // Ensure the peerless result OpCode has the correct BTX format version number.
    if (ReadBTXFormatVersion(opCode) != BTX_FORMAT_VERSION) {
        // TODO - add proper error handling
        return false;
    }

    //printf(">2> %d %d %d %d\n", (char) opCode[5], (char) opCode[6], (char) opCode[7], (char) opCode[8]);

    pr.nEventId = FromChars(opCode[5], opCode[6], opCode[7], opCode[8]);
    uint32_t eventResult = FromChars(opCode[9], opCode[10], opCode[11], opCode[12]);
    pr.nResult = (ResultType) eventResult;

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
    std::string sEventId = ToHex(pr.nEventId, 8);
    std::string sResult  = ToHex(pr.nResult, 8);

    opCode = BTX_HEX_PREFIX "0104" + sEventId + sResult;

    // Ensure peerless result OpCode string is the correct length.
    if (opCode.length() != PRToOp) {
        // TODO - add proper error handling
        return false;
    }

    //printf( "To Result OpCode %s \n", opCode.c_str() );

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
    if (opCode.length() != PUOFromOp) {
        // TODO - add proper error handling
        return false;
    }

    // Ensure the peerless update odds transaction type is correct.
    if (opCode[4] != Params().PLUpdateOddsTxType()) {
        // TODO - add proper error handling
        return false;
    }

    // Ensure the peerless update odds OpCode has the correct BTX format version number.
    if (ReadBTXFormatVersion(opCode) != BTX_FORMAT_VERSION) {
        // TODO - add proper error handling
        return false;
    }

    //printf(">2> %d %d %d %d\n", (char) opCode[5], (char) opCode[6], (char) opCode[7], (char) opCode[8]);

    puo.nEventId  = FromChars(opCode[5], opCode[6], opCode[7], opCode[8]);
    puo.nHomeOdds = FromChars(opCode[9], opCode[10], opCode[11], opCode[12]);
    puo.nAwayOdds = FromChars(opCode[13], opCode[14], opCode[15], opCode[16]);
    puo.nDrawOdds = FromChars(opCode[17], opCode[18], opCode[19], opCode[20]);

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
    if (opCode.length() != PUOToOp) {
        // TODO - add proper error handling
        return false;
    }

    //printf( "To Result OpCode %s \n", opCode.c_str() );

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
    if (opCode.length() != CGEFromOp) {
        // TODO - add proper error handling
        return false;
    }

    // Ensure the chain game event transaction type is correct.
    if (opCode[4] != Params().CGEventTxType()) {
        // TODO - add proper error handling
        return false;
    }

    // Ensure the chain game event OpCode has the correct BTX format version number.
    if (ReadBTXFormatVersion(opCode) != BTX_FORMAT_VERSION) {
        // TODO - add proper error handling
        return false;
    }

    //printf(">2> %d %d %d %d\n", (char) opCode[5], (char) opCode[6], (char) opCode[7], (char) opCode[8]);

    cge.nEventId  = FromChars(opCode[5], opCode[6], opCode[7], opCode[8]);
    cge.nEntryFee = FromChars(opCode[9], opCode[10], opCode[11], opCode[12]);

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
    std::string sEventId  = ToHex(cge.nEventId, 8);
    std::string sEntryFee = ToHex(cge.nEntryFee, 8);

    opCode = BTX_HEX_PREFIX "0106" + sEventId + sEntryFee;

    // Ensure Chain Games event OpCode string is the correct length.
    if (opCode.length() != CGEToOp) {
        // TODO - add proper error handling
        return false;
    }

    //printf( "To CG Event OpCode %s \n", opCode.c_str() );

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
    if (opCode.length() != CGBFromOp) {
        // TODO - add proper error handling
        return false;
    }

    // Ensure the chain game bet transaction type is correct.
    if (opCode[4] != Params().CGBetTxType()) { // OP_CODE type.
        // TODO - add proper error handling
        return false;
    }

    // Ensure the chain game bet OpCode has the correct BTX format version number.
    if (ReadBTXFormatVersion(opCode) != BTX_FORMAT_VERSION) {
        // TODO - add proper error handling
        return false;
    }

    //printf(">2> %d %d %d %d\n", (char) opCode[5], (char) opCode[6], (char) opCode[7], (char) opCode[8]);

    cgb.nEventId = FromChars(opCode[5], opCode[6], opCode[7], opCode[8]);

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
    std::string sEventId  = ToHex(cgb.nEventId, 8);

    opCode = BTX_HEX_PREFIX "0107" + sEventId;

    // Ensure Chain Games bet OpCode string is the correct length.
    if (opCode.length() != CGBToOp) {
        // TODO - add proper error handling
        return false;
    }

    //printf( "To CG bet OpCode %s \n", opCode.c_str() );

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
    if (opCode.length() != CGRFromOp) {
        // TODO - add proper error handling
        return false;
    }

    // Ensure the chain game result transaction type is correct.
    if (opCode[4] != Params().CGResultTxType()) { // OP_CODE type.
        // TODO - add proper error handling
        return false;
    }

    // Ensure the chain game result OpCode has the correct BTX format version number.
    if (ReadBTXFormatVersion(opCode) != BTX_FORMAT_VERSION) {
        // TODO - add proper error handling
        return false;
    }

    //printf(">2> %d %d %d %d\n", (char) opCode[5], (char) opCode[6], (char) opCode[7], (char) opCode[8]);

    cgr.nEventId = FromChars(opCode[5], opCode[6], opCode[7], opCode[8]);

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
    std::string sEventId  = ToHex(cgr.nEventId, 8);

    opCode = BTX_HEX_PREFIX "0108" + sEventId;

    // Ensure Chain Games result OpCode string is the correct length.
    if (opCode.length() != CGRToOp) {
        // TODO - add proper error handling
        return false;
    }

    //printf( "To CG Result OpCode %s \n", opCode.c_str() );

    return true;
}
