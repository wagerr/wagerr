#!/usr/bin/env python3
# Copyright (c) 2019 The WAGERR Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

'''
Tests a valid publicCoinSpend spend
'''


from time import sleep

from fake_stake.util import TestNode

from test_framework.authproxy import JSONRPCException
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import p2p_port


class zWGRValidCoinSpendTest(BitcoinTestFramework):

    def set_test_params(self):
        self.setup_clean_chain = True
        self.num_nodes = 1
        self.extra_args = [['-sporkkey=932HEevBSujW2ud7RfB1YF91AFygbBRQj3de3LyaCRqNzKKgWXi']]


    def setup_network(self):
        self.setup_nodes()


    def init_test(self):
        title = "*** Starting %s ***" % self.__class__.__name__
        underline = "-" * len(title)
        self.log.info("\n\n%s\n%s\n%s\n", title, underline, self.description)

        # Setup the p2p connections and start up the network thread.
        self.test_nodes = []
        for i in range(self.num_nodes):
            self.test_nodes.append(TestNode())
            self.test_nodes[i].peer_connect('127.0.0.1', p2p_port(i))


    def generateBlocks(self, n):
        nGenerated = 0
        while (nGenerated < n):
            try:
                self.nodes[0].generate(1)
                nGenerated += 1
            except JSONRPCException as e:
                if ("Couldn't create new block" in str(e)):
                    # Sleep 5 seconds and retry
                    self.log.warning(str(e))
                    self.log.info("waiting...")
                    sleep(5)
                else:
                    raise e


    def mintZerocoin(self, denom):
        self.nodes[0].mintzerocoin(denom)
        self.generateBlocks(5)
        sleep(1)


    def setV4SpendEnforcement(self, fEnable=True):
        new_val = 1563253447 if fEnable else 4070908800
        # update spork 18 and mine 1 more block
        mess = "Enabling v4" if fEnable else "Enabling v3"
        mess += " PublicSpend version with SPORK 18..."
        self.log.info(mess)
        res = self.nodes[0].spork("SPORK_18_ZEROCOIN_PUBLICSPEND_V4", new_val)
        self.log.info(res)
        assert (res == "success")
        sleep(1)


    def run_test(self):
        self.description = "Tests a valid publicCoinSpend spend."
        self.init_test()

        INITAL_MINED_BLOCKS = 301   # Blocks mined before minting
        MORE_MINED_BLOCKS = 26      # Blocks mined after minting (before spending)
        DENOM_TO_USE = 1         # zc denomination used for double spending attack

        # 1) Start mining blocks
        self.log.info("Mining/Staking %d first blocks..." % INITAL_MINED_BLOCKS)
        for i in range(6):
            self.generateBlocks(50)
            self.log.info("%d blocks generated." % int(50*(i+1)))
        sleep(2)
        self.generateBlocks(INITAL_MINED_BLOCKS-300)


        # 2) Mint zerocoins
        self.log.info("Minting %d-denom zWGRs..." % DENOM_TO_USE)
        for i in range(5):
            self.mintZerocoin(DENOM_TO_USE)
        sleep(1)


        # 3) Mine more blocks and collect the mint
        self.log.info("Mining %d more blocks..." % MORE_MINED_BLOCKS)
        self.generateBlocks(MORE_MINED_BLOCKS)
        sleep(2)
        list = self.nodes[0].listmintedzerocoins(True, True)
        serial_ids = [mint["serial hash"] for mint in list]
        assert(len(serial_ids) >= 3)


        # 4) Get the raw zerocoin data - save a v3 spend for later
        exported_zerocoins = self.nodes[0].exportzerocoins(False)
        zc = [x for x in exported_zerocoins if x["id"] in serial_ids]
        assert (len(zc) >= 3)
        saved_mint = zc[2]["id"]
        old_spend_v3 = self.nodes[0].createrawzerocoinpublicspend(saved_mint)


        # 5) Spend the minted coin (mine six more blocks) - spend v3
        serial_0 = zc[0]["s"]
        randomness_0 = zc[0]["r"]
        privkey_0 = zc[0]["k"]
        self.log.info("Spending the minted coin with serial %s and mining six more blocks..." % serial_0)
        txid = self.nodes[0].spendzerocoinmints([zc[0]["id"]])['txid']
        self.log.info("Spent on tx %s" % txid)
        self.generateBlocks(6)
        sleep(2)
        rawTx = self.nodes[0].getrawtransaction(txid, 1)
        if rawTx is None:
            self.log.warning("rawTx not found for: %s" % txid)
            raise AssertionError("TEST FAILED")
        else:
            assert (rawTx["confirmations"] == 6)
        self.log.info("%s: VALID PUBLIC COIN SPEND (v3) PASSED" % self.__class__.__name__)


        # 6) Check double spends - spend v3
        self.log.info("%s: Trying to spend the serial twice now" % self.__class__.__name__)
        tx = None
        try:
            tx = self.nodes[0].spendrawzerocoin(serial_0, randomness_0, DENOM_TO_USE, privkey_0)
        except JSONRPCException as e:
            self.log.info("GOOD: Double-spending transaction did not verify (%s)" % str(e))
            assert("Trying to spend an already spent serial" in str(e))
        if tx is not None:
            self.log.warning("Tx is: %s" % tx)
            raise AssertionError("TEST FAILED")


        # 7) Check spend v2 disabled
        self.log.info("%s: Trying to spend using the old coin spend method.." % self.__class__.__name__)
        try:
            self.nodes[0].spendzerocoin(DENOM_TO_USE, False, False, "", False)
            raise AssertionError("TEST FAILED, old coinSpend spent")
        except JSONRPCException as e:
            self.log.info("GOOD: spendzerocoin old spend did not verify")
        self.log.info("%s: OLD COIN SPEND NON USABLE ANYMORE, TEST PASSED" % self.__class__.__name__)


        # 8) Activate v4 spends with SPORK_18
        self.log.info("Activating V4 spends with SPORK_18...")
        self.setV4SpendEnforcement(True)
        self.generateBlocks(2)
        sleep(1)


        # 9) Spend the minted coin (mine six more blocks) - spend v4
        serial_1 = zc[1]["s"]
        randomness_1 = zc[1]["r"]
        privkey_1 = zc[1]["k"]
        self.log.info("Spending the minted coin with serial %s and mining six more blocks..." % serial_1)
        txid = self.nodes[0].spendzerocoinmints([zc[1]["id"]])['txid']
        self.log.info("Spent on tx %s" % txid)
        self.generateBlocks(6)
        sleep(2)
        rawTx = self.nodes[0].getrawtransaction(txid, 1)
        if rawTx is None:
            self.log.warning("rawTx not found for: %s" % txid)
            raise AssertionError("TEST FAILED")
        else:
            assert (rawTx["confirmations"] == 6)
        self.log.info("%s: VALID PUBLIC COIN SPEND (v4) PASSED" % self.__class__.__name__)


        # 10) Check double spends - spend v4
        self.log.info("%s: Trying to spend the serial twice now" % self.__class__.__name__)
        tx = None
        try:
            tx = self.nodes[0].spendrawzerocoin(serial_1, randomness_1, DENOM_TO_USE, privkey_1)
        except JSONRPCException as e:
            self.log.info("GOOD: Double-spending transaction did not verify (%s)" % str(e))
            assert ("Trying to spend an already spent serial" in str(e))
        if tx is not None:
            self.log.warning("Tx is: %s" % tx)
            raise AssertionError("TEST FAILED")


        # 11) Try to relay old v3 spend now
        self.log.info("%s: Trying to send old v3 spend now" % self.__class__.__name__)
        try:
            tx = self.nodes[0].sendrawtransaction(old_spend_v3)
            print(str(tx))
            assert(False)
        except JSONRPCException as e:
            self.log.info("GOOD: Old transaction not sent (%s)" % str(e))
            assert ("bad-txns-invalid-zwgr" in str(e))


        # 12) Try to double spend with v4 a mint already spent with v3
        self.log.info("%s: Trying to double spend v4 against v3" % self.__class__.__name__)
        tx = None
        try:
            tx = self.nodes[0].spendrawzerocoin(serial_0, randomness_0, DENOM_TO_USE, privkey_0)
        except JSONRPCException as e:
            self.log.info("GOOD: Double-spending transaction did not verify (%s)" % str(e))
            assert ("Trying to spend an already spent serial" in str(e))
        if tx is not None:
            self.log.warning("Tx is: %s" % tx)
            raise AssertionError("TEST FAILED")


        # 13) Reactivate v3 spends and try to spend the old saved one
        self.log.info("Activating V3 spends with SPORK_18...")
        self.setV4SpendEnforcement(False)
        self.generateBlocks(2)
        sleep(1)
        self.log.info("%s: Trying to send old v3 spend now" % self.__class__.__name__)
        txid = self.nodes[0].sendrawtransaction(old_spend_v3)
        self.log.info("Spent on tx %s" % txid)
        self.generateBlocks(6)
        sleep(2)
        rawTx = self.nodes[0].getrawtransaction(txid, 1)
        if rawTx is None:
            self.log.warning("rawTx not found for: %s" % txid)
            raise AssertionError("TEST FAILED")
        else:
            assert (rawTx["confirmations"] == 6)
        self.log.info("%s: VALID PUBLIC COIN SPEND (v3) PASSED" % self.__class__.__name__)




if __name__ == '__main__':
    zWGRValidCoinSpendTest().main()
