#!/usr/bin/env python3
# -*- coding: utf-8 -*-

from io import BytesIO
from struct import pack
from random import randint, choice
import time

from test_framework.authproxy import JSONRPCException
from test_framework.blocktools import create_coinbase, create_block
from test_framework.key import CECKey
from test_framework.messages import CTransaction, CTxOut, CTxIn, COIN, msg_block
from test_framework.mininode import network_thread_start
from test_framework.test_framework import BitcoinTestFramework
from test_framework.script import CScript, OP_CHECKSIG
from test_framework.util import hash256, bytes_to_hex_str, hex_str_to_bytes, connect_nodes_bi, p2p_port

from util import TestNode, create_transaction, utxo_to_stakingPrevOuts, dir_size
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
        self.log.info("\n\n*** Starting %s ***\n------------------------\n%s\n", self.__class__.__name__, self.description)
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
        self.description = ""
        self.init_test()
        return



    def create_spam_block(self, hashPrevBlock, stakingPrevOuts, height, fStakeDoubleSpent=False, fZPoS=False, spendingPrevOuts=[]):
        ''' creates a spam block filled with num_of_txes transactions
        :param   hashPrevBlock:      (hex string) hash of previous block
                 stakingPrevOuts:    ({COutPoint --> (int, int)} dictionary)
                         map outpoints to (be used as staking inputs) to amount, block_time
                 height:             (int) block height
                 fStakeDoubleSpent:  (bool) spend the coinstake input inside the block
                 fZPoS:              (bool) stake the block with zerocoin
                 spendingPrevOuts:    ({COutPoint --> (int, int)} dictionary)
                         map outpoints to (be used as tx inputs) to amount, block_time
        :return  block:              (CBlock) generated block
        '''
        self.log.info("Creating Spam Block")
        if spendingPrevOuts == []:
            spendingPrevOuts = stakingPrevOuts
        current_time = int(time.time())
        nTime = current_time & 0xfffffff0

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

        self.log.info("Stake found. Signing block...")
        signed_stake_tx = self.sign_stake_tx(block, stakingPrevOuts[block.prevoutStake][0], fZPoS)
        block.vtx.append(signed_stake_tx)

        # remove coinstake input prevout
        if not fZPoS and not fStakeDoubleSpent:
            del spendingPrevOuts[block.prevoutStake]

        # create spam for the block. random transactions
        self.log.info("Creating spam TXes...")
        for outPoint in spendingPrevOuts:
            value_out = int(spendingPrevOuts[outPoint][0] - self.DEFAULT_FEE * COIN)
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



    def sign_stake_tx(self, block, stake_in_value, fZPoS=False):
        ''' signs a coinstake transaction (non zPOS)
        :param      block:          (CBlock) block with stake to sign
                    stake_in_value: (int) staked amount
                    fZPoS:          (bool) zerocoin stake
        :return:                    (CTransaction) signed tx
        '''
        self.block_sig_key = CECKey()

        if fZPoS:
            self.log.info("Signing zPoS stake...")
            raw_stake = self.node.createrawzerocoinstake(block.prevoutStake)
            stake_tx_signed_raw_hex = raw_stake["hex"]
            stake_pkey = raw_stake["private-key"]
            self.block_sig_key.set_compressed(True)
            self.block_sig_key.set_secretbytes(bytes.fromhex(stake_pkey))

        else:
            self.block_sig_key.set_secretbytes(hash256(pack('<I', 0xffff)))
            pubkey = self.block_sig_key.get_pubkey()
            scriptPubKey = CScript([pubkey, OP_CHECKSIG])
            outNValue = int(stake_in_value + 2*COIN)

            stake_tx_unsigned = CTransaction()
            stake_tx_unsigned.nTime = block.nTime
            stake_tx_unsigned.vin.append(CTxIn(block.prevoutStake))
            stake_tx_unsigned.vin[0].nSequence = 0xffffffff
            stake_tx_unsigned.vout.append(CTxOut())
            stake_tx_unsigned.vout.append(CTxOut(outNValue, scriptPubKey))
            stake_tx_signed_raw_hex = self.node.signrawtransaction(bytes_to_hex_str(stake_tx_unsigned.serialize()))['hex']

        stake_tx_signed = CTransaction()
        stake_tx_signed.deserialize(BytesIO(hex_str_to_bytes(stake_tx_signed_raw_hex)))
        return stake_tx_signed


    def get_prevouts(self, utxo_list):
        ''' get prevouts for each utxo in a list
        :param      utxo_list:   (JSON list) returned from listunspent used as input
        :return:    stakingPrevOuts:    ({COutPoint --> (int, int)} dictionary)
                         map outpoints to (be used as staking inputs) to amount, block_time
        '''
        stakingPrevOuts = {}
        for utxo in utxo_list:
            txBlocktime = self.node.getrawtransaction(utxo['txid'], 1)['blocktime']
            utxo_to_stakingPrevOuts(utxo, stakingPrevOuts, txBlocktime)

        return stakingPrevOuts

    def log_data_dir_size(self):
        init_size = dir_size(self.node.datadir + "/regtest/blocks")
        self.log.info("Size of data dir: %s kilobytes" % str(init_size))



    def test_spam(self, name, stakingPrevOuts,
                  fRandomHeight=False, randomRange=0, randomRange2=0,
                  fDoubleSpend=False, fMustPass=False, fZPoS=False,
                  spendingPrevOuts=[]):
        ''' creates and sends spam blocks
        :param      name:            (string) chain branch (usually either "Main" or "Forked")
                    stakingPrevOuts: ({COutPoint --> (int, int)} dictionary) utxos to use for staking
                    fRandomHeight:   (bool) send blocks at random height
                    randomRange:     (int) if fRandomHeight=True, height is >= current-randomRange
                    randomRange2:    (int) if fRandomHeight=True, height is < current-randomRange2
                    fDoubleSpend:    (bool) if true, stake input is double spent in block.vtx
                    fMustPass:       (bool) if true, the blocks must be stored on disk
                    fZPoS:           (bool) stake the block with zerocoin
                    spendingPrevOuts:({COutPoint --> (int, int)} dictionary) utxos to use for spending
        :return:
        '''
        self.log_data_dir_size()
        block_count = self.node.getblockcount()
        pastBlockHash = self.node.getblockhash(block_count)
        randomCount = block_count
        self.log.info("Current height: %d" % block_count)
        for i in range(0, self.NUM_BLOCKS):
            if i !=0:
                self.log.info("Sent %d blocks out of %d" % (i, self.NUM_BLOCKS))

            if fRandomHeight:
                randomCount = randint(block_count - randomRange, block_count - randomRange2)
                pastBlockHash = self.node.getblockhash(randomCount)

            block = self.create_spam_block(pastBlockHash, stakingPrevOuts, randomCount + 1,
                                           fStakeDoubleSpent=fDoubleSpend, fZPoS=fZPoS, spendingPrevOuts=spendingPrevOuts)
            block_time = time.strftime('%Y-%m-%d %H:%M:%S', time.localtime(block.nTime))
            block_size = len(block.serialize())/1000
            self.log.info("Sending block %d [%s...] - nTime: %s - Size (kb): %.2f",
                          randomCount + 1, block.hash[:7], block_time, block_size)

            var = self.node.submitblock(bytes_to_hex_str(block.serialize()))
            if (not fMustPass and var not in [None, "bad-txns-invalid-zwgr"]) or (fMustPass and var != "inconclusive"):
                raise AssertionError("Error, submitblock [fMustPass=%s] result: %s" % (str(fMustPass), str(var)))

            msg = msg_block(block)
            self.test_nodes[0].send_message(msg)

            try:
                block_ret = self.node.getblock(block.hash)
                if not fMustPass and block_ret is not None:
                    raise AssertionError("Error, block stored in %s chain: %s" % (name, str(block_ret)))
                if fMustPass:
                    if block_ret is None:
                        raise AssertionError("Error, block NOT stored in %s chain" % name)
                    else:
                        self.log.info("Good. Block IS stored on disk.")

            except JSONRPCException as e:
                err_msg = str(e)
                if err_msg == "Can't read block from disk (-32603)":
                    if fMustPass:
                        self.log.warning("Bad! Block was NOT stored to disk.")
                    else:
                        self.log.info("Good. Block was not stored on disk.")
                else:
                    self.log.warning(err_msg)

            # remove a random prevout from the list
            # (to randomize block creation if the same height is picked two times)
            del stakingPrevOuts[choice(list(stakingPrevOuts))]

        self.log.info("Sent all %s blocks." % str(self.NUM_BLOCKS))
        self.log_data_dir_size()