#!/usr/bin/env python3
# -*- coding: utf-8 -*-
'''
Covers the scenario of a PoS block where the coinstake input prevout is already spent.
'''
from random import randint
import time

from test_framework.messages import msg_block

from base_test import WAGERR_FakeStakeTest
from util import dir_size

class Test_01(WAGERR_FakeStakeTest):

    def run_test(self):
        self.description = "Covers the scenario of a PoS block where the coinstake input prevout is already spent."
        self.init_test()

        FORK_DEPTH = 20  # Depth at which we are creating a fork. We are mining
        INITAL_MINED_BLOCKS = 150
        self.NUM_BLOCKS = 7

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
        time.sleep(2)

        # 4) collect the prevouts
        self.log.info("Collecting inputs...")
        stakingPrevOuts = self.get_prevouts(utxo_list)
        time.sleep(1)

        # 5) Start mining again so that spent prevouts get confirmted in a block.
        self.log.info("Mining 5 more blocks...")
        self.node.generate(5)
        self.log.info("Sleeping 2 sec. Now mining PoS blocks based on already spent transactions...")
        time.sleep(2)

        # 6) Create "Fake Stake" blocks and send them
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
            #assert_not_equal(self.node.submitblock(bytes_to_hex_str(block.serialize())), None)


        self.log.info("Sent all %s blocks." % str(self.NUM_BLOCKS))
        self.stop_node(0)
        time.sleep(5)

        final_size = dir_size(self.node.datadir + "/regtest/blocks")
        self.log.info("Final size of data dir: %s kilobytes" % str(final_size))
        self.log.info("Total size increase: %s kilobytes" % str(final_size-init_size))