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
    uint256 p1 = uint256("0x000001364c4ed20f1b240810b5aa91fee23ae9b64b6e746b594b611cf6d8c87b");     // First premine block
    uint256 p1001 = uint256("0x0000002a314058a8f61293e18ddbef5664a2097ac0178005f593444549dd5b8c");  // Last block

    BOOST_CHECK(Checkpoints::CheckBlock(1, p1));
    BOOST_CHECK(Checkpoints::CheckBlock(1001, p1001));

    // Wrong hashes at checkpoints should fail:
    BOOST_CHECK(!Checkpoints::CheckBlock(1, p1001));
    BOOST_CHECK(!Checkpoints::CheckBlock(1001, p1));

    // ... but any hash not at a checkpoint should succeed:
    BOOST_CHECK(Checkpoints::CheckBlock(1+1, p1001));
    BOOST_CHECK(Checkpoints::CheckBlock(1001+1, p1));

    // remove this later
    BOOST_CHECK(Checkpoints::GetTotalBlocksEstimate() >= 1001);

}

BOOST_AUTO_TEST_SUITE_END()
