// Copyright (c) 2018 The Wagerr developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef WAGERR_BET_H
#define WAGERR_BET_H

#include "util.h"
#include "chainparams.h"
#include "leveldbwrapper.h"
#include <flushablestorage/flushablestorage.h>
#include <boost/variant.hpp>
#include <boost/filesystem.hpp>
#include <boost/signals2/signal.hpp>
#include <boost/exception/to_string.hpp>
// The supported bet outcome types.
typedef enum OutcomeType {
    moneyLineWin  = 0x01,
    moneyLineLose = 0x02,
    moneyLineDraw = 0x03,
    spreadHome    = 0x04,
    spreadAway    = 0x05,
    totalOver     = 0x06,
    totalUnder    = 0x07
} OutcomeType;

// The supported result types
typedef enum ResultType {
    standardResult = 0x01,
    eventRefund    = 0x02,
    mlRefund       = 0x03,
    spreadsRefund  = 0x04,
    totalsRefund   = 0x05,
} ResultType;

// The supported result types
typedef enum WinnerType {
    homeWin = 0x01,
    awayWin = 0x02,
    push    = 0x03,
} WinnerType;

// The supported betting TX types.
typedef enum BetTxTypes{
    mappingTxType        = 0x01,  // Mapping transaction type identifier.
    plEventTxType        = 0x02,  // Peerless event transaction type identifier.
    plBetTxType          = 0x03,  // Peerless Bet transaction type identifier.
    plResultTxType       = 0x04,  // Peerless Result transaction type identifier.
    plUpdateOddsTxType   = 0x05,  // Peerless update odds transaction type identifier.
    cgEventTxType        = 0x06,  // Chain games event transaction type identifier.
    cgBetTxType          = 0x07,  // Chain games bet transaction type identifier.
    cgResultTxType       = 0x08,  // Chain games result transaction type identifier.
    plSpreadsEventTxType = 0x09,  // Spread odds transaction type identifier.
    plTotalsEventTxType  = 0x0a,  // Totals odds transaction type identifier.
    plEventPatchTxType   = 0x0b   // Peerless event patch transaction type identifier.
} BetTxTypes;

// The supported mapping TX types.
typedef enum MappingTypes {
    sportMapping      = 0x01,
    roundMapping      = 0x02,
    teamMapping       = 0x03,
    tournamentMapping = 0x04
} MappingTypes;

// Class derived from CTxOut
// nBetValue is NOT serialized, nor is it included in the hash.
class CBetOut : public CTxOut {
    public:

    CAmount nBetValue;
    uint32_t nEventId;

    CBetOut() : CTxOut() {
        SetNull();
    }

    CBetOut(const CAmount& nValueIn, CScript scriptPubKeyIn) : CTxOut(nValueIn, scriptPubKeyIn), nBetValue(0), nEventId(0) {};

    CBetOut(const CAmount& nValueIn, CScript scriptPubKeyIn, const CAmount& nBetValueIn) :
            CTxOut(nValueIn, scriptPubKeyIn), nBetValue(nBetValueIn), nEventId(0) {};

    CBetOut(const CAmount& nValueIn, CScript scriptPubKeyIn, const CAmount& nBetValueIn, uint32_t nEventIdIn) :
            CTxOut(nValueIn, scriptPubKeyIn), nBetValue(nBetValueIn), nEventId(nEventIdIn) {};

    void SetNull() {
        CTxOut::SetNull();
        nBetValue = -1;
        nEventId = -1;
    }

    void SetEmpty() {
        CTxOut::SetEmpty();
        nBetValue = 0;
        nEventId = 0;
    }

    bool IsEmpty() const {
        return CTxOut::IsEmpty() && nBetValue == 0 && nEventId == 0;
    }
};

class CPeerlessEvent
{
public:
    int nVersion;

