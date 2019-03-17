#!/usr/bin/env python3
# -*- coding: utf-8 -*-
'''
Covers the scenario of a valid PoS block with a valid coinstake transaction where the
coinstake input prevout is double spent in one of the other transactions in the same block.
'''

from time import sleep

from base_test import WAGERR_FakeStakeTest


class Test_05(WAGERR_FakeStakeTest):

    def run_test(self):
        self.description = "Covers the scenario of a valid PoS block with a valid coinstake transaction where the coinstake input prevout is double spent in one of the other transactions in the same block."
        self.init_test()
        INITAL_MINED_BLOCKS = 300
        FORK_DEPTH = 30
        self.NUM_BLOCKS = 7

        # 1) Starting mining blocks
        self.log.info("Mining %d blocks.." % INITAL_MINED_BLOCKS)
        self.node.generate(INITAL_MINED_BLOCKS)

        # 2) Collect the possible prevouts
        self.log.info("Collecting all unspent coins which we generated from mining...")
        utxo_list = self.node.listunspent()
        stakingPrevOuts = self.get_prevouts(utxo_list)

        # 3) Spam Blocks on the main chain
        self.log.info("-- Main chain blocks first")
        self.test_spam("Main", stakingPrevOuts, fDoubleSpend=True)
        sleep(2)

        # 4) mine some block as buffer
        self.log.info("Mining %d more blocks..." % FORK_DEPTH)
        self.node.generate(FORK_DEPTH)
        sleep(2)

        # 5) Spam Blocks on a forked chain
        self.log.info("-- Forked chain blocks now")
        self.test_spam("Forked", stakingPrevOuts, fRandomHeight=True, randomRange=FORK_DEPTH, fDoubleSpend=True)