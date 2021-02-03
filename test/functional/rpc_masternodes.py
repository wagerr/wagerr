#!/usr/bin/env python3
# Copyright (c) 2019-2021 The Wagerr developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test the functionality of all masternode commands.

"""
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import *

from time import sleep
from decimal import Decimal

import re
import sys
import os
import subprocess

class MasterNodeTest (BitcoinTestFramework):
    def set_test_params(self):
        self.setup_clean_chain = True
        self.num_nodes = 2
        #self.extra_args = [["-debug"]]

    def run_test(self):
        tmpdir=self.options.tmpdir
        self.log.info("!!!!!")
        self.log.info("!!!! This will take some time please be patient")
        self.log.info("!!!!!")
        node0rpcport = rpc_port(0)
        node0pubport = p2p_port(0)
        self.log.info("Node 0 Ports rpc %s pub %s" % (node0rpcport, node0pubport))
        self.log.info("Mining Blocks...")
        self.nodes[0].generate(103)
        self.sync_all()
        self.log.info("Block Count Node 0 %s" % self.nodes[0].getblockcount())
        self.log.info("Balance Node 0 %s" % self.nodes[0].getbalance())
        mnpriv0=self.nodes[0].createmasternodekey()
        self.log.info("New key Node 0 %s" % mnpriv0)
        mnaddr0=self.nodes[0].getnewaddress('node0')
        #mnaddr0=self.nodes[0].getaccountaddress('mn0')
        self.log.info("Masternode Address Node 0 %s" % mnaddr0)
        self.log.info("Send 25000 to %s %s" % (mnaddr0, self.nodes[0].sendtoaddress(mnaddr0, 25000)))
        self.log.info("Addresses node 0 %s" % self.nodes[0].listaddressgroupings())
        mnoutputs0=self.nodes[0].getmasternodeoutputs()
        self.log.info("Masternode Outputs Node 0 %s" % mnoutputs0[0])
        outputidx0=mnoutputs0[0]['outputidx']
        self.log.info("Masternode Outputs outputidx Node 0 %s" % outputidx0)
        txhash0=mnoutputs0[0]['txhash']
        self.log.info("Masternode Outputs Node 0 txhash %s" % txhash0)
        #exit(0)
        configfile0=tmpdir+"/node0/wagerr.conf"
        mnconfig0=tmpdir+"/node0/regtest/masternode.conf"
        self.log.info("Config File Node 0 %s" % configfile0)
        self.log.info("Masternode Config Node 0 %s" % mnconfig0)
        wconfigline01="masternodeprivkey=" + mnpriv0 + "\n"
        wconfigline02="masternode=1\n"
        wconfigline03="masternodeaddr=127.0.0.1:55006\n"
        wconfig = open(configfile0,"a")
        wconfig.write(wconfigline01)
        wconfig.write(wconfigline02)
        wconfig.write(wconfigline03)
        wconfig.close()
        #exit(0)
        subprocess.call(['sed','-i','/litemode*/d',configfile0])
        remove="port="+str(node0pubport)
        self.log.info("Remove Port %s" % remove)
        subprocess.call(['sed','-i','/'+remove+'/d',configfile0])
        mnconfigline0="mn0 127.0.0.1:55006 " + mnpriv0 + " " + txhash0 + " " + str(outputidx0) + "\n"
        self.log.info("Masternode Config Line Node 0 %s" % mnconfigline0)
        configmn = open(mnconfig0, 'a')
        configmn.write(mnconfigline0)
        configmn.close()
        self.log.info("Restarting Node 0")
        self.stop_node(0)
        self.start_node(0)
        self.log.info("Confirming Masternode Wallet Transactions")
        connect_nodes_bi(self.nodes, 0, 1)
        self.nodes[0].generate(15)
        self.sync_all()
        self.log.info("Testing startmasternode 'alias, false, mn0'")
        self.log.info("Starting Masternode Node 0 %s" % self.nodes[0].startmasternode('alias','false', 'mn0'))
        self.log.info("Block Count Node 0 %s" % self.nodes[0].getblockcount())
        self.log.info("Restarting Node 0")
        self.stop_node(0)
        self.start_node(0)
        connect_nodes_bi(self.nodes, 0, 1)
        self.log.info("Testing startmasternode 'local, false'")
        self.log.info("Starting Masternode %s" % self.nodes[0].startmasternode('local','false'))
        self.log.info("Restarting Node 0")
        self.stop_node(0)
        self.start_node(0)
        self.log.info("Testing startmasternode 'local, true'")
        self.log.info("Starting Masternode %s" % self.nodes[0].startmasternode('local','true'))
        self.log.info("Restarting Node 0")
        self.stop_node(0)
        self.start_node(0)
        connect_nodes_bi(self.nodes, 0, 1)
        self.log.info("Testing startmasternode 'many, false'")
        self.log.info("Starting Masternode %s" % self.nodes[0].startmasternode('many','false'))
        self.log.info("Restarting Node 0")
        self.stop_node(0)
        self.start_node(0)
        connect_nodes_bi(self.nodes, 0, 1)
        self.log.info("Testing startmasternode 'many, true'")
        self.stop_node(0)
        self.start_node(0)
        connect_nodes_bi(self.nodes, 0, 1)
        self.log.info("Starting Masternode %s" % self.nodes[0].startmasternode('many','true'))
        #self.log.info("Testing startmasternode 'missing, false'")
        #assert_raises_rpc_error(-1, "You can't use this command until masternode list is synced", self.nodes[0].startmasternode('missing','false'))
        #self.log.info("Restarting Node 0")
        #self.stop_node(0)
        #sleep(10)
        #self.start_node(0)
        #self.log.info("Testing startmasternode 'disabled, false'")
        #assert_raises_rpc_error"(-1,"You can't use this command until masternode list is synced", self.nodes[0].startmasternode('disabled','false'))
        #self.log.info("Restarting Node 0")
        #self.stop_node(0)
        #sleep(10)
        #self.start_node(0)
        self.log.info("Testing startmasternode 'alias, true, mn0'")
        self.stop_node(0)
        self.start_node(0)
        connect_nodes_bi(self.nodes, 0, 1)
        self.log.info("Starting Masternode %s" % self.nodes[0].startmasternode('alias','true','mn0'))
        sleep(5)
        mnbroadcast=self.nodes[0].createmasternodebroadcast('alias','mn0')
        self.log.info("Create Masternode Broadcast %s" % mnbroadcast)
        mnbroadcastkey=mnbroadcast['hex']
        self.log.info("Decode masternode Broadcast %s" % self.nodes[0].decodemasternodebroadcast(mnbroadcastkey))
        self.log.info("Relay Masternode Broadcast %s" % self.nodes[0].relaymasternodebroadcast(mnbroadcastkey))
        self.log.info("Masternode Connect %s" % self.nodes[1].masternodeconnect('127.0.0.1:' + str(node0pubport)))
        self.log.info("Spork %s" % self.nodes[0].spork('show'))
        # First superblock needs to be passed
        nextsblock=self.nodes[0].getnextsuperblock()
        self.log.info("Mining past first superblock %s" % nextsblock)
        genblocks=nextsblock-103+1
        self.nodes[0].generate(genblocks)
        self.sync_all()
        sleep(5)
        self.log.info("Block Count Node 0 %s" % self.nodes[0].getblockcount())
        #exit(1)
        nextsblock=self.nodes[0].getnextsuperblock()
        prophash=self.nodes[0].preparebudget("test1", "https://wagerr.com/about", 10, nextsblock, mnaddr0, 10)
        self.log.info("Prepared Budget %s" % prophash)
        self.log.info("Generating 3 blocks to confirm")
        self.nodes[0].generate(3)
        self.sync_all()
        #self.log.info("Get Budget Projection %s" % self.nodes[0].getbudgetprojection())
        self.log.info("Block Count Node 0 %s" % self.nodes[0].getblockcount())
        subhash=self.nodes[0].submitbudget("test1", "https://wagerr.com/about", 10, nextsblock, mnaddr0, 10, prophash)
        self.log.info("Submitting budget %s" % subhash)
        self.log.info("Generating 5 blocks to confirm")
        self.nodes[0].generate(5)
        self.sync_all()
        self.log.info("Get Budget Projection %s" % self.nodes[0].getbudgetprojection())
        self.log.info("Block Count Node 0 %s" % self.nodes[0].getblockcount())
        self.log.info("Check Budgets %s" % self.nodes[0].checkbudgets())
        self.log.info("Vote for budget test1 %s" % self.nodes[0].mnbudgetvote('many', subhash, 'yes'))
        self.log.info("Generating past next superblock %s" % nextsblock)
        genblocks=nextsblock-163+1
        self.nodes[0].generate(genblocks)
        self.sync_all()
        self.log.info("Get Budget Projection %s" % self.nodes[0].getbudgetprojection())
        self.log.info("Block Count Node 0 %s" % self.nodes[0].getblockcount())
        #self.sync_all()
        #sync_blocks(self.nodes)
        #sync_blocks([self.nodes[0], self.nodes[1]])
        #sync_mempools(self.nodes)
        self.log.info("Block Count Node 0 %s" % self.nodes[0].getblockcount())
        self.log.info("Get Budget Info %s" % self.nodes[0].getbudgetinfo('test1'))
        self.log.info("Get Budget Votes %s" % self.nodes[0].getbudgetvotes('test1'))
        self.log.info("Get Masternode Count %s" % self.nodes[0].getmasternodecount())
        self.log.info("Get Masternode Scores %s" % self.nodes[0].getmasternodescores())
        self.log.info("Get Masternode Status %s" % self.nodes[0].getmasternodestatus())
        self.log.info("Get Masternode Winners %s" % self.nodes[0].getmasternodewinners())
        self.log.info("Get  Next Superblock %s" % self.nodes[0].getnextsuperblock())
        self.log.info("List Masternode Conf %s" % self.nodes[0].listmasternodeconf())
        self.log.info("List Masternodes %s" % self.nodes[0].listmasternodes())
        self.log.info("Masternode Debug %s" % self.nodes[0].masternodedebug())
        self.log.info("Check Budgets %s" % self.nodes[0].checkbudgets())
        self.log.info("Budget Info %s" % self.nodes[0].getbudgetinfo('test1'))
        self.log.info("Budget Votes %s" % self.nodes[0].getbudgetvotes('test1'))
        self.log.info("Block Count Node 0 %s" % self.nodes[0].getblockcount())
        self.log.info("Masternode sync status %s" % self.nodes[0].mnsync("status"))
        self.log.info("Masternode sync reset %s" % self.nodes[0].mnsync("reset"))
        self.log.info("Get Budget Projection %s" % self.nodes[0].getbudgetprojection())

"""
Not Working -- TODO -- Fix it
masternodeconnect
masternodecurrent
mnbudgetrawvote
mnfinalbudget
startmasternode
    - [ ] missing
    - [ ] disabled

"""
if __name__ == '__main__':
    MasterNodeTest().main()
