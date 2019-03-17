#!/usr/bin/env python3
# -*- coding: utf-8 -*-
'''
Performs the same check as in Test_02 verifying that zPoS forked blocks that stake a zerocoin which is spent on mainchain on an higher block are still accepted.
'''
from random import randint
import time
from test_framework.authproxy import JSONRPCException
from base_test import WAGERR_FakeStakeTest
from time import sleep
from util import mints_to_stakingPrevOuts

class Test_04(WAGERR_FakeStakeTest):


    def set_test_params(self):
        ''' Setup test environment
        :param:
        :return:
        '''
        self.setup_clean_chain = True
        self.num_nodes = 1
        self.extra_args = [['-staking=1', '-debug=net', '-zwgrstake']] * self.num_nodes

    def run_test(self):
        self.description = "Performs the same check as in Test_02 verifying that zPoS forked blocks that stake a zerocoin which is spent on mainchain on an higher block are still accepted."
        self.init_test()

        DENOM_TO_USE = 1000  # zc denomination
        INITAL_MINED_BLOCKS = 321
        MORE_MINED_BLOCKS = 301
        FORK_DEPTH = 75
        self.NUM_BLOCKS = 2

        # 1) Starting mining blocks
        self.log.info("Mining %d blocks to get to zPOS activation...." % INITAL_MINED_BLOCKS)
        self.node.generate(INITAL_MINED_BLOCKS)
        sleep(2)

        # 2) Collect the possible prevouts and mint zerocoins with those
        self.log.info("Collecting all unspent coins which we generated from mining...")
        balance = self.node.getbalance("*", 100)
        self.log.info("Minting zerocoins...")
        initial_mints = 0
        while balance > DENOM_TO_USE:
            try:
                self.node.mintzerocoin(DENOM_TO_USE)
            except JSONRPCException:
                break
            sleep(1)
            initial_mints += 1
            self.node.generate(1)
            sleep(1)

            if initial_mints % 5 == 0:
                self.log.info("Minted %d coins" % initial_mints)
            if initial_mints >= 20:
                break
            balance = self.node.getbalance("*", 100)
        self.log.info("Minted %d coins in the %d-denom, remaining balance %d", initial_mints, DENOM_TO_USE, balance)
        sleep(2)

        # 3) mine more blocks
        self.log.info("Mining %d more blocks ... and getting spendable zerocoins" % MORE_MINED_BLOCKS)
        self.node.generate(MORE_MINED_BLOCKS)
        sleep(2)
        mints = self.node.listmintedzerocoins(True, True)
        sleep(1)
        stakingPrevOuts = mints_to_stakingPrevOuts(mints)
        mints_hashes = [x["serial hash"] for x in mints]

        # This mints are not ready spendable, only few of them.
        self.log.info("Got %d confirmed mints" % len(mints_hashes))

        # 4) Start mining again so that spends get confirmed in a block.
        self.log.info("Mining 200 more blocks...")
        self.node.generate(200)
        sleep(2)

        # 5) spend mints
        self.log.info("Spending mints in block %d..." % self.node.getblockcount())
        spends = 0
        spent_mints = []
        for mint in mints_hashes:
            # create a single element list to pass to RPC spendzerocoinmints
            mint_arg = []
            mint_arg.append(mint)
            try:
                self.node.spendzerocoinmints(mint_arg)
                sleep(1)
                spends += 1
                spent_mints.append(mint)
            except JSONRPCException as e:
                self.log.warning(str(e))
                continue
        sleep(1)
        self.log.info("Successfully spent %d mints" % spends)

        self.log.info("Mining 6 more blocks...")
        self.node.generate(6)
        sleep(2)

        # 6) Collect some prevouts for random txes
        self.log.info("Collecting inputs for txes...")
        utxo_list = self.node.listunspent()
        spendingPrevOuts = self.get_prevouts(utxo_list)
        sleep(1)

        # 7) Create valid forked zPoS blocks and send them
        self.log.info("Creating stake zPoS blocks...")
        self.test_spam("Fork", stakingPrevOuts, spendingPrevOuts=spendingPrevOuts, fZPoS=True, fRandomHeight=True, randomRange=FORK_DEPTH, randomRange2=50, fMustPass=True)