    uint32_t nEventId;
    uint64_t nStartTime;
    uint32_t nSport;
    uint32_t nTournament;
    uint32_t nStage;
    uint32_t nHomeTeam;
    uint32_t nAwayTeam;
    uint32_t nHomeOdds;
    uint32_t nAwayOdds;
    uint32_t nDrawOdds;
    uint32_t nSpreadPoints;
    uint32_t nSpreadHomeOdds;
    uint32_t nSpreadAwayOdds;
    uint32_t nTotalPoints;
    uint32_t nTotalOverOdds;
    uint32_t nTotalUnderOdds;
    uint32_t nMoneyLineHomePotentialLiability;
    uint32_t nMoneyLineAwayPotentialLiability;
    uint32_t nMoneyLineDrawPotentialLiability;
    uint32_t nSpreadHomePotentialLiability;
    uint32_t nSpreadAwayPotentialLiability;
    uint32_t nSpreadPushPotentialLiability;
    uint32_t nTotalOverPotentialLiability;
    uint32_t nTotalUnderPotentialLiability;
    uint32_t nTotalPushPotentialLiability;
    uint32_t nMoneyLineHomeBets;
    uint32_t nMoneyLineAwayBets;
    uint32_t nMoneyLineDrawBets;
    uint32_t nSpreadHomeBets;
    uint32_t nSpreadAwayBets;
    uint32_t nSpreadPushBets;
    uint32_t nTotalOverBets;
    uint32_t nTotalUnderBets;
    uint32_t nTotalPushBets;

    // Default Constructor.
    CPeerlessEvent() {}

    static bool ToOpCode(CPeerlessEvent pe, std::string &opCode);
    static bool FromOpCode(std::string opCode, CPeerlessEvent &pe);

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp (Stream& s, Operation ser_action, int nType, int nVersion) {
        READWRITE(this->nVersion);
        nVersion = this->nVersion;
        READWRITE(nEventId);
        READWRITE(nStartTime);
        READWRITE(nSport);
        READWRITE(nTournament);
        READWRITE(nStage);
        READWRITE(nHomeTeam);
        READWRITE(nAwayTeam);
        READWRITE(nHomeOdds);
        READWRITE(nAwayOdds);
        READWRITE(nDrawOdds);
        READWRITE(nSpreadPoints);
        READWRITE(nSpreadHomeOdds);
        READWRITE(nSpreadAwayOdds);
        READWRITE(nTotalPoints);
        READWRITE(nTotalOverOdds);
        READWRITE(nTotalUnderOdds);
        READWRITE(nMoneyLineHomePotentialLiability);
        READWRITE(nMoneyLineAwayPotentialLiability);
        READWRITE(nMoneyLineDrawPotentialLiability);
        READWRITE(nSpreadHomePotentialLiability);
        READWRITE(nSpreadAwayPotentialLiability);
        READWRITE(nSpreadPushPotentialLiability);
        READWRITE(nTotalOverPotentialLiability);
        READWRITE(nTotalUnderPotentialLiability);
        READWRITE(nTotalPushPotentialLiability);
        READWRITE(nMoneyLineHomeBets);
        READWRITE(nMoneyLineAwayBets);
        READWRITE(nMoneyLineDrawBets);
        READWRITE(nSpreadHomeBets);
        READWRITE(nSpreadAwayBets);
        READWRITE(nSpreadPushBets);
        READWRITE(nTotalOverBets);
        READWRITE(nTotalUnderBets);
        READWRITE(nTotalPushBets);
    }
};

class CPeerlessBet
{
public:
    uint32_t nEventId;
    OutcomeType nOutcome;

    // Default constructor.
    CPeerlessBet() {}

    // Parametrized constructor.
    CPeerlessBet(int eventId, OutcomeType outcome)
    {
        nEventId = eventId;
        nOutcome = outcome;
    }

    static bool ToOpCode(CPeerlessBet pb, std::string &opCode);
    static bool FromOpCode(std::string opCode, CPeerlessBet &pb);
};

