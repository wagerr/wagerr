#!/usr/bin/env python3
# -*- coding: utf-8 -*-
from io import BytesIO
import time

from test_framework.authproxy import JSONRPCException
from test_framework.messages import CTransaction, COIN
from test_framework.script import CScript, OP_CHECKSIG
from test_framework.util import bytes_to_hex_str, assert_equal, hex_str_to_bytes

from base_test import WAGERR_FakeStakeTest
from util import create_transaction, dir_size


'''
Covers the scenario of a valid PoS block with a valid coinstake transaction where the 
coinstake input prevout is double spent in one of the other transactions in the same block.
'''

class Test_05(WAGERR_FakeStakeTest):

    def run_test(self):
        self.init_test()
        INITAL_MINED_BLOCKS = 300
        self.NUM_BLOCKS = 10

        # 1) Starting mining blocks
        self.log.info("Mining %d blocks.." % INITAL_MINED_BLOCKS)
        self.node.generate(INITAL_MINED_BLOCKS)

        # 2) Collect the possible prevouts
        self.log.info("Collecting all unspent coins which we generated from mining...")
        utxo_list = self.node.listunspent()
        stakingPrevOuts = self.get_prevouts(utxo_list)

        # 3) Spam Blocks on the main chain
        self.log_data_dir_size()
        block_count = self.node.getblockcount()
        pastBlockHash = self.node.getblockhash(block_count)
        block = self.create_spam_block(pastBlockHash, stakingPrevOuts, block_count + 1, True)
        self.log.info("Sending block %d", block_count + 1)
        var = self.node.submitblock(bytes_to_hex_str(block.serialize()))
        assert_equal(var, None)
        self.log_data_dir_size()

        try:
            block_ret = self.node.getblock(block.hash)
            if block_ret is not None:
                raise AssertionError("Error, block stored in main chain")
        except JSONRPCException as error:
            self.log.info(error)

        # 4) Spam Blocks on a forked chain
        self.log.info("Starting forked chain invalid double spend coin stake..")
        block_count = self.node.getblockcount() - 20
        pastBlockHash = self.node.getblockhash(block_count)
        block = self.create_spam_block(pastBlockHash, stakingPrevOuts, block_count + 1, True)
        self.log.info("Sending block %d", block_count + 1)
        var = self.node.submitblock(bytes_to_hex_str(block.serialize()))
        assert_equal(var, None)
        self.log_data_dir_size()

        try:
            block_ret = self.node.getblock(block.hash)
            if block_ret is not None:
                raise AssertionError("Error, block stored in forked chain")
        except JSONRPCException as error:
            self.log.info(error)


    def log_data_dir_size(self):
        init_size = dir_size(self.node.datadir + "/regtest/blocks")
        self.log.info("Size of data dir: %s kilobytes" % str(init_size))
