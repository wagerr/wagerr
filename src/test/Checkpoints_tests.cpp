// Copyright (c) 2011-2018 The Bitcoin Core developers
// Copyright (c) 2018 The Wagerr developers
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
    // WagerrDevs - RELEASE CHANGE - if required, sanity checks
    uint256 p0 = uint256("0x000007b9191bc7a17bfb6cedf96a8dacebb5730b498361bf26d44a9f9dcc1079");     // Genesis block
    /*
    uint256 p1499 = uint256("0x96bbf9ef28aca64e940444bc54e61d69fbb22c6dfbff445f3230ee613f5204aa");  // Last block

    BOOST_CHECK(Checkpoints::CheckBlock(0, p0));
    BOOST_CHECK(Checkpoints::CheckBlock(1499, p1499));

    // Wrong hashes at checkpoints should fail:
    BOOST_CHECK(!Checkpoints::CheckBlock(0, p1499));
    BOOST_CHECK(!Checkpoints::CheckBlock(1499, p0));

    // ... but any hash not at a checkpoint should succeed:
    BOOST_CHECK(Checkpoints::CheckBlock(0+3, p1499));
    BOOST_CHECK(Checkpoints::CheckBlock(1499+3, p0));

    // remove this later
    BOOST_CHECK(Checkpoints::GetTotalBlocksEstimate() >= 1499);
    */

    BOOST_CHECK(Checkpoints::CheckBlock(0, p0));
    BOOST_CHECK(Checkpoints::GetTotalBlocksEstimate() >= 0);
}

BOOST_AUTO_TEST_SUITE_END()
