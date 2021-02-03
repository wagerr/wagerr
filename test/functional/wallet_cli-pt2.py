#!/usr/bin/env python3
# Copyright (c) 2014-2017 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test the CLI commands sendfrom and sendtoaddressix.

"""
from test_framework.test_framework import BitcoinTestFramework

from test_framework.util import (
    assert_equal,
    wait_until,
    assert_raises_rpc_error,
    connect_nodes_bi,
    disconnect_nodes,
    p2p_port,
)
from time import sleep

from decimal import Decimal

import re
import sys
import os

class SendFromTest (BitcoinTestFramework):
    def set_test_params(self):
        self.setup_clean_chain = True
        self.num_nodes = 2
        #self.extra_args = [["-debug=all"],["-debug=all"]]
        #self.extra_args = [["-staking=1", "-debug=all"],["-staking=1", "-debug=all"]]

    def run_test(self):
        connect_nodes_bi(self.nodes,0,1)
        self.log.info("Mining Blocks...")
        self.nodes[0].generate(2)
        self.nodes[1].generate(102)
        self.sync_all()
        tmpdir=self.options.tmpdir
        self.log.info("Balance of Node 0 %s" % self.nodes[0].getbalance())
        self.log.info("Balance of Node 1 %s" % self.nodes[1].getbalance()) 
        ###
        # Send From
        ###
        sendfrom=self.nodes[0].getnewaddress('SendFrom')
        self.log.info("Sending 500 wgr to %s" % sendfrom)
        self.nodes[1].sendfrom('', sendfrom, 500)
        self.nodes[1].generate(1)
        self.sync_all()
        self.log.info("Balance of address %s %s" % (sendfrom, self.nodes[0].getbalance('SendFrom')))
        ###
        # Send Many
        ###
        smaddr1=self.nodes[0].getnewaddress('SMaddr1')
        smaddr2=self.nodes[0].getnewaddress('SMaddr2')
        self.nodes[1].settxfee(0.0000123)
        self.log.info("Sending 500 wgr to %s" % smaddr1)
        self.log.info("Sending 500 wgr to %s" % smaddr2)
        smtxid=self.nodes[1].sendmany('', {smaddr1: 500, smaddr2: 500})
        self.log.info("SMtxid %s" % smtxid)
        smtransaction=self.nodes[1].gettransaction(smtxid)
        self.log.info("Send Many Transaction Fee %s" % smtransaction['fee'])
        self.nodes[1].generate(1)
        self.sync_all()
        self.log.info("Balance of address %s %s" % (smaddr1, self.nodes[0].getbalance('SMaddr1')))
        self.log.info("Balance of address %s %s" % (smaddr2, self.nodes[0].getbalance('SMaddr2'))) 
        ###
        # Send to Addressix
        ###
        addressix=self.nodes[0].getnewaddress()
        self.nodes[0].setaccount(addressix, 'Addressix')
        self.log.info("Sending 500 wgr to %s" % addressix)
        self.nodes[1].sendtoaddressix(addressix,500)
        self.nodes[1].generate(1)
        self.sync_all()
        self.log.info("Balance of address %s %s" % (addressix, self.nodes[0].getbalance('Addressix')))
        ###
        # Stake thresholds
        ###
        self.nodes[1].generate(146)
        self.sync_all()
        self.log.info("Stake split threshold %s" % self.nodes[0].getstakesplitthreshold())
        assert_equal(2000, self.nodes[0].getstakesplitthreshold())
        self.nodes[0].setstakesplitthreshold(50)
        assert_equal(50, self.nodes[0].getstakesplitthreshold())
        self.log.info("Stake split threshold %s" % self.nodes[0].getstakesplitthreshold())
        self.nodes[0].generate(1)
        self.sync_all()
        self.nodes[1].generate(60)
        self.sync_all()
if __name__ == '__main__':
    SendFromTest().main()