class CPeerlessResult
{
public:
    int nVersion;

    uint32_t nEventId;
    uint32_t nResultType;
    uint32_t nHomeScore;
    uint32_t nAwayScore;


    // Default Constructor.
    CPeerlessResult() {}

    // Parametrized Constructor.
    CPeerlessResult(int eventId, int pResultType, int pHomeScore, int pAwayScore)
    {
        nEventId    = eventId;
        nResultType = pResultType;
        nHomeScore  = pHomeScore;
        nAwayScore  = pAwayScore;
    }

    static bool ToOpCode(CPeerlessResult pr, std::string &opCode);
    static bool FromOpCode(std::string opCode, CPeerlessResult &pr);

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp (Stream& s, Operation ser_action, int nType, int nVersion) {
        READWRITE(this->nVersion);
        nVersion = this->nVersion;
        READWRITE(nEventId);
        READWRITE(nResultType);
        READWRITE(nHomeScore);
        READWRITE(nAwayScore);
    }
};

class CPeerlessUpdateOdds
{
public:
    uint32_t nEventId;
    uint32_t nHomeOdds;
    uint32_t nAwayOdds;
    uint32_t nDrawOdds;

    // Default Constructor.
    CPeerlessUpdateOdds() {}

    static bool ToOpCode(CPeerlessUpdateOdds puo, std::string &opCode);
    static bool FromOpCode(std::string opCode, CPeerlessUpdateOdds &puo);
};

class CChainGamesEvent
{
public:
    uint32_t nEventId;
    uint32_t nEntryFee;

    // Default Constructor.
    CChainGamesEvent() {}

    static bool ToOpCode(CChainGamesEvent cge, std::string &opCode);
    static bool FromOpCode(std::string opCode, CChainGamesEvent &cge);
};

class CChainGamesBet
{
public:
    uint32_t nEventId;

    // Default Constructor.
    CChainGamesBet() {}

    // Parametrized Constructor.
    CChainGamesBet(int eventId) {
        nEventId = eventId;
    }

    static bool ToOpCode(CChainGamesBet cgb, std::string &opCode);
    static bool FromOpCode(std::string opCode, CChainGamesBet &cgb);
};

class CChainGamesResult
{
public:
    uint16_t nEventId;

    // Default Constructor.
    CChainGamesResult() {}

    CChainGamesResult(uint16_t nEventId) : nEventId(nEventId) {};

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion)
    {
        READWRITE(nEventId);
    }

    bool FromScript(CScript script);

    static bool ToOpCode(CChainGamesResult cgr, std::string &opCode);
    static bool FromOpCode(std::string opCode, CChainGamesResult &cgr);
};

class CPeerlessSpreadsEvent
{
public:
    uint32_t nEventId;
    uint32_t nPoints;
    uint32_t nHomeOdds;
    uint32_t nAwayOdds;

    // Default Constructor.
    CPeerlessSpreadsEvent() {}

    static bool ToOpCode(CPeerlessSpreadsEvent pse, std::string &opCode);
    static bool FromOpCode(std::string opCode, CPeerlessSpreadsEvent &pse);
};

class CPeerlessTotalsEvent
{
public:
    uint32_t nEventId;
    uint32_t nPoints;
    uint32_t nOverOdds;
    uint32_t nUnderOdds;

    // Default Constructor.
    CPeerlessTotalsEvent() {}

    static bool ToOpCode(CPeerlessTotalsEvent pte, std::string &opCode);
    static bool FromOpCode(std::string opCode, CPeerlessTotalsEvent &pte);
};

class CPeerlessEventPatch
{
public:
    int nVersion;
    uint32_t nEventId;
    uint64_t nStartTime;

    CPeerlessEventPatch() {}

