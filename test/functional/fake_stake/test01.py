#!/usr/bin/env python3
# -*- coding: utf-8 -*-
'''
Covers the scenario of a PoS block where the coinstake input prevout is already spent.
'''

from time import sleep

from base_test import WAGERR_FakeStakeTest

class Test_01(WAGERR_FakeStakeTest):

    def run_test(self):
        self.description = "Covers the scenario of a PoS block where the coinstake input prevout is already spent."
        self.init_test()

        FORK_DEPTH = 10  # Depth at which we are creating a fork. We are mining
        INITAL_MINED_BLOCKS = 150
        self.NUM_BLOCKS = 3

        # 1) Starting mining blocks
        self.log.info("Mining %d blocks.." % INITAL_MINED_BLOCKS)
        self.node.generate(INITAL_MINED_BLOCKS)

        # 2) Collect the possible prevouts
        self.log.info("Collecting all unspent coins which we generated from mining...")

        # 3) Create 10 addresses - Do the stake amplification
        self.log.info("Performing the stake amplification (3 rounds)...")
        utxo_list = self.node.listunspent()
        address_list = []
        for i in range(10):
            address_list.append(self.node.getnewaddress())
        utxo_list = self.stake_amplification(utxo_list, 3, address_list)

        self.log.info("Done. Utxo list has %d elements." % len(utxo_list))
        sleep(2)

        # 4) collect the prevouts
        self.log.info("Collecting inputs...")
        stakingPrevOuts = self.get_prevouts(utxo_list)
        sleep(1)

        # 5) Start mining again so that spent prevouts get confirmted in a block.
        self.log.info("Mining %d more blocks..." % FORK_DEPTH)
        self.node.generate(FORK_DEPTH)
        sleep(2)

        # 6) Create "Fake Stake" blocks and send them
        self.log.info("Creating Fake stake blocks")
        self.test_spam("Main", stakingPrevOuts)