#!/usr/bin/env python3
# -*- coding: utf-8 -*-
from random import randint
import time

from test_framework.messages import msg_block
from test_framework.authproxy import JSONRPCException

from base_test import WAGERR_FakeStakeTest
from util import utxos_to_stakingPrevOuts, dir_size

class Test_03(WAGERR_FakeStakeTest):

    def run_test(self):
        self.init_test()

        FORK_DEPTH = 20  # Depth at which we are creating a fork. We are mining
        INITAL_MINED_BLOCKS = 1101
        self.NUM_BLOCKS = 15

        # 1) Starting mining blocks
        self.log.info("Mining %d blocks to get to zPOS activation...." % INITAL_MINED_BLOCKS)
        self.node.generate(INITAL_MINED_BLOCKS)
        time.sleep(2)

        # 2) Collect the possible prevouts and mint zerocoins with those
        self.log.info("Collecting all unspent coins which we generated from mining...")
        balance = self.node.getbalance()
        self.log.info("Minting zerocoins...")
        initial_mints = 0
        while balance > 5000:
            try:
                self.node.mintzerocoin(5000)
            except JSONRPCException:
                break
            time.sleep(1)
            initial_mints += 1
            self.node.generate(1)
            time.sleep(1)
            balance = self.node.getbalance()
        self.log.info("Minted %d coins in the 5000-denom..." % initial_mints)
        time.sleep(2)

        # 3) mine 10 blocks
        self.log.info("Mining 10 blocks ... and getting spendable zerocoins")
        self.node.generate(10)
        mints_list = [x["serial hash"] for x in self.node.listmintedzerocoins(True, True)]
        self.log.info("Got %d spendable (confirmed & mature) mints" % len(mints_list))

        # 4) spend mints
        self.log.info("Spending mints...")
        spends = 0
        for mint in mints_list:
            mint_arg = []
            mint_arg.append(mint)
            try:
                self.node.spendzerocoinmints(mint_arg)
                time.sleep(1)
                spends += 1
            except JSONRPCException as e:
                self.log.warning(str(e))
                continue
        time.sleep(1)
        self.log.info("Successfully spent %d mints" % spends)

        # 5) Start mining again so that spends get confirmted in a block.
        self.log.info("Mining 5 more blocks...")
        self.node.generate(5)
        self.log.info("Sleeping 2 sec. Now mining PoS blocks based on already spent transactions...")
        time.sleep(2)

        '''
        # 4) Create "Fake Stake" blocks and send them
        init_size = dir_size(self.node.datadir + "/regtest/blocks")
        self.log.info("Initial size of data dir: %s kilobytes" % str(init_size))

        for i in range(0, self.NUM_BLOCKS):
            if i != 0 and i % 5 == 0:
                self.log.info("Sent %s blocks out of %s" % (str(i), str(self.NUM_BLOCKS)))

            # Create the spam block
            block_count = self.node.getblockcount()
            randomCount = randint(block_count-FORK_DEPTH-1, block_count)
            pastBlockHash = self.node.getblockhash(randomCount)
            block = self.create_spam_block(pastBlockHash, stakingPrevOuts, randomCount+1)
            timeStamp = time.strftime('%Y-%m-%d %H:%M:%S', time.localtime(block.nTime))
            self.log.info("Created PoS block with nTime %s: %s", timeStamp, block.hash)
            msg = msg_block(block)
            self.log.info("Sending block (size: %.2f Kbytes)...", len(block.serialize())/1000)
            self.test_nodes[0].send_message(msg)


        self.log.info("Sent all %s blocks." % str(self.NUM_BLOCKS))
        self.stop_node(0)
        time.sleep(5)

        final_size = dir_size(self.node.datadir + "/regtest/blocks")
        self.log.info("Final size of data dir: %s kilobytes" % str(final_size))
        '''