    static bool ToOpCode(CPeerlessEventPatch pe, std::string &opCode);
    static bool FromOpCode(std::string opCode, CPeerlessEventPatch &pe);

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp (Stream& s, Operation ser_action, int nType, int nVersion) {
        READWRITE(this->nVersion);
        nVersion = this->nVersion;
        READWRITE(nEventId);
        READWRITE(nStartTime);
    }
};

class CMapping
{
public:
    int nVersion;

    uint32_t nMType;
    uint32_t nId;
    std::string sName;

    CMapping() {}

    MappingTypes GetType() const;

    static std::string ToTypeName(MappingTypes type);
    static MappingTypes FromTypeName(const std::string& name);

    static bool ToOpCode(const CMapping& mapping, std::string &opCode);
    static bool FromOpCode(std::string opCode, CMapping &mapping);

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp (Stream& s, Operation ser_action, int nType, int nVersion) {
        READWRITE(this->nVersion);
        nVersion = this->nVersion;
        READWRITE(nMType);
        READWRITE(nId);
        READWRITE(sName);
    }
};

// DataBase Code

// MappingKey
typedef struct MappingKey {
    uint32_t nMType;
    uint32_t nId;

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp (Stream& s, Operation ser_action, int nType, int nVersion) {
        uint32_t be_val;
        if (ser_action.ForRead()) {
            READWRITE(be_val);
            nMType = ntohl(be_val);
            READWRITE(be_val);
            nId = ntohl(be_val);
        }
        else {
            be_val = htonl(nMType);
            READWRITE(be_val);
            be_val = htonl(nId);
            READWRITE(be_val);
        }
    }
} MappingKey;

// ResultKey
typedef struct ResultKey {
    uint32_t eventId;

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp (Stream& s, Operation ser_action, int nType, int nVersion) {
        uint32_t be_val;
        if (ser_action.ForRead()) {
            READWRITE(be_val);
            eventId = ntohl(be_val);
        }
        else {
            be_val = htonl(eventId);
            READWRITE(be_val);
        }
    }
} ResultKey;

// EventKey
using EventKey = ResultKey;

// UndoKey
using BettingUndoKey = uint256;

using BettingUndoVariant = boost::variant<CMapping, CPeerlessEvent, CPeerlessResult>;

typedef enum BettingUndoTypes {
    UndoMapping,
    UndoPeerlessEvent,
    UndoPeerlessResult
} BettingUndoTypes;

class CBettingUndo
{
public:
    uint32_t height = 0;

    CBettingUndo() { }

    CBettingUndo(const BettingUndoVariant& undoVar, const uint32_t height) : height{height}, undoVariant{undoVar} { }

    bool Inited() {
        return !undoVariant.empty();
    }

    BettingUndoVariant Get() {
        return undoVariant;
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp (Stream& s, Operation ser_action, int nType, int nVersion) {
        READWRITE(height);
        int undoType;
        if (ser_action.ForRead()) {
            READWRITE(undoType);
            switch ((BettingUndoTypes)undoType)
            {
                case UndoMapping:
                {
                    CMapping mapping{};
                    READWRITE(mapping);
                    undoVariant = mapping;
                    break;
                }
                case UndoPeerlessEvent:
                {
                    CPeerlessEvent event{};
                    READWRITE(event);
                    undoVariant = event;
                    break;
                }
                case UndoPeerlessResult:
                {
                    CPeerlessResult result{};
                    READWRITE(result);
                    undoVariant = result;
                    break;
                }
                default:
                    std::runtime_error("Undefined undo type");
            }
        }
        else {
            undoType = undoVariant.which();
            READWRITE(undoType);
            switch ((BettingUndoTypes)undoType)
            {
                case UndoMapping:
                {
                    CMapping mapping = boost::get<CMapping>(undoVariant);
                    READWRITE(mapping);
                    break;
                }
                case UndoPeerlessEvent:
                {
                    CPeerlessEvent event = boost::get<CPeerlessEvent>(undoVariant);
                    READWRITE(event);
                    break;
                }
                case UndoPeerlessResult:
                {
                    CPeerlessResult result = boost::get<CPeerlessResult>(undoVariant);
                    READWRITE(result);
                    break;
                }
                default:
                    std::runtime_error("Undefined undo type");
            }
        }
    }

private:
    BettingUndoVariant undoVariant;
};

class CBettingDB
{
public:
    // Default Constructor.
    explicit CBettingDB(CStorageKV& db) : db{db} { }
    // Cache copy constructor (we should set global flushable storage ref as flushable storage of cached copy)
    explicit CBettingDB(CBettingDB& bdb) : CBettingDB(bdb.GetDb()) { }

