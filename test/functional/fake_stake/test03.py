#!/usr/bin/env python3
# -*- coding: utf-8 -*-
'''
Covers the scenario of a zPoS block where the coinstake input is a zerocoin spend
of an already spent coin.
'''
import time

from test_framework.authproxy import JSONRPCException
from test_framework.messages import msg_block

from base_test import WAGERR_FakeStakeTest
from util import dir_size

class Test_03(WAGERR_FakeStakeTest):

    def run_test(self):
        self.description = "Covers the scenario of a zPoS block where the coinstake input is a zerocoin spend of an already spent coin."
        self.init_test()

        DENOM_TO_USE = 5000 # zc denomination
        INITAL_MINED_BLOCKS = 321
        self.NUM_BLOCKS = 10

        # 1) Starting mining blocks
        self.log.info("Mining %d blocks to get to zPOS activation...." % INITAL_MINED_BLOCKS)
        self.node.generate(INITAL_MINED_BLOCKS)
        time.sleep(2)

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
            time.sleep(1)
            initial_mints += 1
            self.node.generate(1)
            time.sleep(1)
            balance = self.node.getbalance("*", 100)
        self.log.info("Minted %d coins in the %d-denom, remaining balance %d...", initial_mints, DENOM_TO_USE, balance)
        time.sleep(2)

        # 3) mine 101 blocks
        self.log.info("Mining 101 more blocks ... and getting spendable zerocoins")
        self.node.generate(101)
        time.sleep(2)
        mints_list = [x["serial hash"] for x in self.node.listmintedzerocoins(True, True)]

        # This mints are not ready spendable, only few of them.
        self.log.info("Got %d confirmed mints" % len(mints_list))

        # 4) spend mints
        self.log.info("Spending mints...")
        spends = 0
        spent_mints = []
        for mint in mints_list:
            mint_arg = []
            mint_arg.append(mint)
            try:
                self.node.spendzerocoinmints(mint_arg)
                time.sleep(1)
                spends += 1
                spent_mints.append(mint)
            except JSONRPCException as e:
                self.log.warning(str(e))
                continue
        time.sleep(1)
        self.log.info("Successfully spent %d mints" % spends)

        # 5) Start mining again so that spends get confirmed in a block.
        self.log.info("Mining 5 more blocks...")
        self.node.generate(5)
        time.sleep(2)

        # 6) Collect some prevouts for random txes
        utxo_list = self.node.listunspent()
        self.log.info("Collecting inputs...")
        stakingPrevOuts = self.get_prevouts(utxo_list)
        time.sleep(1)

        # 7) Create "Fake Stake" blocks and send them
        init_size = dir_size(self.node.datadir + "/regtest/blocks")
        self.log.info("Initial size of data dir: %s kilobytes" % str(init_size))
        block_count = self.node.getblockcount()
        pastBlockHash = self.node.getblockhash(block_count)
        '''

        for i in range(0, self.NUM_BLOCKS):
            if i != 0 and i % 5 == 0:
                self.log.info("Sent %s blocks out of %s" % (str(i), str(self.NUM_BLOCKS)))

            # Create the spam block
            block = self.create_spam_zblock(pastBlockHash, mints_list, stakingPrevOuts, block_count+1)
            msg = msg_block(block)
            self.log.info("Sending block (size: %.2f Kbytes)...", len(block.serialize())/1000)
            self.test_nodes[0].send_message(msg)


        self.log.info("Sent all %s blocks." % str(self.NUM_BLOCKS))
        self.stop_node(0)
        time.sleep(5)

        final_size = dir_size(self.node.datadir + "/regtest/blocks")
        self.log.info("Final size of data dir: %s kilobytes" % str(final_size))
        '''