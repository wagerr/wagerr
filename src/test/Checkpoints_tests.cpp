// Copyright (c) 2011-2013 The Bitcoin Core developers
// Copyright (c) 2017 The PIVX developers
// Copyright (c) 2018 The Wagerr developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

//
// Unit tests for block-chain checkpoints
//

#include "checkpoints.h"

#include "uint256.h"
#include "test_wagerr.h"

#include <boost/test/unit_test.hpp>


BOOST_FIXTURE_TEST_SUITE(Checkpoints_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(sanity)
{
    uint256 p290000 = uint256("0x5a70e614a2e6035be0fa1dd1a67bd6caa0a78e396e889aac42bbbc08e11cdabd");
    uint256 p320000 = uint256("0x9060f8d44058c539653f37eaac4c53de7397e457dda264c5ee1be94293e9f6bb");
    BOOST_CHECK(Checkpoints::CheckBlock(290000, p290000));
    BOOST_CHECK(Checkpoints::CheckBlock(320000, p320000));


    // Wrong hashes at checkpoints should fail:
    BOOST_CHECK(!Checkpoints::CheckBlock(290000, p320000));
    BOOST_CHECK(!Checkpoints::CheckBlock(320000, p290000));

    // ... but any hash not at a checkpoint should succeed:
    BOOST_CHECK(Checkpoints::CheckBlock(290000+1, p290000));
    BOOST_CHECK(Checkpoints::CheckBlock(320000+1, p320000));

    BOOST_CHECK(Checkpoints::GetTotalBlocksEstimate() >= 320000);
}

BOOST_AUTO_TEST_SUITE_END()