    ~CBettingDB() {}

    bool Flush() { return db.Flush(); }

    std::unique_ptr<CStorageKVIterator> NewIterator() {
        return db.NewIterator();
    }

    template<typename KeyType>
    bool Exists(const KeyType& key) {
        return GetDb().Exists(DbTypeToBytes(key));
    }

    template<typename KeyType, typename ValueType>
    bool Write(const KeyType& key, const ValueType& value) {
        auto vKey = DbTypeToBytes(key);
        auto vValue = DbTypeToBytes(value);
        if (GetDb().Exists(vKey))
            return false;
        return GetDb().Write(vKey, vValue);
    }

    template<typename KeyType, typename ValueType>
    bool Update(const KeyType& key, const ValueType& value) {
        auto vKey = DbTypeToBytes(key);
        auto vValue = DbTypeToBytes(value);
        if (!GetDb().Exists(vKey))
            return false;
        return GetDb().Write(vKey, vValue);
    }

    template<typename KeyType>
    bool Erase(const KeyType& key) {
        if (!GetDb().Exists(DbTypeToBytes(key)))
            return false;
        return GetDb().Erase(DbTypeToBytes(key));
    }

    template<typename KeyType, typename ValueType>
    bool Read(const KeyType& key, ValueType& value) {
        std::vector<unsigned char> value_v;
        if (GetDb().Read(DbTypeToBytes(key), value_v)) {
            BytesToDbType(value_v, value);
            return true;
        }
        return false;
    }

    static std::size_t dbWrapperCacheSize() { return 10 << 20; }

    static std::string MakeDbPath(const char* name) {
        using namespace boost::filesystem;

        std::string result{};
        path dir{GetDataDir()};

        dir /= "betting";
        dir /= name;

        if (boost::filesystem::is_directory(dir) || boost::filesystem::create_directories(dir) ) {
            result = boost::to_string(dir);
            result.erase(0, 1);
            result.erase(result.size() - 1);
        }

        return result;
    }

    template<typename T>
    static std::vector<unsigned char> DbTypeToBytes(const T& value) {
        CDataStream stream(SER_DISK, CLIENT_VERSION);
        stream << value;
        return std::vector<unsigned char>(stream.begin(), stream.end());
    }

    template<typename T>
    static void BytesToDbType(const std::vector<unsigned char>& bytes, T& value) {
        CDataStream stream(bytes, SER_DISK, CLIENT_VERSION);
        stream >> value;
        assert(stream.size() == 0);
    }
protected:
    CFlushableStorageKV& GetDb() { return db; }
    CFlushableStorageKV db;
};

/** Container for several db objects */
class CBettingsView
{
    // fields will be init in init.cpp
public:
    std::unique_ptr<CBettingDB> mappings; // "mappings"
    std::unique_ptr<CStorageKV> mappingsStorage;
    std::unique_ptr<CBettingDB> results; // "results"
    std::unique_ptr<CStorageKV> resultsStorage;
    std::unique_ptr<CBettingDB> events; // "events"
    std::unique_ptr<CStorageKV> eventsStorage;
    std::unique_ptr<CBettingDB> undos; // "undos"
    std::unique_ptr<CStorageKV> undosStorage;

    // default constructor
    CBettingsView() { }

