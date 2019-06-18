#!/usr/bin/env python3
# Copyright (c) 2019 The WAGERR Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

'''
Covers the 'Wrapped Serials Attack' scenario
'''

import random
from time import sleep

from test_framework.authproxy import JSONRPCException
from test_framework.util import assert_equal, assert_greater_than

from fake_stake.base_test import WAGERR_FakeStakeTest

class zWGRValidCoinSpendTest(WAGERR_FakeStakeTest):

    def run_test(self):
        self.description = "Covers the 'valid publicCoinSpend spend' scenario."
        self.init_test()

        INITAL_MINED_BLOCKS = 301   # Blocks mined before minting
        MORE_MINED_BLOCKS = 52      # Blocks mined after minting (before spending)
        DENOM_TO_USE = 1         # zc denomination used for double spending attack

        # 1) Start mining blocks
        self.log.info("Mining %d first blocks..." % INITAL_MINED_BLOCKS)
        self.node.generate(INITAL_MINED_BLOCKS)
        sleep(2)

        # 2) Mint zerocoins
        self.log.info("Minting %d-denom zWGRs..." % DENOM_TO_USE)
        self.node.mintzerocoin(DENOM_TO_USE)
        self.node.generate(1)
        sleep(2)
        self.node.mintzerocoin(DENOM_TO_USE)
        sleep(2)

        # 3) Mine more blocks and collect the mint
        self.log.info("Mining %d more blocks..." % MORE_MINED_BLOCKS)
        self.node.generate(MORE_MINED_BLOCKS)
        sleep(2)
        list = self.node.listmintedzerocoins(True, True)
        mint = list[0]

        # 4) Get the raw zerocoin data
        exported_zerocoins = self.node.exportzerocoins(False)
        zc = [x for x in exported_zerocoins if mint["serial hash"] == x["id"]]
        if len(zc) == 0:
            raise AssertionError("mint not found")

        # 5) Spend the minted coin (mine six more blocks)
        self.log.info("Spending the minted coin with serial %s and mining six more blocks..." % zc[0]["s"])
        txid = self.node.spendzerocoinmints([mint["serial hash"]])['txid']
        self.log.info("Spent on tx %s" % txid)
        self.node.generate(6)
        sleep(2)

        rawTx = self.node.getrawtransaction(txid, 1)
        if rawTx is None:
            self.log.warning("rawTx is: %s" % rawTx)
            raise AssertionError("TEST FAILED")
        else:
            assert (rawTx["confirmations"] == 6)

        self.log.info("%s VALID PUBLIC COIN SPEND PASSED" % self.__class__.__name__)

        self.log.info("%s Trying to spend the serial twice now" % self.__class__.__name__)

        serial = zc[0]["s"]
        randomness = zc[0]["r"]
        privkey = zc[0]["k"]

        tx = None
        try:
            tx = self.node.spendrawzerocoin(serial, randomness, DENOM_TO_USE, privkey)
        except JSONRPCException as e:
            self.log.info("GOOD: Transaction did not verify")

        if tx is not None:
            self.log.warning("Tx is: %s" % tx)
            raise AssertionError("TEST FAILED")

        self.log.info("%s DOUBLE SPENT SERIAL NOT VERIFIED, TEST PASSED" % self.__class__.__name__)

        self.log.info("%s Trying to spend using the old coin spend method.." % self.__class__.__name__)

        tx = None
        try:
            tx = self.node.spendzerocoin(DENOM_TO_USE, False, False, "", False)
            raise AssertionError("TEST FAILED, old coinSpend spent")
        except JSONRPCException as e:
            self.log.info("GOOD: spendzerocoin old spend did not verify")


        self.log.info("%s OLD COIN SPEND NON USABLE ANYMORE, TEST PASSED" % self.__class__.__name__)


if __name__ == '__main__':
    zWGRValidCoinSpendTest().main()
