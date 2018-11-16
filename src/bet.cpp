// Copyright (c) 2018 The Wagerr developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "bet.h"
#include <boost/filesystem.hpp>

#ifdef ENABLE_WALLET
#include "wallet.h"
#endif

#define BTX_FORMAT_VERSION 0x01
#define BTX_HEX_PREFIX "425458"

// String lengths for all currently supported op codes.
#define PE_FROM_OP_STRLEN  49
#define PE_TO_OP_STRLEN    98
#define PB_FROM_OP_STRLEN  13
#define PB_TO_OP_STRLEN    26
#define PR_FROM_OP_STRLEN  13
#define PR_TO_OP_STRLEN    26
#define PUO_FROM_OP_STRLEN 21
#define PUO_TO_OP_STRLEN   42
#define CGE_FROM_OP_STRLEN 13
#define CGE_TO_OP_STRLEN   26
#define CGB_FROM_OP_STRLEN 9
#define CGB_TO_OP_STRLEN   18
#define CGR_FROM_OP_STRLEN 9
#define CGR_TO_OP_STRLEN   18

/**
 * Validate the transaction to ensure it has been posted by an oracle node.
 *
 * @param txin  TX vin input hash.
 * @return      Bool
 */
bool IsValidOracleTx(const CTxIn &txin)
{
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
uint32_t FromChars(unsigned char a, unsigned char b, unsigned char c, unsigned char d)
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
    if (opCode.length() != PE_FROM_OP_STRLEN) {
        // TODO - add proper error handling
        return false;
    }

    // Ensure the peerless event transaction type is correct.
    if (opCode[4] != plEventTxType) {
        // TODO - add proper error handling
        return false;
    }

    // Ensure the peerless event OpCode has the correct BTX format version number.
    if (ReadBTXFormatVersion(opCode) != BTX_FORMAT_VERSION) {
        // TODO - add proper error handling
        return false;
    }

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
    if (opCode.length() != PE_TO_OP_STRLEN) {
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
    if (opCode.length() != PB_FROM_OP_STRLEN) {
        // TODO - add proper error handling
        return false;
    }

    // Ensure the peerless bet transaction type is correct.
    if (opCode[4] != plBetTxType) {
        // TODO - add proper error handling
        return false;
    }

    // Ensure the peerless bet OpCode has the correct BTX format version number.
    if (ReadBTXFormatVersion(opCode) != BTX_FORMAT_VERSION) {
        // TODO - add proper error handling
        return false;
    }

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
    if (opCode.length() != PB_TO_OP_STRLEN) {
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
    if (opCode.length() != PR_FROM_OP_STRLEN) {
        // TODO - add proper error handling
        return false;
    }

    // Ensure the peerless result transaction type is correct.
    if (opCode[4] != plResultTxType) {
        // TODO - add proper error handling
        return false;
    }

    // Ensure the peerless result OpCode has the correct BTX format version number.
    if (ReadBTXFormatVersion(opCode) != BTX_FORMAT_VERSION) {
        // TODO - add proper error handling
        return false;
    }

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
    if (opCode.length() != PR_TO_OP_STRLEN) {
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
    if (opCode.length() != PUO_FROM_OP_STRLEN) {
        // TODO - add proper error handling
        return false;
    }

    // Ensure the peerless update odds transaction type is correct.
    if (opCode[4] != plUpdateOddsTxType) {
        // TODO - add proper error handling
        return false;
    }

    // Ensure the peerless update odds OpCode has the correct BTX format version number.
    if (ReadBTXFormatVersion(opCode) != BTX_FORMAT_VERSION) {
        // TODO - add proper error handling
        return false;
    }

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
    if (opCode.length() != PUO_TO_OP_STRLEN) {
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
    if (opCode.length() != CGE_FROM_OP_STRLEN) {
        // TODO - add proper error handling
        return false;
    }

    // Ensure the chain game event transaction type is correct.
    if (opCode[4] != cgEventTxType) {
        // TODO - add proper error handling
        return false;
    }

    // Ensure the chain game event OpCode has the correct BTX format version number.
    if (ReadBTXFormatVersion(opCode) != BTX_FORMAT_VERSION) {
        // TODO - add proper error handling
        return false;
    }

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
    if (opCode.length() != CGE_TO_OP_STRLEN) {
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
    if (opCode.length() != CGB_FROM_OP_STRLEN) {
        // TODO - add proper error handling
        return false;
    }

    // Ensure the chain game bet transaction type is correct.
    if (opCode[4] != cgBetTxType) {
        // TODO - add proper error handling
        return false;
    }

    // Ensure the chain game bet OpCode has the correct BTX format version number.
    if (ReadBTXFormatVersion(opCode) != BTX_FORMAT_VERSION) {
        // TODO - add proper error handling
        return false;
    }

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
    if (opCode.length() != CGB_TO_OP_STRLEN) {
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
    if (opCode.length() != CGR_FROM_OP_STRLEN) {
        // TODO - add proper error handling
        return false;
    }

    // Ensure the chain game result transaction type is correct.
    if (opCode[4] != cgResultTxType) {
        // TODO - add proper error handling
        return false;
    }

    // Ensure the chain game result OpCode has the correct BTX format version number.
    if (ReadBTXFormatVersion(opCode) != BTX_FORMAT_VERSION) {
        // TODO - add proper error handling
        return false;
    }

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
    if (opCode.length() != CGR_TO_OP_STRLEN) {
        // TODO - add proper error handling
        return false;
    }

    return true;
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
    if (opCode[4] != mappingTxType) {
        // TODO - add proper error handling
        return false;
    }

    // Ensure the mapping OpCode has the correct BTX format version number.
    if (ReadBTXFormatVersion(opCode) != BTX_FORMAT_VERSION) {
        // TODO - add proper error handling
        return false;
    }

    cm.nMType = FromChars(opCode[5], opCode[6], opCode[7], opCode[8]);
    cm.nId    = FromChars(opCode[9], opCode[10], opCode[11], opCode[12]);

    // Decode the the rest of the mapping OP Code to get the name.
    std::string name;
    int nextOpIndex = 13;

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
        return error("%s : Failed to open file %s", __func__, pathTemp.c_str());

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
        return error("%s : Failed to open file %s", __func__, pathTemp.c_str());

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
 * Reads the events.dat file and deserializes the data to recreate event index map object as well as the last block hash before file was written to.
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
