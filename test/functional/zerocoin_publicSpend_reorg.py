# !/usr/bin/env python3
# Copyright (c) 2019 The WAGERR Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

'''
Covers the scenario of two valid PoS blocks with same height
and same coinstake input.
'''

from copy import deepcopy
from io import BytesIO
import time
from test_framework.messages import CTransaction, CBlock
from test_framework.util import bytes_to_hex_str, hex_str_to_bytes, assert_equal
from fake_stake.base_test import WAGERR_FakeStakeTest


class ZerocoinPublicSpendReorg(WAGERR_FakeStakeTest):

    def run_test(self):
        self.description = "Covers the reorg with a zc public spend in vtx"
        self.init_test()
        DENOM_TO_USE = 10           # zc denomination
        INITAL_MINED_BLOCKS = 321   # First mined blocks (rewards collected to mint)
        MORE_MINED_BLOCKS = 105     # More blocks mined before spending zerocoins

        # 1) Starting mining blocks
        self.log.info("Mining %d blocks.." % INITAL_MINED_BLOCKS)
        self.node.generate(INITAL_MINED_BLOCKS)

        # 2) Mint 2 zerocoins
        self.node.mintzerocoin(DENOM_TO_USE)
        self.node.generate(1)
        self.node.mintzerocoin(DENOM_TO_USE)
        self.node.generate(1)

        # 3) Mine additional blocks and collect the mints
        self.log.info("Mining %d blocks.." % MORE_MINED_BLOCKS)
        self.node.generate(MORE_MINED_BLOCKS)
        self.log.info("Collecting mints...")
        mints = self.node.listmintedzerocoins(True, False)
        assert len(mints) == 2, "mints list has len %d (!= 2)" % len(mints)

        # 4) Get unspent coins and chain tip
        self.unspent = self.node.listunspent()
        block_count = self.node.getblockcount()
        pastBlockHash = self.node.getblockhash(block_count)
        self.log.info("Block count: %d - Current best: %s..." % (self.node.getblockcount(), self.node.getbestblockhash()[:5]))
        pastBlock = CBlock()
        pastBlock.deserialize(BytesIO(hex_str_to_bytes(self.node.getblock(pastBlockHash, False))))
        checkpoint = pastBlock.nAccumulatorCheckpoint

        # 5) get the raw zerocoin spend txes
        self.log.info("Getting the raw zerocoin public spends...")
        public_spend_A = self.node.createrawzerocoinpublicspend(mints[0].get("serial hash"))
        tx_A = CTransaction()
        tx_A.deserialize(BytesIO(hex_str_to_bytes(public_spend_A)))
        tx_A.rehash()
        public_spend_B = self.node.createrawzerocoinpublicspend(mints[1].get("serial hash"))
        tx_B = CTransaction()
        tx_B.deserialize(BytesIO(hex_str_to_bytes(public_spend_B)))
        tx_B.rehash()
        # Spending same coins to different recipients to get different txids
        my_addy = "yAVWM5urwaTyhiuFQHP2aP47rdZsLUG5PH"
        public_spend_A2 = self.node.createrawzerocoinpublicspend(mints[0].get("serial hash"), my_addy)
        tx_A2 = CTransaction()
        tx_A2.deserialize(BytesIO(hex_str_to_bytes(public_spend_A2)))
        tx_A2.rehash()
        public_spend_B2 = self.node.createrawzerocoinpublicspend(mints[1].get("serial hash"), my_addy)
        tx_B2 = CTransaction()
        tx_B2.deserialize(BytesIO(hex_str_to_bytes(public_spend_B2)))
        tx_B2.rehash()
        self.log.info("tx_A id: %s" % str(tx_A.hash))
        self.log.info("tx_B id: %s" % str(tx_B.hash))
        self.log.info("tx_A2 id: %s" % str(tx_A2.hash))
        self.log.info("tx_B2 id: %s" % str(tx_B2.hash))


        self.test_nodes[0].handle_connect()

        # 6) create block_A --> main chain
        self.log.info("")
        self.log.info("*** block_A ***")
        self.log.info("Creating block_A [%d] with public spend tx_A in it." % (block_count + 1))
        block_A = self.new_block(block_count, pastBlock, checkpoint, tx_A)
        self.log.info("Hash of block_A: %s..." % block_A.hash[:5])
        self.log.info("sending block_A...")
        var = self.node.submitblock(bytes_to_hex_str(block_A.serialize()))
        if var is not None:
            self.log.info("result: %s" % str(var))
            raise Exception("block_A not accepted")
        time.sleep(2)
        assert_equal(self.node.getblockcount(), block_count+1)
        assert_equal(self.node.getbestblockhash(), block_A.hash)
        self.log.info("  >>  block_A connected  <<")
        self.log.info("Current chain: ... --> block_0 [%d] --> block_A [%d]\n" % (block_count, block_count+1))

        # 7) create block_B --> forked chain
        self.log.info("*** block_B ***")
        self.log.info("Creating block_B [%d] with public spend tx_B in it." % (block_count + 1))
        block_B = self.new_block(block_count, pastBlock, checkpoint, tx_B)
        self.log.info("Hash of block_B: %s..." % block_B.hash[:5])
        self.log.info("sending block_B...")
        var = self.node.submitblock(bytes_to_hex_str(block_B.serialize()))
        self.log.info("result of block_B submission: %s" % str(var))
        time.sleep(2)
        assert_equal(self.node.getblockcount(), block_count+1)
        assert_equal(self.node.getbestblockhash(), block_A.hash)
        # block_B is not added. Chain remains the same
        self.log.info("  >>  block_B not connected  <<")
        self.log.info("Current chain: ... --> block_0 [%d] --> block_A [%d]\n" % (block_count, block_count+1))

        # 8) Create new block block_C on the forked chain (block_B)
        block_count += 1
        self.log.info("*** block_C ***")
        self.log.info("Creating block_C [%d] on top of block_B triggering the reorg" % (block_count + 1))
        block_C = self.new_block(block_count, block_B, checkpoint)
        self.log.info("Hash of block_C: %s..." % block_C.hash[:5])
        self.log.info("sending block_C...")
        var = self.node.submitblock(bytes_to_hex_str(block_C.serialize()))
        if var is not None:
            self.log.info("result: %s" % str(var))
            raise Exception("block_C not accepted")
        time.sleep(2)
        assert_equal(self.node.getblockcount(), block_count+1)
        assert_equal(self.node.getbestblockhash(), block_C.hash)
        self.log.info("  >>  block_A disconnected / block_B and block_C connected  <<")
        self.log.info("Current chain: ... --> block_0 [%d] --> block_B [%d] --> block_C [%d]\n" % (
                block_count - 1, block_count, block_count+1
        ))

        # 7) Now create block_D which tries to spend same coin of tx_B again on the (new) main chain
        # (this block will be rejected)
        block_count += 1
        self.log.info("*** block_D ***")
        self.log.info("Creating block_D [%d] trying to double spend the coin of tx_B" % (block_count + 1))
        block_D = self.new_block(block_count, block_C, checkpoint, tx_B2)
        self.log.info("Hash of block_D: %s..." % block_D.hash[:5])
        self.log.info("sending block_D...")
        var = self.node.submitblock(bytes_to_hex_str(block_D.serialize()))
        self.log.info("result of block_D submission: %s" % str(var))
        time.sleep(2)
        assert_equal(self.node.getblockcount(), block_count)
        assert_equal(self.node.getbestblockhash(), block_C.hash)
        # block_D is not added. Chain remains the same
        self.log.info("  >>  block_D rejected  <<")
        self.log.info("Current chain: ... --> block_0 [%d] --> block_B [%d] --> block_C [%d]\n" % (
                block_count - 2, block_count - 1, block_count
        ))

        # 8) Now create block_E which spends tx_A again on main chain
        # (this block will be accepted and connected since tx_A was spent on block_A now disconnected)
        self.log.info("*** block_E ***")
        self.log.info("Creating block_E [%d] trying spend tx_A on main chain" % (block_count + 1))
        block_E = self.new_block(block_count, block_C, checkpoint, tx_A)
        self.log.info("Hash of block_E: %s..." % block_E.hash[:5])
        self.log.info("sending block_E...")
        var = self.node.submitblock(bytes_to_hex_str(block_E.serialize()))
        if var is not None:
            self.log.info("result: %s" % str(var))
            raise Exception("block_E not accepted")
        time.sleep(2)
        assert_equal(self.node.getblockcount(), block_count+1)
        assert_equal(self.node.getbestblockhash(), block_E.hash)
        self.log.info("  >>  block_E connected <<")
        self.log.info("Current chain: ... --> block_0 [%d] --> block_B [%d] --> block_C [%d] --> block_E [%d]\n" % (
                block_count - 2, block_count - 1, block_count, block_count+1
        ))

        # 9) Now create block_F which tries to double spend the coin in tx_A
        # # (this block will be rejected)
        block_count += 1
        self.log.info("*** block_F ***")
        self.log.info("Creating block_F [%d] trying to double spend the coin in tx_A" % (block_count + 1))
        block_F = self.new_block(block_count, block_E, checkpoint, tx_A2)
        self.log.info("Hash of block_F: %s..." % block_F.hash[:5])
        self.log.info("sending block_F...")
        var = self.node.submitblock(bytes_to_hex_str(block_F.serialize()))
        self.log.info("result of block_F submission: %s" % str(var))
        time.sleep(2)
        assert_equal(self.node.getblockcount(), block_count)
        assert_equal(self.node.getbestblockhash(), block_E.hash)
        self.log.info("  >>  block_F rejected <<")
        self.log.info("Current chain: ... --> block_0 [%d] --> block_B [%d] --> block_C [%d] --> block_E [%d]\n" % (
                block_count - 3, block_count - 2, block_count - 1, block_count
        ))
        self.log.info("All good.")



    def new_block(self, block_count, prev_block, checkpoint, zcspend = None):
        if prev_block.hash is None:
            prev_block.rehash()
        staking_utxo_list = [self.unspent.pop()]
        pastBlockHash = prev_block.hash
        stakingPrevOuts = self.get_prevouts(staking_utxo_list, block_count)
        block = self.create_spam_block(pastBlockHash, stakingPrevOuts, block_count + 1)
        if zcspend is not None:
            block.vtx.append(zcspend)
            block.hashMerkleRoot = block.calc_merkle_root()
        block.nAccumulatorCheckpoint = checkpoint
        block.rehash()
        block.sign_block(self.block_sig_key)
        return block

if __name__ == '__main__':
    ZerocoinPublicSpendReorg().main()
