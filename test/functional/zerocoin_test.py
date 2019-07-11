#!/usr/bin/env python3
# Copyright (c) 2014-2017 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test the functionality of Zerocoin commands.

"""
from test_framework.test_framework import BitcoinTestFramework

from test_framework.util import (
    assert_equal,
    wait_until,
    assert_raises_rpc_error,
    connect_nodes_bi,
    connect_nodes,
    disconnect_nodes,
    p2p_port,
)

from time import sleep

from decimal import Decimal

import re
import sys
import os

class ZeroCoinTest (BitcoinTestFramework):
    def set_test_params(self):
        self.setup_clean_chain = True
        self.num_nodes = 1
        #self.extra_args = [["-debug"]]

    def run_test(self):
        tmpdir=self.options.tmpdir
        self.log.info("!!!!!")
        self.log.info("!!!! This will take a great deal of time please be patient")
        self.log.info("!!!!!")
        self.nodes[0].getnewaddress('Main')
        self.log.info("Mine to Block 301")
        self.nodes[0].generate(301)
        self.log.info("Zerocoin block reached")
        self.log.info("Block Count Node 0 %s\n" % self.nodes[0].getblockcount())
        self.log.info("Minting zerocoins")
        zerocoinmint1=self.nodes[0].mintzerocoin(9876)
        self.log.info("Zerocoin Mint\n%s" % zerocoinmint1)
        self.log.info("Number of Zerocoin Mints\n%s" % len(zerocoinmint1))
        self.log.info("Zerocoin Balance\n%s" % self.nodes[0].getzerocoinbalance())
        self.log.info("Zerocoin Amounts\n%s" % self.nodes[0].listzerocoinamounts())
        self.log.info("Generating 48 more blocks to get to block before zerocoin block zcSpends")
        self.nodes[0].generate(48)
        self.log.info("Waiting 35 Sec minute to mature zcSpend chain")
        sleep(35)
        self.log.info("Generating 51 more blocks to get to enable zerocoin block zcSpends")
        self.nodes[0].generate(51)
        self.log.info("Zerocoin Balance\n%s" % self.nodes[0].getzerocoinbalance())
        self.log.info("Zerocoin Amounts\n%s" % self.nodes[0].listzerocoinamounts())
        self.log.info("DZwgr State\n%s" % self.nodes[0].dzwgrstate())
        self.log.info("Zerocoin Export\n%s" % self.nodes[0].exportzerocoins(True))
        self.log.info("Minted Zerocoins\n%s" % self.nodes[0].listmintedzerocoins(True,True))
        self.log.info("Mint Lst\n%s" % self.nodes[0].generatemintlist(1, 10))
        for i in range(0,len(zerocoinmint1)):
            self.log.info("Zerocoin Mint %s %s" % (i, zerocoinmint1[i]))
            self.log.info("Hashes %s" % zerocoinmint1[i]['serialhash'])
            self.log.info("Value %s" % zerocoinmint1[i]['value'])
        #self.log.info("Stake %s" % self.nodes[0].createrawzerocoinstake(zerocoinmint[i]['serialhash']))
        self.log.info("Zerocoin Balance\n%s" % self.nodes[0].getzerocoinbalance())
        zcspend0=self.nodes[0].getnewaddress('ZcSpend0')
        zcspend=self.nodes[0].getnewaddress('ZcSpend')
        self.log.info("Spend address Balance %s" % self.nodes[0].getbalance('ZcSpend'))
        ZCSpendZ=self.nodes[0].spendzerocoin(6666, True, True, zcspend)
        self.log.info("Spend Zerocoin %s" % ZCSpendZ)
        ZCTxID=ZCSpendZ['txid']
        self.log.info("Transaction ID %s" % ZCTxID)
        ZCTrans=self.nodes[0].gettransaction(ZCTxID)
        self.log.info("Transaction Outputs %s" % ZCTrans)
        self.log.info("Number of Transactions %s" % len(ZCTrans))
        self.nodes[0].generate(1)
        self.log.info("Spend address Balance %s" % self.nodes[0].getbalance('ZcSpend'))
        self.log.info("Zerocoin Balance\n%s" % self.nodes[0].getzerocoinbalance())
        self.log.info("address Balance %s" % self.nodes[0].listaddressgroupings())
        for i in range(0, (len(ZCTrans)-2)):
            self.log.info("Spent zerocoins %s" % self.nodes[0].getspentzerocoinamount(ZCTxID, i))
        self.log.info("Spent zerocoin pubkeys %s" % self.nodes[0].listspentzerocoins())
        self.log.info("Archived Zerocoins %s" % self.nodes[0].getarchivedzerocoin())
        self.log.info("Reconsidered Zerocoins %s" % self.nodes[0].reconsiderzerocoins())
        self.log.info("Reset Zerocoin Mints %s" % self.nodes[0].resetmintzerocoin())
        self.log.info("Reset Spent Zerocoin %s" % self.nodes[0].resetspentzerocoin())
        self.log.info("Search DZWgr %s" % self.nodes[0].searchdzwgr(1, 100, 2))
        self.log.info("Original ZWGR Seed %s" % self.nodes[0].getzwgrseed())
        self.log.info("Set ZWGR Seed\n%s" % self.nodes[0].setzwgrseed('884d99151a2c20105f80196cd03f2eb62176a366ea85aa61221625ff39046de0'))
        self.log.info("New ZWGR Seed\n%s" % self.nodes[0].getzwgrseed())

if __name__ == '__main__':
    ZeroCoinTest().main()
