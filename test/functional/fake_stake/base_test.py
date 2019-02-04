#!/usr/bin/env python3
# -*- coding: utf-8 -*-

from io import BytesIO
from struct import pack
import time

from test_framework.authproxy import JSONRPCException
from test_framework.blocktools import create_coinbase, create_block
from test_framework.key import CECKey
from test_framework.messages import CTransaction, CTxOut, CTxIn, COIN
from test_framework.mininode import network_thread_start
from test_framework.test_framework import BitcoinTestFramework
from test_framework.script import CScript, OP_CHECKSIG
from test_framework.util import hash256, bytes_to_hex_str, hex_str_to_bytes, connect_nodes_bi, p2p_port

from util import TestNode, create_transaction
''' -------------------------------------------------------------------------
WAGERR_FakeStakeTest CLASS ----------------------------------------------------

General Test Class to be extended by individual tests for each attack test
'''
class WAGERR_FakeStakeTest(BitcoinTestFramework):

    def set_test_params(self):
        ''' Setup test environment
        :param:
        :return:
        '''
        self.setup_clean_chain = True
        self.num_nodes = 1
        self.extra_args = [['-staking=1', '-debug=net']]*self.num_nodes


    def setup_network(self):
        ''' Can't rely on syncing all the nodes when staking=1
        :param:
        :return:
        '''
        self.setup_nodes()
        for i in range(self.num_nodes - 1):
            for j in range(i+1, self.num_nodes):
                connect_nodes_bi(self.nodes, i, j)

    def init_test(self):
        ''' Initializes Attack parameters
        :param:
        :return:
        '''
        self.log.info("\n***Starting %s test ***", self.__class__.__name__)
        # Global Test parameters (override in run_test)
        self.DEFAULT_FEE = 0.1
        # Spam blocks to send in current test
        self.NUM_BLOCKS = 30

        # Setup the p2p connections and start up the network thread.
        self.test_nodes = []
        for i in range(self.num_nodes):
            self.test_nodes.append(TestNode())
            self.test_nodes[i].peer_connect('127.0.0.1', p2p_port(i))

        network_thread_start()  # Start up network handling in another thread
        self.node = self.nodes[0]

        # Let the test nodes get in sync
        for i in range(self.num_nodes):
            self.test_nodes[i].wait_for_verack()


    def run_test(self):
        ''' Performs the attack of this test - run init_test first.
        :param:
        :return:
        '''
        self.init_test()
        return


    def create_spam_block(self, hashPrevBlock, stakingPrevOuts, height):
        ''' creates a spam block filled with num_of_txes transactions
        :param   hashPrevBlock:    (hex string) hash of previous block
                 stakingPrevOuts:  ({COutPoint --> (int, int)} dictionary)
                         map outpoints to (be used as staking inputs) to amount, block_time
                 num_of_txes:      (int) number of transactions to include in the block
        :return  block:            (CBlock) generated block
        '''
        current_time = int(time.time())
        nTime = current_time & 0xfffffff0

        # PoS blocks have empty coinbase so we don't need to specify block number
        coinbase = create_coinbase(height)
        coinbase.vout[0].nValue = 0
        coinbase.vout[0].scriptPubKey = b""
        coinbase.nTime = nTime
        coinbase.rehash()

        block = create_block(int(hashPrevBlock, 16), coinbase, nTime)

        # create a new private key used for block signing.
        # solve for the block here
        parent_block_stake_modifier = int(self.node.getblock(hashPrevBlock)['modifier'], 16)
        if not block.solve_stake(parent_block_stake_modifier, stakingPrevOuts):
            raise Exception("Not able to solve for any prev_outpoint")

        signed_stake_tx = self.sign_stake_tx(block)
        block.vtx.append(signed_stake_tx)
        del stakingPrevOuts[block.prevoutStake]

        # create spam for the block. random transactions
        for outPoint in stakingPrevOuts:
            value_out = int(stakingPrevOuts[outPoint][0] - self.DEFAULT_FEE * COIN)
            tx = create_transaction(outPoint, b"", value_out, nTime, scriptPubKey=CScript([self.block_sig_key.get_pubkey(), OP_CHECKSIG]))
            # sign txes
            signed_tx_hex = self.node.signrawtransaction(bytes_to_hex_str(tx.serialize()))['hex']
            signed_tx = CTransaction()
            signed_tx.deserialize(BytesIO(hex_str_to_bytes(signed_tx_hex)))
            block.vtx.append(signed_tx)

        block.hashMerkleRoot = block.calc_merkle_root()

        block.rehash()
        block.sign_block(self.block_sig_key)
        return block


    def spend_utxo(self, utxo, address_list):
        ''' spend amount from previously unspent output to a provided address
        :param      utxo:           (JSON) returned from listunspent used as input
                    addresslist:    (string) destination address
        :return:                    (string) tx hash if successful, empty string otherwise
        '''
        try:
            inputs = [{"txid":utxo["txid"], "vout":utxo["vout"]}]
            out_amount = (float(utxo["amount"]) - self.DEFAULT_FEE)/len(address_list)
            outputs = {}
            for address in address_list:
                outputs[address] = out_amount
            spendingTx = self.node.createrawtransaction(inputs, outputs)
            spendingTx_signed = self.node.signrawtransaction(spendingTx)
            if spendingTx_signed["complete"]:
                txhash = self.node.sendrawtransaction(spendingTx_signed["hex"])
                return txhash
            else:
                self.log.warning("Error: %s" % str(spendingTx_signed["errors"]))
                return ""
        except JSONRPCException as e:
            self.log.error("JSONRPCException: %s" % str(e))
            return ""


    def spend_utxos(self, utxo_list, address_list = []):
        ''' spend utxo to random addresses
        :param      utxo_list:  (JSON list) returned from listunspent used as input
                    address_list:  (JSON list) [optional] recipients. if not set,
                        10 new addresses will be generated from the wallet for each tx.
        :return:                (string list) tx hashes
        '''
        txHashes = []
        if address_list == []:
            # get 10 new addresses
            for i in range(10):
                address_list.append(self.node.getnewaddress())

        for utxo in utxo_list:
            try:
                # spend current utxo to provided addresses
                txHash = self.spend_utxo(utxo, address_list)
                if txHash != "":
                    txHashes.append(txHash)
            except JSONRPCException as e:
                self.log.error("JSONRPCException: %s" % str(e))
                continue
        return txHashes


    def stake_amplification_step(self, utxo_list, address_list = []):
        ''' spends a list of utxos providing the list of new outputs
        :param      utxo_list:  (JSON list) returned from listunspent used as input
        :return:                (JSON list) list of new (valid) inputs after the spends
        '''
        self.log.info("--> Stake Amplification step started with %d UTXOs", len(utxo_list))
        txHashes = self.spend_utxos(utxo_list, address_list)
        num_of_txes = len(txHashes)
        new_utxos = []
        if num_of_txes> 0:
            self.log.info("Created %d transactions...Mining 2 blocks to include them..." % num_of_txes)
            self.node.generate(2)
            time.sleep(2)
            new_utxos = self.node.listunspent()

        self.log.info("Amplification step produced %d new \"Fake Stake\" inputs:" % len(new_utxos))
        return new_utxos


    def stake_amplification(self, utxo_list, iterations, address_list = []):
        ''' performs the "stake amplification" which gives higher chances at finding fake stakes
        :param      utxo_list:  (JSON list) returned from listunspent used as input
        :return:                (JSON list) list of new (valid) inputs after the spends
        '''
        self.log.info("** Stake Amplification started with %d UTXOs", len(utxo_list))
        valid_inputs = utxo_list
        all_inputs = []
        for i in range(iterations):
            all_inputs = all_inputs + valid_inputs
            old_inputs = valid_inputs
            valid_inputs = self.stake_amplification_step(old_inputs, address_list)
        self.log.info("** Stake Amplification ended with %d \"fake\" UTXOs", len(all_inputs))
        return all_inputs





    def sign_stake_tx(self, block):
        ''' signs a coinstake transaction (non zPOS)
        :param      block:  (CBlock) block with stake to sign
        :return:            (CTransaction) signed tx
        '''
        self.block_sig_key = CECKey()
        self.block_sig_key.set_secretbytes(hash256(pack('<I', 0xffff)))
        pubkey = self.block_sig_key.get_pubkey()
        scriptPubKey = CScript([pubkey, OP_CHECKSIG])
        outNValue = 2

        stake_tx_unsigned = CTransaction()
        stake_tx_unsigned.nTime = block.nTime
        stake_tx_unsigned.vin.append(CTxIn(block.prevoutStake))
        stake_tx_unsigned.vin[0].nSequence = 0xffffffff
        stake_tx_unsigned.vout.append(CTxOut())
        stake_tx_unsigned.vout.append(CTxOut(int(outNValue*COIN), scriptPubKey))
        stake_tx_signed_raw_hex = self.node.signrawtransaction(bytes_to_hex_str(stake_tx_unsigned.serialize()))['hex']
        stake_tx_signed = CTransaction()
        stake_tx_signed.deserialize(BytesIO(hex_str_to_bytes(stake_tx_signed_raw_hex)))
        return stake_tx_signed

