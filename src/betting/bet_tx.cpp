// Copyright (c) 2020 The Wagerr developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <betting/bet_tx.h>
#include <betting/bet_common.h>

template<typename BettingTxTypeName>
std::unique_ptr<CBettingTx> DeserializeBettingTx(CDataStream &ss)
{
    BettingTxTypeName bettingTx;
    if (ss.size() < bettingTx.GetSerializeSize(SER_NETWORK, PROTOCOL_VERSION))
        return nullptr;
    ss >> bettingTx;
    // check buffer stream is empty after deserialization
    if (!ss.empty())
        return nullptr;
    else
        return MakeUnique<BettingTxTypeName>(bettingTx);
}

std::unique_ptr<CBettingTx> ParseBettingTx(const CTxOut& txOut)
{
    CScript const & script = txOut.scriptPubKey;
    CScript::const_iterator pc = script.begin();
    std::vector<unsigned char> opcodeData;
    opcodetype opcode;

    if (!script.GetOp(pc, opcode) || opcode != OP_RETURN)
        return nullptr;

    if (!script.GetOp(pc, opcode, opcodeData) ||
            (opcode > OP_PUSHDATA1 &&
             opcode != OP_PUSHDATA2 &&
             opcode != OP_PUSHDATA4))
    {
        return nullptr;
    }

    CDataStream ss(opcodeData, SER_NETWORK, PROTOCOL_VERSION);
    // deserialize betting tx header
    CBettingTxHeader header;
    if (ss.size() < header.GetSerializeSize(SER_NETWORK, PROTOCOL_VERSION))
        return nullptr;
    ss >> header;
    if (header.prefix != BTX_PREFIX ||
            header.version != BTX_FORMAT_VERSION)
        return nullptr;

    // deserialize opcode data to tx classes
    switch ((BetTxTypes) header.txType)
    {
        case mappingTxType:
            return DeserializeBettingTx<CMappingTx>(ss);
        case plEventTxType:
            return DeserializeBettingTx<CPeerlessEventTx>(ss);
        case fEventTxType:
            return DeserializeBettingTx<CFieldEventTx>(ss);
        case fUpdateOddsTxType:
            return DeserializeBettingTx<CFieldUpdateOddsTx>(ss);
        case fUpdateMarginTxType:
            return DeserializeBettingTx<CFieldUpdateMarginTx>(ss);
        case fZeroingOddsTxType:
            return DeserializeBettingTx<CFieldZeroingOddsTx>(ss);
        case fResultTxType:
            return DeserializeBettingTx<CFieldResultTx>(ss);
        case fBetTxType:
            return DeserializeBettingTx<CFieldBetTx>(ss);
        case fParlayBetTxType:
            return DeserializeBettingTx<CFieldParlayBetTx>(ss);
        case plBetTxType:
            return DeserializeBettingTx<CPeerlessBetTx>(ss);
        case plResultTxType:
            return DeserializeBettingTx<CPeerlessResultTx>(ss);
        case plUpdateOddsTxType:
            return DeserializeBettingTx<CPeerlessUpdateOddsTx>(ss);
        case cgEventTxType:
            return DeserializeBettingTx<CChainGamesEventTx>(ss);
        case cgBetTxType:
            return DeserializeBettingTx<CChainGamesBetTx>(ss);
        case cgResultTxType:
            return DeserializeBettingTx<CChainGamesResultTx>(ss);
        case plSpreadsEventTxType:
            return DeserializeBettingTx<CPeerlessSpreadsEventTx>(ss);
        case plTotalsEventTxType:
            return DeserializeBettingTx<CPeerlessTotalsEventTx>(ss);
        case plEventPatchTxType:
            return DeserializeBettingTx<CPeerlessEventPatchTx>(ss);
        case plParlayBetTxType:
            return DeserializeBettingTx<CPeerlessParlayBetTx>(ss);
        case qgBetTxType:
            return DeserializeBettingTx<CQuickGamesBetTx>(ss);
        case plEventZeroingOddsTxType:
            return DeserializeBettingTx<CPeerlessEventZeroingOddsTx>(ss);
        default:
            return nullptr;
    }
}