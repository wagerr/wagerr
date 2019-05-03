#!/usr/bin/env python3
# Copyright (c) 2019 The WAGERR Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

'''
Covers the scenario of a valid PoS block with a valid coinstake transaction where the
coinstake input prevout is double spent in one of the other transactions in the same block.
'''

from time import sleep

from fake_stake.base_test import WAGERR_FakeStakeTest


class PoSDoubleSpend(WAGERR_FakeStakeTest):

    def run_test(self):
        self.description = "Covers the scenario of a valid PoS block with a valid coinstake transaction where the coinstake input prevout is double spent in one of the other transactions in the same block."
        self.init_test()
        INITAL_MINED_BLOCKS = 300
        FORK_DEPTH = 30
        self.NUM_BLOCKS = 3

        # 1) Starting mining blocks
        self.log.info("Mining %d blocks.." % INITAL_MINED_BLOCKS)
        self.node.generate(INITAL_MINED_BLOCKS)

        # 2) Collect the possible prevouts
        self.log.info("Collecting all unspent coins which we generated from mining...")
        staking_utxo_list = self.node.listunspent()

        # 3) Spam Blocks on the main chain
        self.log.info("-- Main chain blocks first")
        self.test_spam("Main", staking_utxo_list, fDoubleSpend=True)
        sleep(2)

        # 4) Mine some block as buffer
        self.log.info("Mining %d more blocks..." % FORK_DEPTH)
        self.node.generate(FORK_DEPTH)
        sleep(2)

        # 5) Spam Blocks on a forked chain
        self.log.info("-- Forked chain blocks now")
        err_msgs = self.test_spam("Forked", staking_utxo_list, fRandomHeight=True, randomRange=FORK_DEPTH, fDoubleSpend=True)

        if not len(err_msgs) == 0:
            self.log.error("result: " + " | ".join(err_msgs))
            raise AssertionError("TEST FAILED")

        self.log.info("%s PASSED" % self.__class__.__name__)

if __name__ == '__main__':
    PoSDoubleSpend().main()
