// Copyright (c) 2020 The Wagerr developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef WAGERR_BET_TX_H
#define WAGERR_BET_TX_H

#include <util.h>
#include <serialize.h>

class CTxOut;

#define BTX_FORMAT_VERSION 0x01
#define BTX_PREFIX 'B'

// The supported betting TX types.
typedef enum BetTxTypes{
    mappingTxType            = 0x01,  // Mapping transaction type identifier.
    plEventTxType            = 0x02,  // Peerless event transaction type identifier.
    plBetTxType              = 0x03,  // Peerless Bet transaction type identifier.
    plResultTxType           = 0x04,  // Peerless Result transaction type identifier.
    plUpdateOddsTxType       = 0x05,  // Peerless update odds transaction type identifier.
    cgEventTxType            = 0x06,  // Chain games event transaction type identifier.
    cgBetTxType              = 0x07,  // Chain games bet transaction type identifier.
    cgResultTxType           = 0x08,  // Chain games result transaction type identifier.
    plSpreadsEventTxType     = 0x09,  // Spread odds transaction type identifier.
    plTotalsEventTxType      = 0x0a,  // Totals odds transaction type identifier.
    plEventPatchTxType       = 0x0b,  // Peerless event patch transaction type identifier.
    plParlayBetTxType        = 0x0c,  // Peerless Parlay Bet transaction type identifier.
    qgBetTxType              = 0x0d,  // Quick Games Bet transaction type identifier.
    plEventZeroingOddsTxType = 0x0e,
} BetTxTypes;

// class for serialization common betting header from opcode
class CBettingTxHeader
{
public:
    uint8_t prefix;
    uint8_t version;
    uint8_t txType;

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp (Stream& s, Operation ser_action, int nType, int nVersion) {
        READWRITE(prefix);
        READWRITE(version);
        READWRITE(txType);
    }
};

// Virtual class for all TX classes
class CBettingTx
{
public:
    virtual BetTxTypes GetTxType() const = 0;
};

class CMappingTx : public CBettingTx
{
public:

    uint8_t nMType;
    uint32_t nId;
    std::string sName;

    CMappingTx(): nMType(0), nId(0) {}

    BetTxTypes GetTxType() const override { return mappingTxType; }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp (Stream& s, Operation ser_action, int nType, int nVersion) {
        READWRITE(nMType);
        // if mapping type is teamMapping (0x03) nId is 4 bytes, else - 2 bytes
        if (nMType == 0x03) {
            uint32_t nId32;
            if (ser_action.ForRead()) {
                READWRITE(nId32);
                nId = nId32;
            }
            else {
                nId32 = nId;
                READWRITE(nId32);
            }
        }
        else {
            uint16_t nId16;
            if (ser_action.ForRead()) {
                READWRITE(nId16);
                nId = nId16;
            }
            else {
                nId16 = static_cast<uint16_t>(nId);
                READWRITE(nId16);
            }
        }
        // OMG string serialization
        char ch;
        if (ser_action.ForRead()) {
            sName.clear();
            while (s.size() != 0) {
                READWRITE(ch);
                sName += ch;
            }
        }
        else {
            for (size_t i = 0; i < sName.size(); i++) {
                ch = (uint8_t) sName[i];
                READWRITE(ch);
            }
        }
    }
};

/*
 * Peerless betting TX structures
 */

class CPeerlessEventTx : public CBettingTx
{
public:
    uint32_t nEventId;
    uint32_t nStartTime;
    uint16_t nSport;
    uint16_t nTournament;
    uint16_t nStage;
    uint32_t nHomeTeam;
    uint32_t nAwayTeam;
    uint32_t nHomeOdds;
    uint32_t nAwayOdds;
    uint32_t nDrawOdds;

    // Default Constructor.
    CPeerlessEventTx() {}

    BetTxTypes GetTxType() const override { return plEventTxType; }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp (Stream& s, Operation ser_action, int nType, int nVersion) {
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
    }
};

class CPeerlessBetTx : public CBettingTx
{
public:
    uint32_t nEventId;
    uint8_t nOutcome;

    // Default constructor.
    CPeerlessBetTx() {}
    CPeerlessBetTx(uint32_t eventId, uint8_t outcome) : nEventId(eventId), nOutcome(outcome) {}

    BetTxTypes GetTxType() const override { return plBetTxType; }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp (Stream& s, Operation ser_action, int nType, int nVersion) {
        READWRITE(nEventId);
        READWRITE(nOutcome);
    }
};

class CPeerlessResultTx : public CBettingTx
{
public:
    uint32_t nEventId;
    uint8_t nResultType;
    uint16_t nHomeScore;
    uint16_t nAwayScore;


    // Default Constructor.
    CPeerlessResultTx() {}

