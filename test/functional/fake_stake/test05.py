#!/usr/bin/env python3
# -*- coding: utf-8 -*-
import io
from io import BytesIO
from random import randint
import time

from test_framework.messages import msg_block
from test_framework.authproxy import JSONRPCException

from test_framework.util import bytes_to_hex_str, assert_equal, hex_str_to_bytes

from base_test import WAGERR_FakeStakeTest

from functional.fake_stake.util import create_transaction
from functional.test_framework.messages import CTransaction, CTxIn, COutPoint, CTxOut, COIN
from functional.test_framework.script import CScript, OP_CHECKSIG
from util import utxos_to_stakingPrevOuts, dir_size


## Covers the scenario of a valid PoS block with a valid coinstake input that
## double spent the coinstake input in one of the transactions in the same block.

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

        utxo_list_simplified = []

        # 3) Main chain invalid double spend coinstake

        self.log.info("Starting main chain invalid double spend coin stake..")
        tx = utxo_list[0]
        conf = tx['confirmations']
        # Get oldest utxo
        for utxo_r in utxo_list:
            if utxo_r['confirmations'] > conf:
                tx = utxo_r
                conf = tx['confirmations']

        utxo_list_simplified.append(tx)
        blocktime = self.node.getrawtransaction(tx['txid'], 1)['blocktime']

        staking_prev_outs = utxos_to_stakingPrevOuts(utxo_list_simplified, blocktime)
        self.log_data_dir_size()

        # Create PoS blocks with double spent coinstake and send them
        ####### Invalid PoS block on the main chain
        block_count = self.node.getblockcount()
        staking_prev_outs = utxos_to_stakingPrevOuts(utxo_list_simplified, blocktime)
        block = self.create_new_block(block_count, staking_prev_outs, utxo_list_simplified, blocktime)
        blockHex = bytes_to_hex_str(block.serialize())
        var = self.node.submitblock(blockHex)
        assert_equal(var, None)
        self.log.info("All good on the main chain")
        self.log_data_dir_size()

        try:
            block_ret = self.node.getblock(block.hash)
            if block_ret is not None:
                raise AssertionError("Error, block stored in main chain")
        except JSONRPCException as error:
            self.log.info(error)

        ####### Now on a forked chain

        self.log.info("Starting forked chain invalid double spend coin stake..")
        block_count = self.node.getblockcount() - 20
        staking_prev_outs = utxos_to_stakingPrevOuts(utxo_list_simplified, blocktime)
        block = self.create_new_block(block_count, staking_prev_outs, utxo_list_simplified, blocktime)
        self.log.info("Sending block %d", block_count)
        self.node.submitblock(bytes_to_hex_str(block.serialize()))

        try:
            block_ret = self.node.getblock(block.hash)
            if block_ret is not None:
                raise AssertionError("Error, block stored in forked chain")
        except JSONRPCException as error:
            self.log.info(error)

        self.log_data_dir_size()


    def log_data_dir_size(self):
        init_size = dir_size(self.node.datadir + "/regtest/blocks")
        self.log.info("Size of data dir: %s kilobytes" % str(init_size))

    def create_new_block(self, block_count, stakingPrevOuts, utxo_list_simplified, blocktime):
        pastBlockHash = self.node.getblockhash(block_count)
        block = self.create_spam_block(pastBlockHash, stakingPrevOuts, block_count + 1)

        # add invalid tx
        stakingPrevOuts = utxos_to_stakingPrevOuts(utxo_list_simplified, blocktime)
        out = next(iter(stakingPrevOuts))
        value_out = int(stakingPrevOuts[out][0] - self.DEFAULT_FEE * COIN)
        tx2 = create_transaction(out, b"", value_out, block.nTime,
                                 scriptPubKey=CScript([self.block_sig_key.get_pubkey(), OP_CHECKSIG]))
        signed_tx_hex = self.node.signrawtransaction(bytes_to_hex_str(tx2.serialize()))['hex']

        signed_tx2 = CTransaction()
        signed_tx2.deserialize(BytesIO(hex_str_to_bytes(signed_tx_hex)))
        block.vtx.append(signed_tx2)

        # Re hash it
        block.hashMerkleRoot = block.calc_merkle_root()
        block.rehash()
        block.sign_block(self.block_sig_key)

        return block
