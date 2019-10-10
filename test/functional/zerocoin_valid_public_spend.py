#!/usr/bin/env python3
# Copyright (c) 2019 The WAGERR Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

'''
Tests a valid publicCoinSpend spend
'''


from time import sleep

from test_framework.authproxy import JSONRPCException

from fake_stake.base_test import WAGERR_FakeStakeTest

class zWGRValidCoinSpendTest(WAGERR_FakeStakeTest):

    def mintZerocoin(self, denom):
        self.node.mintzerocoin(denom)
        self.node.generate(5)
        sleep(1)

    def run_test(self):
        self.description = "Tests a valid publicCoinSpend spend."
        self.init_test()

        INITAL_MINED_BLOCKS = 301   # Blocks mined before minting
        MORE_MINED_BLOCKS = 26      # Blocks mined after minting (before spending)
        DENOM_TO_USE = 1         # zc denomination used for double spending attack
        V4_ACTIVATION = 450

        # 1) Start mining blocks
        self.log.info("Mining %d first blocks..." % INITAL_MINED_BLOCKS)
        self.node.generate(INITAL_MINED_BLOCKS)
        sleep(2)

        # 2) Mint zerocoins
        self.log.info("Minting %d-denom zWGRs..." % DENOM_TO_USE)
        for i in range(5):
            self.mintZerocoin(DENOM_TO_USE)
        sleep(1)

        # 3) Mine more blocks and collect the mint
        self.log.info("Mining %d more blocks..." % MORE_MINED_BLOCKS)
        self.node.generate(MORE_MINED_BLOCKS)
        sleep(2)
        list = self.node.listmintedzerocoins(True, True)
        serial_ids = [mint["serial hash"] for mint in list]
        assert(len(serial_ids) >= 3)

        # 4) Get the raw zerocoin data - save a v3 spend for later
        exported_zerocoins = self.node.exportzerocoins(False)
        zc = [x for x in exported_zerocoins if x["id"] in serial_ids]
        assert (len(zc) >= 3)
        saved_mint = zc[2]["id"]
        old_spend_v3 = self.node.createrawzerocoinpublicspend(saved_mint)

        # 5) Spend the minted coin (mine six more blocks) - spend v3
        self.log.info("Spending the minted coin with serial %s and mining six more blocks..." % zc[0]["s"])
        txid = self.node.spendzerocoinmints([zc[0]["id"]])['txid']
        self.log.info("Spent on tx %s" % txid)
        self.node.generate(6)
        sleep(2)
        rawTx = self.node.getrawtransaction(txid, 1)
        if rawTx is None:
            self.log.warning("rawTx not found for: %s" % txid)
            raise AssertionError("TEST FAILED")
        else:
            assert (rawTx["confirmations"] == 6)
        self.log.info("%s: VALID PUBLIC COIN SPEND (v3) PASSED" % self.__class__.__name__)

        # 6) Check double spends - spend v3
        self.log.info("%s: Trying to spend the serial twice now" % self.__class__.__name__)
        serial = zc[0]["s"]
        randomness = zc[0]["r"]
        privkey = zc[0]["k"]
        tx = None
        try:
            tx = self.node.spendrawzerocoin(serial, randomness, DENOM_TO_USE, privkey)
        except JSONRPCException as e:
            self.log.info("GOOD: Double-spending transaction did not verify (%s)" % str(e))
            assert("Trying to spend an already spent serial" in str(e))
        if tx is not None:
            self.log.warning("Tx is: %s" % tx)
            raise AssertionError("TEST FAILED")

        # 6) Check spend v2 disabled
        self.log.info("%s: Trying to spend using the old coin spend method.." % self.__class__.__name__)
        try:
            self.node.spendzerocoin(DENOM_TO_USE, False, False, "", False)
            raise AssertionError("TEST FAILED, old coinSpend spent")
        except JSONRPCException as e:
            self.log.info("GOOD: spendzerocoin old spend did not verify")
        self.log.info("%s: OLD COIN SPEND NON USABLE ANYMORE, TEST PASSED" % self.__class__.__name__)

        # 7) Mine more blocks
        more_blocks = V4_ACTIVATION - self.node.getblockcount() + 1
        self.log.info("Mining %d more blocks to get to V4 Spends activation..." % more_blocks)
        self.node.generate(more_blocks)
        sleep(2)

        # 8) Spend the minted coin (mine six more blocks) - spend v4
        self.log.info("Spending the minted coin with serial %s and mining six more blocks..." % zc[1]["s"])
        txid = self.node.spendzerocoinmints([zc[1]["id"]])['txid']
        self.log.info("Spent on tx %s" % txid)
        self.node.generate(6)
        sleep(2)
        rawTx = self.node.getrawtransaction(txid, 1)
        if rawTx is None:
            self.log.warning("rawTx not found for: %s" % txid)
            raise AssertionError("TEST FAILED")
        else:
            assert (rawTx["confirmations"] == 6)
        self.log.info("%s: VALID PUBLIC COIN SPEND (v4) PASSED" % self.__class__.__name__)

        # 9) Check double spends - spend v4
        self.log.info("%s: Trying to spend the serial twice now" % self.__class__.__name__)
        serial = zc[1]["s"]
        randomness = zc[1]["r"]
        privkey = zc[1]["k"]
        tx = None
        try:
            tx = self.node.spendrawzerocoin(serial, randomness, DENOM_TO_USE, privkey)
        except JSONRPCException as e:
            self.log.info("GOOD: Double-spending transaction did not verify (%s)" % str(e))
            assert ("Trying to spend an already spent serial" in str(e))
        if tx is not None:
            self.log.warning("Tx is: %s" % tx)
            raise AssertionError("TEST FAILED")

        # 10) Try to relay old v3 spend now
        self.log.info("%s: Trying to send old v3 spend now" % self.__class__.__name__)
        try:
            tx = self.node.sendrawtransaction(old_spend_v3)
            print(str(tx))
            assert(False)
        except JSONRPCException as e:
            self.log.info("GOOD: Old transaction not sent (%s)" % str(e))
            assert ("bad-txns-invalid-zwgr" in str(e))

        # 11) Try to spend the same mint with a v4 spend now
        self.log.info("Spending the minted coin with serial %s and mining six more blocks..." % zc[1]["s"])
        txid = self.node.spendzerocoinmints([saved_mint])['txid']
        self.log.info("Spent on tx %s" % txid)
        self.node.generate(6)
        sleep(2)
        rawTx = self.node.getrawtransaction(txid, 1)
        if rawTx is None:
            self.log.warning("rawTx not found for: %s" % txid)
            raise AssertionError("TEST FAILED")
        else:
            assert (rawTx["confirmations"] == 6)
        self.log.info("%s: VALID PUBLIC COIN SPEND (v4) PASSED" % self.__class__.__name__)

        # 12) Try to double spend with v4 a mint already spent with v3
        self.log.info("%s: Trying to double spend v4 against v3" % self.__class__.__name__)
        serial = zc[0]["s"]
        randomness = zc[0]["r"]
        privkey = zc[0]["k"]
        tx = None
        try:
            tx = self.node.spendrawzerocoin(serial, randomness, DENOM_TO_USE, privkey)
        except JSONRPCException as e:
            self.log.info("GOOD: Double-spending transaction did not verify (%s)" % str(e))
            assert ("Trying to spend an already spent serial" in str(e))
        if tx is not None:
            self.log.warning("Tx is: %s" % tx)
            raise AssertionError("TEST FAILED")


if __name__ == '__main__':
    zWGRValidCoinSpendTest().main()
