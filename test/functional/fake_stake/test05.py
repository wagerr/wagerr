#!/usr/bin/env python3
# -*- coding: utf-8 -*-
from random import randint
import time

from test_framework.authproxy import JSONRPCException
from test_framework.util import bytes_to_hex_str, assert_equal

from base_test import WAGERR_FakeStakeTest


'''
Covers the scenario of a valid PoS block with a valid coinstake transaction where the 
coinstake input prevout is double spent in one of the other transactions in the same block.
'''

class Test_05(WAGERR_FakeStakeTest):

    def run_test(self):
        self.init_test()
        INITAL_MINED_BLOCKS = 300
        self.NUM_BLOCKS = 3

        # 1) Starting mining blocks
        self.log.info("Mining %d blocks.." % INITAL_MINED_BLOCKS)
        self.node.generate(INITAL_MINED_BLOCKS)

        # 2) Collect the possible prevouts
        self.log.info("Collecting all unspent coins which we generated from mining...")
        utxo_list = self.node.listunspent()
        stakingPrevOuts = self.get_prevouts(utxo_list)

        # 3) Spam Blocks on the main chain
        self.log.info("-- Main chain blocks first")
        self.log_data_dir_size()
        for i in range(0, self.NUM_BLOCKS):
            if i != 0:
                self.log.info("Sent %s blocks out of %s" % (str(i), str(self.NUM_BLOCKS)))
            block_count = self.node.getblockcount()
            pastBlockHash = self.node.getblockhash(block_count)
            block = self.create_spam_block(pastBlockHash, stakingPrevOuts, block_count + 1, True)
            self.log.info("Sending block %d [%s...]", block_count + 1, block.hash[:7])
            var = self.node.submitblock(bytes_to_hex_str(block.serialize()))
            assert_equal(var, None)

            try:
                block_ret = self.node.getblock(block.hash)
                if block_ret is not None:
                    raise AssertionError("Error, block stored in main chain")
            except JSONRPCException as e:
                err_msg = str(e)
                if err_msg == "Can't read block from disk (-32603)":
                    err_msg = "Good. Block was not stored on disk."
                self.log.info(err_msg)

            # remove a random prevout from the list (to randomize block creation)
            stakingPrevOuts.popitem()

        self.log.info("Sent all %s blocks." % str(self.NUM_BLOCKS))
        self.log_data_dir_size()
        time.sleep(3)

        # 4) Spam Blocks on a forked chain
        self.log.info("-- Forked chain blocks now")
        # regenerate prevouts
        self.log.info("Mining %d more blocks as buffer..." % 31)
        self.node.generate(31)
        stakingPrevOuts = self.get_prevouts(utxo_list)
        for i in range(0, self.NUM_BLOCKS):
            if i !=0:
                self.log.info("Sent %s blocks out of %s" % (str(i), str(self.NUM_BLOCKS)))
            block_count = self.node.getblockcount()
            randomCount = randint(block_count - 30, block_count)
            pastBlockHash = self.node.getblockhash(randomCount)
            block = self.create_spam_block(pastBlockHash, stakingPrevOuts, randomCount + 1, True)
            self.log.info("Sending block %d [%s...]", randomCount + 1, block.hash[:7])
            var = self.node.submitblock(bytes_to_hex_str(block.serialize()))
            if var is not "duplicate" or not None:
                raise AssertionError("Error, submitted forked block stored on disk")

            try:
                block_ret = self.node.getblock(block.hash)
                if block_ret is not None:
                    raise AssertionError("Error, block stored in forked chain")
            except JSONRPCException as e:
                err_msg = str(e)
                if err_msg == "Can't read block from disk (-32603)":
                    err_msg = "Good. Block was not stored on disk."
                self.log.info(err_msg)

        self.log.info("Sent all %s blocks." % str(self.NUM_BLOCKS))
        self.log_data_dir_size()