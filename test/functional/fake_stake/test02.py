#!/usr/bin/env python3
# -*- coding: utf-8 -*-
from random import randint
import time

from test_framework.messages import msg_block

from base_test import WAGERR_FakeStakeTest
from util import utxos_to_stakingPrevOuts, dir_size

class Test_02(WAGERR_FakeStakeTest):

    def run_test(self):
        self.init_test()
        INITAL_MINED_BLOCKS = 150
        MORE_MINED_BLOCKS = 50
        self.NUM_BLOCKS = 3

        # 1) Starting mining blocks
        self.log.info("Mining %d blocks.." % INITAL_MINED_BLOCKS)
        self.node.generate(INITAL_MINED_BLOCKS)

        # 2) Collect the possible prevouts
        self.log.info("Collecting all unspent coins which we generated from mining...")
        utxo_list = self.node.listunspent()
        time.sleep(2)

        # 3) Mine more blocks
        self.log.info("Mining %d more blocks.." % MORE_MINED_BLOCKS)
        self.node.generate(MORE_MINED_BLOCKS)
        time.sleep(2)

        # 4) Spend the coins collected in 2 (mined in the first 100 blocks)
        self.log.info("Spending the coins mined in the first %d blocks..." % INITAL_MINED_BLOCKS)
        tx_block_time = int(time.time())
        stakingPrevOuts = utxos_to_stakingPrevOuts(utxo_list, tx_block_time)
        tx_hashes = self.spend_utxos(utxo_list)
        self.log.info("Spent %d transactions" % len(tx_hashes))
        time.sleep(2)

        # 5) Mine 10 more blocks
        self.log.info("Mining 10 more blocks to include the TXs in chain...")
        self.node.generate(10)
        time.sleep(2)

        # 6) Create PoS blocks and send them
        init_size = dir_size(self.node.datadir + "/regtest/blocks")
        self.log.info("Initial size of data dir: %s kilobytes" % str(init_size))

        for i in range(0, self.NUM_BLOCKS):
            if i != 0 and i % 5 == 0:
                self.log.info("Sent %s blocks out of %s" % (str(i), str(self.NUM_BLOCKS)))

            # Create the spam block
            block_count = self.node.getblockcount()
            randomCount = randint(block_count-MORE_MINED_BLOCKS-1, block_count-12)
            pastBlockHash = self.node.getblockhash(randomCount)
            block = self.create_spam_block(pastBlockHash, stakingPrevOuts)
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
        self.log.info("Total size increase: %s kilobytes" % str(final_size-init_size))

