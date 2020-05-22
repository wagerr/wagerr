// Copyright (c) 2020 The Wagerr developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef WAGERR_QUICKGAMESVIEW_H
#define WAGERR_QUICKGAMESVIEW_H

#include "uint256.h"
#include <map>

typedef enum QuickGamesType {
    qgDice = 0x00,
} QuickGamesType;

/* The quick game handler prototype, handles
 * a quick game bet with an incoming seed (pos hash)
 * and returns the odds factor which indicate win (more than odds divisor), lose (0) or refund (odds divisor).
 */
typedef uint32_t (*const BetHandler)(std::vector<unsigned char>& betInfo, uint256 seed);

/*
 * The quick game bet info parser for RPC
 * Ret value is KV map with paramName: paramValue
 */
typedef std::map<std::string, std::string> (*const BetInfoParser)(std::vector<unsigned char>& betInfo, uint256 seed);

/* The quick games framework model */
class CQuickGamesView
{
public:
    const std::string name;
    const QuickGamesType type;
    const BetHandler handler;
    const BetInfoParser betInfoParser;
    const std::string specialAddress;
    const uint32_t nFeePermille = 10; // 1%
    const uint32_t nOMNORewardPermille;
    const uint32_t nDevRewardPermille;

    explicit CQuickGamesView() = delete;
    explicit CQuickGamesView(const std::string name, const QuickGamesType type, BetHandler handler, BetInfoParser betInfoParser, const std::string specialAddress, const uint32_t nOMNORewardPermille, const uint32_t nDevRewardPermille) :
            name(name), type(type), handler(handler), betInfoParser(betInfoParser), specialAddress(specialAddress), nOMNORewardPermille(nOMNORewardPermille), nDevRewardPermille(nDevRewardPermille) { }
    // move constructor
    explicit CQuickGamesView(const CQuickGamesView&& view) :
            name(view.name), type(view.type), handler(view.handler), betInfoParser(view.betInfoParser), specialAddress(view.specialAddress), nFeePermille(view.nFeePermille), nOMNORewardPermille(view.nOMNORewardPermille),  nDevRewardPermille(view.nDevRewardPermille) { }
    // copy constructor
    explicit CQuickGamesView(const CQuickGamesView& view) = delete;
};

#endif //WAGERR_QUICKGAMESVIEW_H
