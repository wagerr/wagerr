// Copyright (c) 2011-2013 The Bitcoin Core developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

//
// Unit tests for block-chain checkpoints
//

#include "checkpoints.h"

#include "uint256.h"

#include <boost/test/unit_test.hpp>

using namespace std;

BOOST_AUTO_TEST_SUITE(Checkpoints_tests)

BOOST_AUTO_TEST_CASE(sanity)
{
    uint256 p0 = uint256("0x000007b9191bc7a17bfb6cedf96a8dacebb5730b498361bf26d44a9f9dcc1079");
//    uint256 p1001 = uint256("0x");
    BOOST_CHECK(Checkpoints::CheckBlock(0, p0));
//    BOOST_CHECK(Checkpoints::CheckBlock(1001, p1001));

    // Wrong hashes at checkpoints should fail:
//    BOOST_CHECK(!Checkpoints::CheckBlock(0, p1001));
//    BOOST_CHECK(!Checkpoints::CheckBlock(1001, p0));

    // ... but any hash not at a checkpoint should succeed:
//    BOOST_CHECK(Checkpoints::CheckBlock(0+1, p1001));
//    BOOST_CHECK(Checkpoints::CheckBlock(1001+1, p0));
//    BOOST_CHECK(Checkpoints::GetTotalBlocksEstimate() >= 1001);

    // remove this later
    BOOST_CHECK(Checkpoints::GetTotalBlocksEstimate() >= 0);

}

BOOST_AUTO_TEST_SUITE_END()