    BetTxTypes GetTxType() const override { return plResultTxType; }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp (Stream& s, Operation ser_action, int nType, int nVersion) {
        READWRITE(nEventId);
        READWRITE(nResultType);
        READWRITE(nHomeScore);
        READWRITE(nAwayScore);
    }
};

class CPeerlessUpdateOddsTx : public CBettingTx
{
public:
    uint32_t nEventId;
    uint32_t nHomeOdds;
    uint32_t nAwayOdds;
    uint32_t nDrawOdds;

    // Default Constructor.
    CPeerlessUpdateOddsTx() {}

    BetTxTypes GetTxType() const override { return plUpdateOddsTxType; }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp (Stream& s, Operation ser_action, int nType, int nVersion) {
        READWRITE(nEventId);
        READWRITE(nHomeOdds);
        READWRITE(nAwayOdds);
        READWRITE(nDrawOdds);
    }
};

class CPeerlessSpreadsEventTx : public CBettingTx
{
public:
    uint32_t nEventId;
    int16_t nPoints;
    uint32_t nHomeOdds;
    uint32_t nAwayOdds;

    // Default Constructor.
    CPeerlessSpreadsEventTx() {}

    BetTxTypes GetTxType() const override { return plSpreadsEventTxType; }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion)
    {
        READWRITE(nEventId);
        READWRITE(nPoints);
        READWRITE(nHomeOdds);
        READWRITE(nAwayOdds);
    }

};

class CPeerlessTotalsEventTx : public CBettingTx
{
public:
    uint32_t nEventId;
    uint16_t nPoints;
    uint32_t nOverOdds;
    uint32_t nUnderOdds;

    // Default Constructor.
    CPeerlessTotalsEventTx() {}

    BetTxTypes GetTxType() const override { return plTotalsEventTxType; }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion)
    {
        READWRITE(nEventId);
        READWRITE(nPoints);
        READWRITE(nOverOdds);
        READWRITE(nUnderOdds);
    }
};

class CPeerlessEventPatchTx : public CBettingTx
{
public:
    uint32_t nEventId;
    uint32_t nStartTime;

    CPeerlessEventPatchTx() {}

    BetTxTypes GetTxType() const override { return plEventPatchTxType; }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp (Stream& s, Operation ser_action, int nType, int nVersion) {
        READWRITE(nEventId);
        READWRITE(nStartTime);
    }
};

class CPeerlessParlayBetTx : public CBettingTx
{
public:
    std::vector<CPeerlessBetTx> legs;

    // Default constructor.
    CPeerlessParlayBetTx() {}

    BetTxTypes GetTxType() const override { return plParlayBetTxType; }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp (Stream& s, Operation ser_action, int nType, int nVersion) {
        READWRITE(legs);
    }
};

/*
 * Chain Games betting TX structures
 */

class CChainGamesEventTx : public CBettingTx
{
public:
    uint16_t nEventId;
    uint16_t nEntryFee;

    // Default Constructor.
    CChainGamesEventTx() {}

    BetTxTypes GetTxType() const override { return cgEventTxType; }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp (Stream& s, Operation ser_action, int nType, int nVersion) {
        READWRITE(nEventId);
        READWRITE(nEntryFee);
    }
};

class CChainGamesBetTx : public CBettingTx
{
public:
    uint16_t nEventId;

    // Default Constructor.
    CChainGamesBetTx() {}
    CChainGamesBetTx(uint16_t eventId) : nEventId(eventId) {}

    BetTxTypes GetTxType() const override { return cgBetTxType; }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion)
    {
        READWRITE(nEventId);
    }

};

class CChainGamesResultTx : public CBettingTx
{
public:
    uint16_t nEventId;

    // Default Constructor.
    CChainGamesResultTx() {}

    BetTxTypes GetTxType() const override { return cgResultTxType; }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion)
    {
        READWRITE(nEventId);
    }

};

/*
 * Quick Games betting TX structures
 */

class CQuickGamesBetTx : public CBettingTx
{
public:
    uint8_t gameType;
    std::vector<unsigned char> vBetInfo;

    // Default constructor.
    CQuickGamesBetTx() {}

    BetTxTypes GetTxType() const override { return qgBetTxType; }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp (Stream& s, Operation ser_action, int nType, int nVersion) {
        READWRITE(gameType);
        READWRITE(vBetInfo);
    }
};

class CPeerlessEventZeroingOddsTx : public CBettingTx
{
public:
    std::vector<uint32_t> vEventIds;

    // Default Constructor.
    CPeerlessEventZeroingOddsTx() {}

    BetTxTypes GetTxType() const override { return plEventZeroingOddsTxType; }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp (Stream& s, Operation ser_action, int nType, int nVersion) {
        READWRITE(vEventIds);
    }
};

std::unique_ptr<CBettingTx> ParseBettingTx(const CTxOut& txOut);

#endif