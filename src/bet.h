// Copyright (c) 2018 The Wagerr developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef WAGERR_BET_H
#define WAGERR_BET_H

#include <cstdint>
#include <string>

typedef enum OutcomeType {
    OutcomeTypeWin   = 0x01,
    OutcomeTypeLose  = 0x02,
    OutcomeTypeDraw  = 0x03
} OutcomeType;

class CPeerlessEvent {
public:
    uint32_t nId;
    uint64_t nStartTime;
    uint32_t nSport;
    uint32_t nTournament;
    uint32_t nStage;
    uint32_t nHomeTeam;
    uint32_t nAwayTeam;
    uint32_t nHomeOdds;
    uint32_t nAwayOdds;
    uint32_t nDrawOdds;

    CPeerlessEvent();

    static bool FromOpCode(const char* opCode, CPeerlessEvent &pe);
};

class CPeerlessBet {
public:
    uint32_t nEventId;
    OutcomeType outcome;

    CPeerlessBet();
    CPeerlessBet(int eventId, int outcome);

    std::string ToOpCode();
};

#endif // WAGERR_BET_H
