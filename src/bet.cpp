// Copyright (c) 2018 The Wagerr developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "bet.h"

#include <cstring>

#define BTX_FORMAT_VERSION 0x01

// `ReadBTXFormatVersion` returns -1 if the `opCode` doesn't begin with a valid
// "BTX" prefix.
int ReadBTXFormatVersion(const char* opCode) {
    if (strncmp(opCode, "BTX", 3) != 0) {
        return -1;
    }
    int v = opCode[3];

    // Versions outside the range [1, 254] are not supported.
    return v < 1 || v > 254 ? -1 : v;
}

bool CPeerlessEvent::FromOpCode(const char* opCode, CPeerlessEvent &pe) {
    if (strlen(opCode) != 15) {
        return false;
    }

    if (ReadBTXFormatVersion(opCode) != BTX_FORMAT_VERSION) {
        return false;
    }

    pe.nId = opCode[4];
    // TODO Combine `opCode[5]` and `opCode[6]` to create `nStartTime`.
    pe.nSport = opCode[7];
    pe.nTournament = opCode[8];
    pe.nStage = opCode[9];
    pe.nHomeTeam = opCode[10];
    pe.nAwayTeam = opCode[11];
    pe.nHomeOdds = opCode[12];
    pe.nAwayOdds = opCode[13];
    pe.nDrawOdds = opCode[14];

    return true;
}
