// Copyright (c) 2014-2018 The Bitcoin Core developers
// Copyright (c) 2014-2015 The Dash developers
// Copyright (c) 2015-2017 The PIVX developers
// Copyright (c) 2018 The Wagerr developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "primitives/transaction.h"
#include "main.h"

#include <boost/test/unit_test.hpp>

BOOST_AUTO_TEST_SUITE(main_tests)

CAmount nMoneySupplyPoWEnd = 398360470 * COIN;

BOOST_AUTO_TEST_CASE(subsidy_limit_test)
{
    CAmount nSum = 0;
    for (int nHeight = 0; nHeight < 1; nHeight += 1) {
        /* premine in block 1 173,360,471 WGR) */
        CAmount nSubsidy = GetBlockValue(nHeight);
        BOOST_CHECK(nSubsidy <= 0 * COIN);
        nSum += nSubsidy;
    }

    for (int nHeight = 1; nHeight < 2; nHeight += 1) {
        /* PoW Phase One */
        CAmount nSubsidy = GetBlockValue(nHeight);
        BOOST_CHECK(nSubsidy <= 173360471 * COIN);
        nSum += nSubsidy;
    }

    for (int nHeight = 2; nHeight < 102; nHeight += 1) {
        /* PoW Phase Two */
        CAmount nSubsidy = GetBlockValue(nHeight);
        BOOST_CHECK(nSubsidy <= 250000 * COIN);
        nSum += nSubsidy;
    }

    for (int nHeight = 102; nHeight < 1001; nHeight += 1) {
        /* PoW Phase Two */
        CAmount nSubsidy = GetBlockValue(nHeight);
        BOOST_CHECK(nSubsidy <= 0 * COIN);
        BOOST_CHECK(MoneyRange(nSubsidy));
        nSum += nSubsidy;
        BOOST_CHECK(nSum > 0 && nSum <= nMoneySupplyPoWEnd);
    }
    BOOST_CHECK(nSum == 19626072100000000ULL);
}

BOOST_AUTO_TEST_SUITE_END()
