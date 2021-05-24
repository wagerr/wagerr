// Copyright (c) 2021 The Wagerr Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "util.h"

#include <boost/test/unit_test.hpp>

#include <betting/bet_db.h>

BOOST_AUTO_TEST_SUITE(field_events_tests)

BOOST_AUTO_TEST_CASE(FieldEventOddsCalc)
{
    CFieldEventDB fEvent;
    fEvent.nMarginPercent = 11100; // 111.00 %
    std::map<uint32_t, ContenderInfo> mContenders {
        {1, ContenderInfo{650000, 0, 0, 0, 0}}, // 6.5
        {2, ContenderInfo{450000, 0, 0, 0, 0}}, // 4.5
        {3, ContenderInfo{275000, 0, 0, 0, 0}}, // 2.75
        {4, ContenderInfo{2900000, 0, 0, 0, 0}}, // 29
        {5, ContenderInfo{1700000, 0, 0, 0, 0}}, // 17
        {6, ContenderInfo{1700000, 0, 0, 0, 0}}, // 17
        {7, ContenderInfo{2900000, 0, 0, 0, 0}}, // 29
        {8, ContenderInfo{1100000, 0, 0, 0, 0}}, // 11
        {9, ContenderInfo{650000, 0, 0, 0, 0}} // 6.5
    };
    fEvent.contenders = mContenders;
}

BOOST_AUTO_TEST_SUITE_END()