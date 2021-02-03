#!/usr/bin/env python3
# Copyright (c) 2014-2017 The Bitcoin Core developers
# Copyright (c) 2019-2021 The Wagerr Developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test the functionality of all CLI commands.

"""
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import (
    assert_equal,
    assert_raises_rpc_error,
    wait_until,
    assert_raises_rpc_error,
    connect_nodes_bi,
    disconnect_nodes,
    p2p_port,
)
from time import sleep

import pprint

from decimal import Decimal

import re
import os

class WalletCLITest (BitcoinTestFramework):
    def set_test_params(self):
        self.setup_clean_chain = True
        self.num_nodes = 2

    def run_test(self):
        ###
        # Help
        ###
        self.log.info("\nTHe Following commands will be tested\n %s " % self.nodes[0].help())
        ###
        # get info
        ###
        self.log.info("\nInfo %s " % self.nodes[0].getinfo())
        ###
        # mine blocks
        ###
        connect_nodes_bi(self.nodes, 0, 1)
        self.log.info("Mining blocks...")
        self.nodes[0].generate(2)
        #sleep(10)
        self.nodes[1].generate(102)
        #sleep(10)
        self.sync_all([self.nodes[0:1]])
        tmpdir=self.options.tmpdir
        ###
        # check block count
        ###
        blockcount=self.nodes[0].getblockcount()
        self.log.info("Block count Node 0 %s" % blockcount)
        blockcount1=self.nodes[1].getblockcount()
        self.log.info("Block count Node 1 %s" % blockcount1)
        ###
        # get accumultor values
        ###
        self.log.info("Accumulator Values %s" % self.nodes[0].getaccumulatorvalues(blockcount))
        ###
        # get bestblockhash
        ###
        bestblockhash=self.nodes[0].getbestblockhash()
        self.log.info("Best Block Hash %s" % bestblockhash)
        ###
        # get block contents and compare to bestblockhash
        ###
        blockcontents=self.nodes[0].getblock(bestblockhash)
        self.log.info("Block Hash %s" % blockcontents['hash'])
        self.log.info("\nBlock Contents %s " % blockcontents)
        assert_equal(bestblockhash, blockcontents['hash'])
        self.log.info("Blockhash from block and Bestblockhash match\n")
        ###
        # List address groupings
        ###
        self.log.info("\nAddressgroupings Node 0 %s " % self.nodes[0].listaddressgroupings())
        self.log.info("\nAddressgroupings Node 1 %s " % self.nodes[1].listaddressgroupings())
        ###
        # get  balances and addresses
        ###
        self.log.info("Balance Node 0 %s " % self.nodes[0].getbalance())
        n0add=self.nodes[0].getaccountaddress("")
        self.log.info("Address Node 0 %s " % n0add)
        self.log.info("Balance Node 1 %s " % self.nodes[1].getbalance())
        n1add=self.nodes[1].getaccountaddress("")
        self.log.info("Address Node 1 %s " % n1add)
        ###
        # get blockchain info
        ###
        self.sync_all()
        bestblockhash=self.nodes[0].getbestblockhash()
        blockchaininfo=self.nodes[0].getblockchaininfo()
        self.log.info("\nBlockchaininfo %s" % blockchaininfo)
        assert_equal(bestblockhash, blockchaininfo['bestblockhash'])
        self.log.info("Blockhash from blockchaininfo and Bestblockhash match\n")
        ###
        # get and test blockhash and height
        ###
        blockhash=self.nodes[0].getblockhash(blockcount)
        self.log.info("Blockhash %s" % blockhash)
        blockheader=self.nodes[0].getblockheader(blockhash)
        self.log.info("Block Header %s" % blockheader['hash'])
        assert_equal(blockheader['hash'], blockhash)
        self.log.info("Hash from blockheader matches blockhash")
        assert_equal(blockheader['height'], blockcount)
        self.log.info("Blockheader height matches blockcount")
        ###
        # getchaintips, this is a long one
        ###
        self.sync_all()
        chaintips0=self.nodes[0].getchaintips()
        self.log.info("Chaintips 0 %s" % chaintips0)
        self.log.info("Peerinfo %s" % self.nodes[0].getpeerinfo())
        address1 = self.nodes[0].getpeerinfo()[1]['addr']
        self.log.info("Address1 %s" % address1)
        self.nodes[0].disconnectnode(address1)
        wait_until(lambda: len(self.nodes[0].getpeerinfo()) == 1, timeout=10)
        self.nodes[1].generate(1)
        chaintips1=self.nodes[1].getchaintips()
        chaintips0=self.nodes[0].getchaintips()
        self.log.info("Chaintips 0 %s" % chaintips0)
        self.log.info("Chaintips 1 %s" % chaintips1)
        assert not (chaintips0 == chaintips1)
        self.log.info("Chaintips do not match")
        connect_nodes_bi(self.nodes, 0, 1)  # reconnect the node
        assert_equal(len(self.nodes[0].getpeerinfo()), 2)
        chaintips1=self.nodes[1].getchaintips()
        chaintips0=self.nodes[0].getchaintips()
        self.log.info("Chaintips 0 %s" % chaintips0)
        self.log.info("Chaintips 1 %s" % chaintips1)
        assert_equal(chaintips0[0]['hash'], chaintips1[0]['hash'])
        self.log.info("Hash from chaintips match each other")
        assert_equal(chaintips0[0]['height'], chaintips1[0]['height'])
        self.log.info("Height from chaintips match each other")
        ###
        # get difficulty
        ###
        self.log.info("Diffficulty %s " % self.nodes[0].getdifficulty())
        ###
        # get fee info
        ###
        self.log.info("Fee Info %s " % self.nodes[0].getfeeinfo((self.nodes[0].getblockcount()-1)))
        ###
        # get mempool info
        ###
        self.log.info("Mempool Info %s " % self.nodes[0].getmempoolinfo())
        ###
        # get raw mempool info
        ###
        self.log.info("Raw Mempool Info %s " % self.nodes[0].getrawmempool(True))
        ###
        # get txout info
        ###
        #self.log.info("Transaction out Info %s " % self.nodes[0].gettxout(TRANSACTIONID))
        ###
        # get txoutset info node 0 and 1
        ###
        self.log.info("TX Outset Info Node 0 %s " % self.nodes[0].gettxoutsetinfo())
        self.log.info("TX Outset Info Node 1 %s " % self.nodes[1].gettxoutsetinfo())
        ###
        # Verify Chain
        ###
        self.log.info("Chain Verification %s " % self.nodes[0].verifychain())
        ###
        # Getgenerate
        ###
        self.log.info("Get Generation Status %s " % self.nodes[0].getgenerate())
        ###
        # Get hashes per second
        ###
        self.log.info("Hashes per Second %s " % self.nodes[0].gethashespersec())
        ###
        # Setgenerate does not work anymore, error code is not recognized
        ###
        # assert_raises_rpc_error(-32601, "Use the generate method instead of setgenerate on this network", self.nodes[0].setgenerate(True, 1))
        ###
        # Get Block Template
        ###
        self.log.info("Block Template \n %s " % self.nodes[0].getblocktemplate())
        ###
        # Get Mining Info
        ###
        self.log.info("Mining Info \n %s " % self.nodes[0].getmininginfo())
        ###
        # Get Network Hashes per Second
        ###
        self.log.info("Network Hashes per Second %s " % self.nodes[0].getnetworkhashps())
        ###
        # Prioritize Transaction
        ###
        #prioritisetransaction <txid> <priority delta> <fee delta>
        ###
        # Reserve Balance
        ###
        reservebalance=self.nodes[0].reservebalance()
        self.log.info("Reserve Balance False %s " % reservebalance)
        reservebalance=self.nodes[0].reservebalance(True, 100)
        self.log.info("Reserve Balance True %s " % reservebalance)
        assert_equal(reservebalance['amount'], 100)
        assert_equal(reservebalance['reserve'], True)
        self.log.info("Reserve Balance Set to 100 True")
        reservebalance=self.nodes[0].reservebalance(False)
        assert_equal(reservebalance['amount'], 0)
        assert_equal(reservebalance['reserve'], False)
        reservebalance=self.nodes[0].reservebalance()
        self.log.info("Reserve Balance Set back to False %s " % reservebalance)
        ###
        # Submit Block
        ###
        #submitblock "hexdata" ( "jsonparametersobject" ) -- TODO
        ###
        # setban, listbanned and clearbanned tests
        ###
        self.log.info("Test setban and listbanned RPCs")
        assert_equal(len(self.nodes[1].getpeerinfo()), 2)  # node1 should have 2 connections to node0 at this point
        self.nodes[1].setban("127.0.0.1", "add")
        wait_until(lambda: len(self.nodes[1].getpeerinfo()) == 0, timeout=10)
        assert_equal(len(self.nodes[1].getpeerinfo()), 0)  # all nodes must be disconnected at this point
        assert_equal(len(self.nodes[1].listbanned()), 1)
        self.log.info("setban: successfully banned a single IP address")
        self.nodes[1].clearbanned()
        assert_equal(len(self.nodes[1].listbanned()), 0)
        connect_nodes_bi(self.nodes, 0, 1)
        assert_equal(len(self.nodes[1].getpeerinfo()), 2)
        self.log.info("clearbanned: successfully cleared ban list")
        ###
        # addnode/getaddednodeinfo
        ###
        self.log.info("Node info %s" % self.nodes[0].getconnectioncount())
        self.log.info("Node info %s" % self.nodes[0].getconnectioncount())
        ip_port = "127.0.0.1:{}".format(p2p_port(2))
        self.nodes[0].addnode(ip_port, 'add')
        # check that the node has indeed been added
        added_nodes = self.nodes[0].getaddednodeinfo(True, ip_port)
        self.log.info("Added Node %s" % added_nodes)
        self.log.info("Added Node Info %s" % self.nodes[1].getaddednodeinfo(True))
        self.nodes[0].addnode(ip_port, 'remove')
        assert_raises_rpc_error(-24, "Error: Node has not been added", self.nodes[0].getaddednodeinfo, True, ip_port)
        self.log.info("Added Node Info %s" % self.nodes[1].getaddednodeinfo(True))
        self.nodes[0].addnode(ip_port, 'add')
        added_nodes = self.nodes[0].getaddednodeinfo(True, ip_port)
        self.log.info("Added Node %s" % added_nodes)
        ###
        # Get Connection Count
        ###
        self.log.info("Connection Count %s" % self.nodes[0].getconnectioncount())
        ###
        # Get Net Totals
        ###
        self.log.info("Network Totals\n %s" % self.nodes[0].getnettotals())
        ###
        # Get Network Info
        ###
        self.log.info("Network Info\n %s" % self.nodes[0].getnetworkinfo())
        ###
        # Ping
        ###
        # Doesn't increment sent/rec'd counters -- TODO
        self.log.info("Peer info Before Ping\n %s" % self.nodes[0].getpeerinfo())
        self.log.info("Send Ping")
        self.nodes[0].ping()
        self.nodes[1].ping()
        self.log.info("Peer info After Ping\n %s" % self.nodes[1].getpeerinfo())
        ###
        # Rawtransactions
        ###
        unspent=self.nodes[1].listunspent(0)
        self.log.info("Unspent\n%s" % unspent)
        Transaction=unspent[0]
        self.log.info("Transaction Details\n%s" % Transaction)
        newtxid=Transaction['txid']
        self.log.info("txid\n%s" % newtxid)
        pubkey=Transaction['scriptPubKey']
        self.log.info("pubKey\n%s" % pubkey)
        privKeys = ['TCgkoWvkgnbvCyExTrkkbm4X6J5xWdib8zPP43aDyrdN2v7Jnc96']
        self.nodes[1].importprivkey(privKeys[0])
        inputs = [
            # Valid pay-to-pubkey script
            {'txid': newtxid, 'vout': 0,
            'scriptPubKey': pubkey}
        ]

        outputs = {'TJmzF7yUPSvK7f977ZbZLrb7dHD7E757mj': 0.1}

        rawTx = self.nodes[1].createrawtransaction(inputs, outputs)
        self.log.info("Raw TX %s" % rawTx)
        decodedTx=self.nodes[1].decoderawtransaction(rawTx)
        self.log.info("Decoded TX %s" % decodedTx)
        txHex=decodedTx['hex']
        self.log.info("TX Hex %s" % txHex)
        rawTxSigned = self.nodes[1].signrawtransaction(rawTx, inputs)
        self.log.info("Raw TX Signed %s" % rawTxSigned)
        self.log.info("Raw TX Signed Hex %s" % rawTxSigned['hex'])
        self.log.info("Decoded script %s", self.nodes[1].decodescript(pubkey))
        #self.nodes[1].sendrawtransaction(rawTxSigned['hex']) # rawTxSigned not working -- TODO -- fix it
        node0received=self.nodes[0].getnewaddress()
        txIdNotBroadcasted  = self.nodes[1].sendtoaddress(node0received, 2)
        txObjNotBroadcasted = self.nodes[1].gettransaction(txIdNotBroadcasted)
        self.nodes[0].generate(1) #mine a block, tx should not be in there
        self.sync_all()

        #now broadcast from another node, mine a block, sync, and check the balance
        self.nodes[0].sendrawtransaction(txObjNotBroadcasted['hex'])
        self.nodes[0].generate(1)
        self.sync_all()
        txObjNotBroadcasted = self.nodes[1].gettransaction(txIdNotBroadcasted)
        self.log.info("TX Object %s" % txObjNotBroadcasted)
        self.log.info("Tx OUT %s " % self.nodes[0].gettxout(txIdNotBroadcasted, 0, True))
        ###
        # Multisig
        ###
        self.log.info("Create And Add Multisig Addresses")

        self.log.info("Node 0 Blocks %s" % self.nodes[0].getblockcount())
        self.log.info("Node 1 Blocks %s" % self.nodes[1].getblockcount())

        self.log.info("Node 0 Balance %s" % self.nodes[0].getbalance())
        self.log.info("Node 1 Balance %s" % self.nodes[1].getbalance())
     
        addr1 = self.nodes[0].getnewaddress()
        addr2 = self.nodes[0].getnewaddress()

        addr1Obj = self.nodes[0].validateaddress(addr1)
        addr2Obj = self.nodes[0].validateaddress(addr2)

        # Tests for createmultisig and addmultisigaddress
        # createmultisig can only take public keys
        self.nodes[1].createmultisig(2, [addr1Obj['pubkey'], addr2Obj['pubkey']])
        mSigObj = self.nodes[0].addmultisigaddress(2, [addr1Obj['pubkey'], addr1])
        self.log.info("Multisig Signature %s" % mSigObj)
        bal = self.nodes[0].getbalance()

        # send 10 Wagerr to msig adr
        txId = self.nodes[1].sendtoaddress(mSigObj, 10)
        self.sync_all()
        self.nodes[1].generate(1)
        self.sync_all()

        self.log.info("Node 0 Blocks %s" % self.nodes[0].getblockcount())
        self.log.info("Node 1 Blocks %s" % self.nodes[1].getblockcount())
        
        self.log.info("Node 0 Balance %s" % self.nodes[0].getbalance())
        self.log.info("Node 1 Balance %s" % self.nodes[1].getbalance())
        ###
        # estimate fee
        ###
        self.log.info("Generating Transactions for Fee Estimate")
        feeest=-1
        while feeest==-1:
            feeaddr=self.nodes[0].getnewaddress()
            self.nodes[1].sendtoaddress(feeaddr, 1000)
            self.nodes[0].generate(1)
            feeest=self.nodes[1].estimatefee(10)
        self.log.info("Fee Estimate Node 0 10 Blocks %s" % self.nodes[0].estimatefee(10))
        self.log.info("Fee Estimate Node 1 10 Blocks %s" % self.nodes[1].estimatefee(10))
        self.log.info("Block Count Node 0 %s" % self.nodes[0].getblockcount())
        self.log.info("Block Count Node 1 %s" % self.nodes[1].getblockcount())
        ###
        # estimate priority -- always -1 as far as i've found so far
        ###
        #priest=-1
        #while priest==-1:
        priaddr=self.nodes[0].getnewaddress()
        self.nodes[1].sendtoaddress(priaddr, 1000)
        self.nodes[0].generate(1)
            #priest=self.nodes[0].estimatepriority(10)
        self.log.info("Priority Estimate Node 0 %s" % self.nodes[0].estimatepriority(10))
        self.log.info("Priority Estimate Node 1 %s" % self.nodes[1].estimatepriority(10))
        self.log.info("Block Count Node 0 %s" % self.nodes[0].getblockcount())
        self.log.info("Block Count Node 1 %s" % self.nodes[1].getblockcount())
        ###
        # sign and verify message
        ###
        messAddr=self.nodes[0].getnewaddress()
        self.log.info("Message Address %s" % messAddr)
        messPriv=self.nodes[0].dumpprivkey(messAddr)
        self.log.info("Message Priv Key %s" % messPriv)
        messSignature=self.nodes[0].signmessage(messAddr, "Test Message")
        self.log.info("Message Signature %s" % messSignature)
        self.log.info("Message Received %s" % self.nodes[1].verifymessage(messAddr, messSignature, "Test Message"))
        ###
        # backup and restore wallet
        ###
        self.log.info("Wallet Info %s" % self.nodes[0].getwalletinfo())
        self.log.info("Backup Wallet")
        self.nodes[0].backupwallet(tmpdir + "/node0/backup.dat")
        self.log.info("Restore Wallet")
        self.nodes[0].importwallet(tmpdir + "/node0/backup.dat")
        self.log.info("Wallet Info %s" % self.nodes[0].getwalletinfo())
        ###
        #  bip38encrypt/decrypt
        ####
        self.log.info("Balance Node 0 %s" % self.nodes[0].getbalance())
        self.log.info("Balance Node 1 %s" % self.nodes[1].getbalance())
        self.nodes[1].dumpwallet(tmpdir + "/node1/regtest/test1.orig")
        newaddr=self.nodes[0].getnewaddress("Test account")
        self.log.info("Address %s" % newaddr)
        self.log.info("Private Key %s " % self.nodes[0].dumpprivkey(newaddr))
        passphrase="abc123"
        enckey=self.nodes[0].bip38encrypt(newaddr, passphrase)
        self.log.info("Encrypted Key %s" % enckey['Encrypted Key'])
        deckey=self.nodes[1].bip38decrypt(enckey['Encrypted Key'], passphrase)
        self.log.info("Decrypted Key %s", deckey)
        self.log.info("Balance Node 0 %s" % self.nodes[0].getbalance())
        self.log.info("Balance Node 1 %s" % self.nodes[1].getbalance())
        self.log.info("Tmpdir %s" % tmpdir)
        self.nodes[0].dumpwallet(tmpdir + "/node0/regtest/test0")
        self.nodes[1].dumpwallet(tmpdir + "/node1/regtest/test1")
        file=open(tmpdir + "/node0/regtest/test0", "r")
        for line in file:
             if re.search(newaddr, line):
                 self.log.info("Address Found in Node 0 %s " % line)
                 test0succ=True
        file=open(tmpdir + "/node1/regtest/test1", "r")
        for line in file:
             if re.search(newaddr, line):
                 self.log.info("Address Found in Node 1 %s " % line)
                 test1succ=True
        if test0succ and test1succ:
             self.log.info("Successful import of address using encrypted key")
        self.log.info("Removing address from wallet and restarting Node 1")
        self.stop_node(1)
        sleep(10)
        os.rename(tmpdir + "/node1/regtest/wallet.dat", tmpdir + "/node1/regtest/wallet.old")
        self.start_node(1)
        self.nodes[1].importwallet(tmpdir + "/node1/regtest/test1.orig")
        ###
        # enable autocombinerewards
        ####
        self.nodes[0].autocombinerewards(True, 5)
        self.nodes[1].autocombinerewards(True, 5)        
        self.log.info("Autocombine Rewards Successful")
        ###
        # automint addresses
        ###
        self.log.info("Automint Address Node 0 %s" % self.nodes[0].createautomintaddress())
        self.log.info("Automint Address Node 1 %s" % self.nodes[1].createautomintaddress())
        ###
        # encrypt wallet set up staking
        ###
        self.log.info("Wallet Encryption %s" % self.nodes[0].encryptwallet("123abc"))
        self.log.info("Restarting node 0")
        self.stop_node(0)
        sleep(10)
        self.start_node(0)
        self.log.info("Wallet Passphrase Change %s" % self.nodes[0].walletpassphrasechange("123abc", "abc123"))
        self.log.info("Blocks %s" % self.nodes[0].getblockcount())
        #self.log.info("Generating 150 Blocks") # <-- will work once staking is fixed
        #for i in range(150):
        #   self.nodes[0].generate(1)
        #   self.log.info("Node 0 Blocks %s" % self.nodes[0].getblockcount())
        #sleep(10)
        self.log.info("Node 0 Blocks %s" % self.nodes[0].getblockcount())
        self.log.info("Node 1 Blocks %s" % self.nodes[1].getblockcount())
        self.log.info("Wallet Staking test") # need staking wallet
        self.log.info("Staking Status %s" % self.nodes[0].getstakingstatus())
        self.nodes[0].walletpassphrase("abc123", 0, True)
        self.nodes[0].generate(4)
        self.log.info("Staking Status %s" % self.nodes[0].getstakingstatus())
        self.log.info("Lock Wallet %s" % self.nodes[0].walletlock())
        self.log.info("Un-Encrypt Wallet")
        self.nodes[0].walletpassphrase("abc123", 10)
        self.nodes[0].dumpwallet(tmpdir + "/node0/regtest/wallet.backup")
        self.stop_node(0)
        sleep(10)
        os.rename(tmpdir + "/node0/regtest/wallet.dat", tmpdir + "/node0/regtest/wallet.old")
        self.start_node(0)
        self.nodes[0].importwallet(tmpdir + "/node0/regtest/wallet.backup")
        self.log.info("Wallet successfully re-imported and unencrypted")
        ###
        # getaccount
        ###
        self.log.info("Account Name for %s %s" % (newaddr, self.nodes[0].getaccount(newaddr)))
        ###
        # list acccounts
        ###
        self.log.info("Accounts Node 0 %s" % self.nodes[0].listaccounts())
        self.log.info("Accounts Node 1 %s" % self.nodes[1].listaccounts())
        ###
        # Get Addresses by account
        ###
        self.log.info("Address for Test Account %s" % self.nodes[0].getaddressesbyaccount("Test account"))
        ###
        # Get Extended Balance
        ###
        self.log.info("\nExtended balance Node 0\n%s" % self.nodes[0].getextendedbalance())
        self.log.info("\nExtended balance Node 1\n%s" % self.nodes[1].getextendedbalance())
        ###
        # Create Raw Change Address
        ###
        self.log.info("Raw Change address Node 0 %s" % self.nodes[0].getrawchangeaddress())
        ###
        # Get Received by account
        ###
        self.log.info("Received by Node 0 default account %s" % self.nodes[0].getreceivedbyaccount(""))
        self.log.info("Received by Node 1 default account %s" % self.nodes[1].getreceivedbyaccount(""))
        ###
        # Get Received by Address
        ###
        self.log.info("Received by address %s %s" % (node0received, self.nodes[0].getreceivedbyaddress(node0received)))
        ###
        # Unconfirmed Balances
        ###
        self.log.info("Unconfirmed Balance Node 0 %s" % self.nodes[0].getunconfirmedbalance())
        self.log.info("Unconfirmed Balance Node 1 %s" % self.nodes[1].getunconfirmedbalance())
        ###
        # Importaddress
        ###
        self.log.info("Import address TKmHSnVXtUp8YkxuNBzQdssAf96sf9q2Qa to Node 0 %s" % self.nodes[0].importaddress("TKmHSnVXtUp8YkxuNBzQdssAf96sf9q2Qa", "New Test"))
        self.log.info("Address in wallet %s" % self.nodes[0].getaddressesbyaccount("New Test"))
        ###
        # Import Private Key
        ###
        self.log.info("Import Private Key TJj3fvF52KSv7WaRtXZUUW4THSrnKxMVCwpizZU3JxS4v1ZuJJB1")
        self.nodes[0].importprivkey("TJj3fvF52KSv7WaRtXZUUW4THSrnKxMVCwpizZU3JxS4v1ZuJJB1", "PrivKeyTest")
        self.log.info("Address in wallet should be TKLiuGFeXkU6f9JncerMoyTVaCSA2StES6 %s" % self.nodes[0].getaddressesbyaccount("PrivKeyTest"))
        ###
        # lock/unlock unspent
        ###
        self.log.info("Balance Node 1 %s" % self.nodes[1].getbalance())
        txid=self.nodes[1].sendtoaddress(self.nodes[0].getnewaddress(), 1000)
        unspent_0 = {"txid":txid, "vout":0}
        self.nodes[1].lockunspent(False, [unspent_0])
        self.log.info("Locked Unspent %s" % self.nodes[1].listlockunspent())
        self.nodes[1].lockunspent(True, [unspent_0])
        self.log.info("Locked Unspent %s" % self.nodes[1].listlockunspent())
        ###
        # keypool refill
        ###
        self.log.info("Refilling Keypool")
        self.nodes[0].keypoolrefill(200)
        ###
        # Received by Account
        ###
        self.log.info("Recieved by Account\n%s" % self.nodes[0].listreceivedbyaccount(1, True))
        ###
        # Received by Address
        ###
        self.log.info("Recieved by Address\n%s" % self.nodes[0].listreceivedbyaddress(1, True))
        ###
        # List Since Block 
        ###
        recbhash=self.nodes[0].getblockhash(self.nodes[0].getblockcount()-10)
        self.log.info("Transactions since Block %s\n%s" % (recbhash, self.nodes[0].listsinceblock(recbhash)))
        ###
        # List Transaction Records
        ###
        self.log.info("Last 10 Transaction Records %s" % self.nodes[0].listtransactionrecords())
        ###
        # List Transactions
        ###
        self.log.info("Last 10 Transactions %s" % self.nodes[0].listtransactions())
        ###
        # Move Coins
        ###
        movaddr=self.nodes[0].getnewaddress('Move')
        self.log.info("Balance of new account %s\n%s" % (movaddr, self.nodes[0].getbalance('Move')))
        self.nodes[0].move('', 'Move', 1000)
        self.log.info("New Balance of account %s\n%s" % (movaddr, self.nodes[0].getbalance('Move')))
        
            
if __name__ == '__main__':
    WalletCLITest().main()
