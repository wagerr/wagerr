#!/usr/bin/env python3
# Copyright (c) 2014-2017 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test the functionality of all Multisend commands.
   Cannot do masternode activate in these tests, will do them in the 
   masternode test script.
"""
from test_framework.test_framework import BitcoinTestFramework

from test_framework.util import *

from time import sleep

from decimal import Decimal

import re
import sys
import os

class WalletCLITest3 (BitcoinTestFramework):
    def set_test_params(self):
        self.setup_clean_chain = True
        self.num_nodes = 1
        #self.extra_args = [["-debug"],["-debug"]]

    def run_test(self):
        self.log.info("Mining Blocks...")
        self.nodes[0].generate(251)
        #self.sync_all()
        tmpdir=self.options.tmpdir
        ###
        # Staking Test
        ###
        self.log.info("Block Count Node 0 %s" % self.nodes[0].getblockcount())
        count=1
        self.log.info("Staking 5 blocks")
        while count < 5:
            self.nodes[0].generate(1)
            count=count+1
            self.log.info("Block Count %s" % self.nodes[0].getblockcount())
        self.log.info("Block Count %s" % self.nodes[0].getblockcount())
        self.log.info("Staking Status %s" % self.nodes[0].getstakingstatus())
        
        ###
        # multisend
        ###
        """ multisend <command>
        ****************************************************************
        WHAT IS MULTISEND?
        MultiSend allows a user to automatically send a percent of their stake reward to as many addresses as you would like
        The MultiSend transaction is sent when the staked coins mature (100 confirmations)
        ****************************************************************
        TO CREATE OR ADD TO THE MULTISEND VECTOR:
        multisend <WAGERR Address> <percent>
        This will add a new address to the MultiSend vector
        Percent is a whole number 1 to 100.
        ****************************************************************
        MULTISEND COMMANDS (usage: multisend <command>)
        print - displays the current MultiSend vector 
        clear - deletes the current MultiSend vector 
        enablestake/activatestake - activates the current MultiSend vector to be activated on stake rewards
        enablemasternode/activatemasternode - activates the current MultiSend vector to be activated on masternode rewards
        disable/deactivate - disables the current MultiSend vector 
        delete <Address #> - deletes an address from the MultiSend vector 
        disable <address> - prevents a specific address from sending MultiSend transactions
        enableall - enables all addresses to be eligible to send MultiSend transactions
        ****************************************************************
        """
        msaddr1=self.nodes[0].getnewaddress('msaddr1')
        msaddr2=self.nodes[0].getnewaddress('msaddr2')
        msaddr3=self.nodes[0].getnewaddress('msaddr3')
        self.log.info("Address 1 %s" % self.nodes[0].multisend(msaddr1, '20'))
        self.log.info("Address 2 %s" % self.nodes[0].multisend(msaddr2, '30'))
        self.log.info("Address 3 %s" % self.nodes[0].multisend(msaddr3, '50'))
        self.log.info("\nMultisend Enable Staking\n%s" % self.nodes[0].multisend('enablestake'))
        count=1
        self.log.info("Staking 200 Blocks")
        while count < 20:
            self.nodes[0].generate(10)
            count=count+1
            self.log.info("Block Count Node 0 %s" % self.nodes[0].getblockcount())
        self.log.info("\nMultisend Status\n%s" % self.nodes[0].multisend('print'))
        self.log.info("Balance of Addresses %s" % self.nodes[0].listaddressgroupings())
        self.log.info("\nDisable\n%s" % self.nodes[0].multisend('disable'))
        self.log.info("\nRe-Enable\n%s" % self.nodes[0].multisend('activatestake'))
        self.log.info("\nDisable Address %s\n%s" % (msaddr2, self.nodes[0].multisend('disable', msaddr2)))
        self.log.info("\nEnable All\n%s" % self.nodes[0].multisend('enableall'))
        self.log.info("\nDelete Address 0\n%s" % self.nodes[0].multisend('delete', '0'))
        self.log.info("\nClear\n%s" % self.nodes[0].multisend('clear'))
        self.log.info("\nMultisend Status\n%s" % self.nodes[0].multisend('print'))

if __name__ == '__main__':
    WalletCLITest3().main()
