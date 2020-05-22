// Copyright (c) 2020 The Wagerr developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.


#ifndef WAGERR_DICE_H
#define WAGERR_DICE_H

#include "serialize.h"
#include "uint256.h"

namespace quickgames {

typedef enum QuickGamesDiceBetType {
    qgDiceEqual = 0x00,
    qgDiceNotEqual = 0x01,
    qgDiceTotalOver = 0x02,
    qgDiceTotalUnder = 0x03,
    qgDiceEven = 0x04,
    qgDiceOdd = 0x05,
    qgDiceUndefined = 0xff,
} QuickGamesDiceBetType;

struct DiceBetInfo {
    QuickGamesDiceBetType betType;
    uint32_t betNumber;

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp (Stream& s, Operation ser_action, int nType, int nVersion) {
        uint8_t type;
        if (ser_action.ForRead()) {
            READWRITE(type);
            betType = (QuickGamesDiceBetType) type;
        }
        else {
            type = (uint8_t) betType;
            READWRITE(type);
        }
        if (type != qgDiceEven && type != qgDiceOdd) {
            READWRITE(betNumber);
        }
    }
};

std::map<std::string, std::string> DiceBetInfoParser(std::vector<unsigned char>& betInfo, uint256 seed);

uint32_t DiceHandler(std::vector<unsigned char>& betInfo, uint256 seed);

std::string DiceGameTypeToStr(QuickGamesDiceBetType type);
QuickGamesDiceBetType StrToDiceGameType(std::string strType);

} // namespace quickgames


#endif //WAGERR_DICE_H