    // copy constructor for creating DB cache
    CBettingsView(CBettingsView* phr) {
        mappings = MakeUnique<CBettingDB>(*phr->mappings.get());
        results = MakeUnique<CBettingDB>(*phr->results.get());
        events = MakeUnique<CBettingDB>(*phr->events.get());
        undos = MakeUnique<CBettingDB>(*phr->undos.get());
    }

    bool Flush() {
        return mappings->Flush() && results->Flush() && events->Flush() && undos->Flush();
    }

    void SetLastHeight(uint32_t height) {
        if (!undos->Exists(std::string("LastHeight"))) {
            undos->Write(std::string("LastHeight"), height);
        }
        else {
            undos->Update(std::string("LastHeight"), height);
        }
    }

    uint32_t GetLastHeight() {
        uint32_t height;
        if (!undos->Read(std::string("LastHeight"), height))
            return 0;
        return height;
    }

    bool SaveBettingUndo(const BettingUndoKey& key, CBettingUndo undo) {
        assert(!undos->Exists(key));
        return undos->Write(key, undo);
    }

    bool EraseBettingUndo(const BettingUndoKey& key) {
        return undos->Erase(key);
    }

    CBettingUndo GetBettingUndo(const BettingUndoKey& key) {
        CBettingUndo undo;
        if (undos->Read(key, undo))
            return undo;
        else
            return CBettingUndo();
    }

    void PruneOlderUndos(const uint32_t height) {
        CBettingUndo undo;
        BettingUndoKey key;
        std::string str;
        auto it = undos->NewIterator();
        std::vector<unsigned char> lastHeightKey = CBettingDB::DbTypeToBytes(std::string("LastHeight"));
        for (it->Seek(std::vector<unsigned char>{}); it->Valid(); it->Next()) {
            // check that key is serialized "LastHeight" key and skip if true
            if (it->Key() == lastHeightKey) {
                continue;
            }
            CBettingDB::BytesToDbType(it->Key(), key);
            CBettingDB::BytesToDbType(it->Value(), undo);
            if (undo.height < height) {
                undos->Erase(key);
            }
        }
    }
};

extern CBettingsView *bettingsView;

/** Ensures a TX has come from an OMNO wallet. **/
bool IsValidOracleTx(const CTxIn &txin);

/** Aggregates the amount of WGR to be minted to pay out all bets as well as dev and OMNO rewards. **/
int64_t GetBlockPayouts(std::vector<CBetOut>& vExpectedPayouts, CAmount& nMNBetReward);

/** Aggregates the amount of WGR to be minted to pay out all CG Lotto winners as well as OMNO rewards. **/
int64_t GetCGBlockPayouts(std::vector<CBetOut>& vexpectedCGPayouts, CAmount& nMNBetReward);

/** Validating the payout block using the payout vector. **/
bool IsBlockPayoutsValid(std::vector<CBetOut> vExpectedPayouts, CBlock block);

/** Find peerless events. **/
std::vector<CPeerlessResult> getEventResults(int height);

/** Find chain games lotto result. **/
std::pair<std::vector<CChainGamesResult>,std::vector<std::string>> getCGLottoEventResults(int height);

/** Get the peerless winning bets from the block chain and return the payout vector. **/
std::vector<CBetOut> GetBetPayouts(int height);

/** Get the chain games winner and return the payout vector. **/
std::vector<CBetOut> GetCGLottoBetPayouts(int height);

/** Parse the transaction for betting data **/
void ParseBettingTx(CBettingsView& bettingsViewCache, const CTransaction& tx, const int height);

/** Get the chain height **/
int GetActiveChainHeight(const bool lockHeld = false);

bool RecoveryBettingDB(boost::signals2::signal<void(const std::string&)> & progress);

bool UndoBettingTx(CBettingsView& bettingsViewCache, const CTransaction& tx, const uint32_t height);


#endif // WAGERR_BET_H
