#include "betting/quickgames/dice.h"
#include "chainparams.h"
#include "clientversion.h"
#include "streams.h"
#include "betting/bet.h"

#define BET_ODDSDIVISOR 10000   // Odds divisor, Facilitates calculations with floating integers.

uint64_t Dice_NumberOfWinCases(uint64_t sum) {              //    sum: 2, 3, 4, 5, 6, 7, 8, 9, 10  11, 12
    return (sum <= 7) * (sum - 1) + (sum > 7) * (13 - sum); // result: 1, 2, 3, 4, 5, 6, 5, 4,  3,  2,  1
}

namespace quickgames {

std::string DiceGameTypeToStr(QuickGamesDiceBetType type)
{
    switch (type)
    {
    case qgDiceEqual: return std::string("equal");
    case qgDiceNotEqual: return std::string("not equal");
    case qgDiceTotalOver: return std::string("total over");
    case qgDiceTotalUnder: return std::string("total under");
    case qgDiceEven: return std::string("even");
    case qgDiceOdd: return std::string("odd");
    default:
        return std::string("undefined game type");
    }
}

QuickGamesDiceBetType StrToDiceGameType(std::string strType)
{
    if (strType == "equal")
        return qgDiceEqual;
    else if (strType == "not equal")
        return qgDiceNotEqual;
    else if (strType == "total over")
        return qgDiceTotalOver;
    else if (strType == "total under")
        return qgDiceTotalUnder;
    else if (strType == "even")
        return qgDiceEven;
    else if (strType == "odd")
        return qgDiceOdd;

    return qgDiceUndefined;
}

std::map<std::string, std::string> DiceBetInfoParser(std::vector<unsigned char>& betInfo, uint256 seed)
{
    CDataStream ss{betInfo, SER_NETWORK, CLIENT_VERSION};
    DiceBetInfo info;
    ss >> info;

    uint64_t firstDice = seed.Get64(0) % 6 + 1;
    uint64_t secondDice = seed.Get64(1) % 6 + 1;
    uint64_t sum = firstDice + secondDice;

    std::string strBetNumber = std::to_string(info.betNumber);
    if (info.betType == qgDiceTotalOver || info.betType == qgDiceTotalUnder)
        strBetNumber.append(".5");
    return {std::make_pair("diceGameType", DiceGameTypeToStr(info.betType)),
        std::make_pair("betNumber", (info.betType != qgDiceEven && info.betType != qgDiceOdd) ? strBetNumber : std::string("")),
        std::make_pair("firstDice", std::to_string(firstDice)),
        std::make_pair("secondDice", std::to_string(secondDice)),
        std::make_pair("sum", std::to_string(sum)),
        std::make_pair("odds", std::to_string(DiceHandler(betInfo, seed)))};
}

uint32_t DiceHandler(std::vector<unsigned char>& betInfo, uint256 seed)
{
    static const uint32_t NUMBER_OF_OUTCOMES = 36;
    CDataStream ss{betInfo, SER_NETWORK, CLIENT_VERSION};
    DiceBetInfo info;
    ss >> info;

    uint64_t firstDice = seed.Get64(0) % 6 + 1;
    uint64_t secondDice = seed.Get64(1) % 6 + 1;
    uint64_t sum = firstDice + secondDice;

    if (info.betType == qgDiceOdd && sum % 2 == 1) {
        return BET_ODDSDIVISOR * 2;
    }
    else if (info.betType == qgDiceEven && sum % 2 == 0) {
        return BET_ODDSDIVISOR * 2;
    }
    if (info.betType == qgDiceEqual && sum == info.betNumber) {
        return BET_ODDSDIVISOR * NUMBER_OF_OUTCOMES / Dice_NumberOfWinCases(info.betNumber);
    }
    else if (info.betType == qgDiceNotEqual && sum != info.betNumber) {
        return BET_ODDSDIVISOR * NUMBER_OF_OUTCOMES / (NUMBER_OF_OUTCOMES - Dice_NumberOfWinCases(info.betNumber));
    }
    else if (info.betType == qgDiceTotalUnder && sum <= info.betNumber ) {
        uint64_t numberOfWinCases = 0;
        for (uint32_t i = 2; i <= info.betNumber; ++i)
            numberOfWinCases += Dice_NumberOfWinCases(i);
        return BET_ODDSDIVISOR * NUMBER_OF_OUTCOMES / numberOfWinCases;
    }
    else if (info.betType == qgDiceTotalOver && sum > info.betNumber) {
        uint64_t numberOfWinCases = 0;
        for (uint32_t i = 2; i <= info.betNumber; ++i)
            numberOfWinCases += Dice_NumberOfWinCases(i);
        return BET_ODDSDIVISOR * NUMBER_OF_OUTCOMES / (NUMBER_OF_OUTCOMES - numberOfWinCases);
    }

    return 0;
}

} // namespace quickgames
