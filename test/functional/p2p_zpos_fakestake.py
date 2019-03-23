#!/usr/bin/env python3
# Copyright (c) 2019 The PIVX Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

'''
Covers the scenario of a zPoS block where the coinstake input is a zerocoin spend
of an already spent coin.
'''

from time import sleep

from test_framework.authproxy import JSONRPCException

from fake_stake.base_test import WAGERR_FakeStakeTest

class zPoSFakeStake(WAGERR_FakeStakeTest):

    def run_test(self):
        self.description = "Covers the scenario of a zPoS block where the coinstake input is a zerocoin spend of an already spent coin."
        self.init_test()

        DENOM_TO_USE = 5000         # zc denomination
        INITAL_MINED_BLOCKS = 321   # First mined blocks (rewards collected to mint)
        MORE_MINED_BLOCKS = 301     # More blocks mined before spending zerocoins
        self.NUM_BLOCKS = 2         # Number of spammed blocks

        # 1) Starting mining blocks
        self.log.info("Mining %d blocks to get to zPOS activation...." % INITAL_MINED_BLOCKS)
        self.node.generate(INITAL_MINED_BLOCKS)
        sleep(2)

        # 2) Collect the possible prevouts and mint zerocoins with those
        self.log.info("Collecting all unspent coins which we generated from mining...")
        balance = self.node.getbalance("*", 100)
        self.log.info("Minting zerocoins...")
        initial_mints = 0
        while balance > DENOM_TO_USE:
            try:
                self.node.mintzerocoin(DENOM_TO_USE)
            except JSONRPCException:
                break
            sleep(1)
            initial_mints += 1
            self.node.generate(1)
            sleep(1)

            if initial_mints % 5 == 0:
                self.log.info("Minted %d coins" % initial_mints)
            if initial_mints >= 70:
                break
            balance = self.node.getbalance("*", 100)
        self.log.info("Minted %d coins in the %d-denom, remaining balance %d", initial_mints, DENOM_TO_USE, balance)
        sleep(2)

        # 3) mine more blocks
        self.log.info("Mining %d more blocks ... and getting spendable zerocoins" % MORE_MINED_BLOCKS)
        self.node.generate(MORE_MINED_BLOCKS)
        sleep(2)
        mints = self.node.listmintedzerocoins(True, True)
        mints_hashes = [x["serial hash"] for x in mints]

        # This mints are not ready spendable, only few of them.
        self.log.info("Got %d confirmed mints" % len(mints_hashes))

        # 4) spend mints
        self.log.info("Spending mints in block %d..." % self.node.getblockcount())
        spends = 0
        spent_mints = []
        for mint in mints_hashes:
            # create a single element list to pass to RPC spendzerocoinmints
            mint_arg = []
            mint_arg.append(mint)
            try:
                self.node.spendzerocoinmints(mint_arg)
                sleep(1)
                spends += 1
                spent_mints.append(mint)
            except JSONRPCException as e:
                self.log.warning(str(e))
                continue
        sleep(1)
        self.log.info("Successfully spent %d mints" % spends)

        # 5) Start mining again so that spends get confirmed in a block.
        self.log.info("Mining 5 more blocks...")
        self.node.generate(5)
        sleep(2)

        # 6) Collect some prevouts for random txes
        self.log.info("Collecting inputs for txes...")
        spending_utxo_list = self.node.listunspent()
        sleep(1)

        # 7) Create "Fake Stake" blocks and send them
        self.log.info("Creating Fake stake zPoS blocks...")
        err_msgs = self.test_spam("Main", mints, spending_utxo_list=spending_utxo_list, fZPoS=True)

        if not len(err_msgs) == 0:
            self.log.error("result: " + " | ".join(err_msgs))
            raise AssertionError("TEST FAILED")

        self.log.info("%s PASSED" % self.__class__.__name__)

if __name__ == '__main__':
    zPoSFakeStake().main()
