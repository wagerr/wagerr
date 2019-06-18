#!/usr/bin/env python3
# Copyright (c) 2019 The WAGERR Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

'''
Covers the scenario of a PoS block where the coinstake input prevout is already spent.
'''

from time import sleep

from fake_stake.base_test import WAGERR_FakeStakeTest

class PoSFakeStake(WAGERR_FakeStakeTest):

    def run_test(self):
        self.description = "Covers the scenario of a PoS block where the coinstake input prevout is already spent."
        self.init_test()

        INITAL_MINED_BLOCKS = 350   # First mined blocks (rewards collected to spend)
        MORE_MINED_BLOCKS = 100     # Blocks mined after spending
        STAKE_AMPL_ROUNDS = 2       # Rounds of stake amplification
        self.NUM_BLOCKS = 3         # Number of spammed blocks

        # 1) Starting mining blocks
        self.log.info("Mining %d blocks.." % INITAL_MINED_BLOCKS)
        self.node.generate(INITAL_MINED_BLOCKS)

        # 2) Collect the possible prevouts
        self.log.info("Collecting all unspent coins which we generated from mining...")

        # 3) Create 10 addresses - Do the stake amplification
        self.log.info("Performing the stake amplification (%d rounds)..." % STAKE_AMPL_ROUNDS)
        utxo_list = self.node.listunspent()
        address_list = []
        for i in range(10):
            address_list.append(self.node.getnewaddress())
        utxo_list = self.stake_amplification(utxo_list, STAKE_AMPL_ROUNDS, address_list)

        self.log.info("Done. Utxo list has %d elements." % len(utxo_list))
        sleep(2)

        # 4) Start mining again so that spent prevouts get confirmted in a block.
        self.log.info("Mining %d more blocks..." % MORE_MINED_BLOCKS)
        self.node.generate(MORE_MINED_BLOCKS)
        sleep(2)

        # 5) Create "Fake Stake" blocks and send them
        self.log.info("Creating Fake stake blocks")
        err_msgs = self.test_spam("Main", utxo_list)
        if not len(err_msgs) == 0:
            self.log.error("result: " + " | ".join(err_msgs))
            raise AssertionError("TEST FAILED")

        self.log.info("%s PASSED" % self.__class__.__name__)

if __name__ == '__main__':
    PoSFakeStake().main()
