#!/usr/bin/env python3
# Copyright (c) 2014-2017 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test the wallet."""
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import *

class WalletTest(BitcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 4
        self.setup_clean_chain = True

    def setup_network(self):
        self.add_nodes(4)
        self.start_node(0)
        self.start_node(1)
        self.start_node(2)
        connect_nodes_bi(self.nodes,0,1)
        connect_nodes_bi(self.nodes,1,2)
        connect_nodes_bi(self.nodes,0,2)
        self.sync_all([self.nodes[0:3]])

    def check_fee_amount(self, curr_balance, balance_with_fee, fee_per_byte, tx_size):
        """Return curr_balance after asserting the fee was in range"""
        fee = balance_with_fee - curr_balance
        fee2 = round(tx_size * fee_per_byte / 1000, 8)
        self.log.info("current: %s, withfee: %s, perByte: %s, size: %s, fee: %s" % (str(curr_balance), str(balance_with_fee), str(fee_per_byte), str(tx_size), str(fee2)))
        assert_fee_amount(fee, tx_size, fee_per_byte * 1000)
        return curr_balance

    def get_vsize(self, txn):
        return self.nodes[0].decoderawtransaction(txn)['size']

    def run_test(self):
        # Check that there's no UTXO on none of the nodes
        assert_equal(len(self.nodes[0].listunspent()), 0)
        assert_equal(len(self.nodes[1].listunspent()), 0)
        assert_equal(len(self.nodes[2].listunspent()), 0)

        self.log.info("Mining blocks...")

        self.nodes[0].generate(1)

        walletinfo = self.nodes[0].getwalletinfo()
        assert_equal(walletinfo['immature_balance'], 250)
        assert_equal(walletinfo['balance'], 0)

        self.sync_all([self.nodes[0:3]])
        self.nodes[1].generate(101)
        self.sync_all([self.nodes[0:3]])

        assert_equal(self.nodes[0].getbalance(), 250)
        assert_equal(self.nodes[1].getbalance(), 250)
        assert_equal(self.nodes[2].getbalance(), 0)

        # Check that only first and second nodes have UTXOs
        utxos = self.nodes[0].listunspent()
        assert_equal(len(utxos), 1)
        assert_equal(len(self.nodes[1].listunspent()), 1)
        assert_equal(len(self.nodes[2].listunspent()), 0)

        # Send 21 BTC from 0 to 2 using sendtoaddress call.
        # Second transaction will be child of first, and will require a fee
        self.nodes[0].sendtoaddress(self.nodes[2].getnewaddress(), 21)
        #self.nodes[0].sendtoaddress(self.nodes[2].getnewaddress(), 10)

        walletinfo = self.nodes[0].getwalletinfo()
        assert_equal(walletinfo['immature_balance'], 0)

        # Have node0 mine a block, thus it will collect its own fee.
        self.nodes[0].generate(1)
        self.sync_all([self.nodes[0:3]])

        # Exercise locking of unspent outputs
        unspent_0 = self.nodes[2].listunspent()[0]
        unspent_0 = {"txid": unspent_0["txid"], "vout": unspent_0["vout"]}
        self.nodes[2].lockunspent(False, [unspent_0])
        assert_raises_rpc_error(-4, "Insufficient funds", self.nodes[2].sendtoaddress, self.nodes[2].getnewaddress(), 20)
        assert_equal([unspent_0], self.nodes[2].listlockunspent())
        self.nodes[2].lockunspent(True, [unspent_0])
        assert_equal(len(self.nodes[2].listlockunspent()), 0)

        # Have node1 generate 100 blocks (so node0 can recover the fee)
        self.nodes[1].generate(100)
        self.sync_all([self.nodes[0:3]])

        # node0 should end up with 100 btc in block rewards plus fees, but
        # minus the 21 plus fees sent to node2
        assert_equal(self.nodes[0].getbalance(), 500-21)
        assert_equal(self.nodes[2].getbalance(), 21)

        # Node0 should have two unspent outputs.
        # Create a couple of transactions to send them to node2, submit them through
        # node1, and make sure both node0 and node2 pick them up properly:
        node0utxos = self.nodes[0].listunspent(1)
        assert_equal(len(node0utxos), 2)

        # create both transactions
        txns_to_send = []
        for utxo in node0utxos:
            inputs = []
            outputs = {}
            inputs.append({ "txid" : utxo["txid"], "vout" : utxo["vout"]})
            outputs[self.nodes[2].getnewaddress("from1")] = float(utxo["amount"])
            raw_tx = self.nodes[0].createrawtransaction(inputs, outputs)
            txns_to_send.append(self.nodes[0].signrawtransaction(raw_tx))

        # Have node 1 (miner) send the transactions
        self.nodes[1].sendrawtransaction(txns_to_send[0]["hex"], True)
        self.nodes[1].sendrawtransaction(txns_to_send[1]["hex"], True)

        # Have node1 mine a block to confirm transactions:
        self.nodes[1].generate(1)
        self.sync_all([self.nodes[0:3]])

        assert_equal(self.nodes[0].getbalance(), 0)
        assert_equal(self.nodes[2].getbalance(), 500)
        assert_equal(self.nodes[2].getbalance("from1"), 500-21)

        # Send 10 BTC normal
        address = self.nodes[0].getnewaddress("test")
        fee_per_byte = Decimal('0.001') / 1000
        self.nodes[2].settxfee(float(fee_per_byte * 1000))
        txid = self.nodes[2].sendtoaddress(address, 10, "", "")
        fee = self.nodes[2].gettransaction(txid)["fee"]
        self.nodes[2].generate(1)
        self.sync_all([self.nodes[0:3]])
        node_2_bal = self.nodes[2].getbalance()
        #node_2_bal = self.check_fee_amount(balance, Decimal(balance - fee), fee_per_byte, self.get_vsize(self.nodes[2].getrawtransaction(txid)))
        assert_equal(self.nodes[0].getbalance(), Decimal('10'))

        # Send 10 BTC with subtract fee from amount
        txid = self.nodes[2].sendtoaddress(address, 10, "", "")
        self.nodes[2].generate(1)
        self.sync_all([self.nodes[0:3]])
        node_2_bal -= Decimal('10')
        assert_equal(self.nodes[2].getbalance() - fee, node_2_bal)
        node_0_bal = self.nodes[0].getbalance()
        assert_equal(node_0_bal, Decimal('20'))

        # Sendmany 10 BTC
        txid = self.nodes[2].sendmany('from1', {address: 10}, 0, "")
        self.nodes[2].generate(1)
        self.sync_all([self.nodes[0:3]])
        node_0_bal += Decimal('10')
        node_2_bal -= Decimal('10')
        #node_2_bal = self.check_fee_amount(self.nodes[2].getbalance(), node_2_bal - Decimal('10'), fee_per_byte, self.get_vsize(self.nodes[2].getrawtransaction(txid)))
        assert_equal(self.nodes[0].getbalance(), node_0_bal)

        # Sendmany 10 BTC with subtract fee from amount
        txid = self.nodes[2].sendmany('from1', {address: 10}, 0, "")
        self.nodes[2].generate(1)
        self.sync_all([self.nodes[0:3]])
        node_2_bal -= Decimal('10')
        assert_equal(self.nodes[2].getbalance(), node_2_bal + (fee * 3))
        #node_0_bal = self.check_fee_amount(self.nodes[0].getbalance(), node_0_bal + Decimal('10'), fee_per_byte, self.get_vsize(self.nodes[2].getrawtransaction(txid)))

        # Test ResendWalletTransactions:
        # Create a couple of transactions, then start up a fourth
        # node (nodes[3]) and ask nodes[0] to rebroadcast.
        # EXPECT: nodes[3] should have those transactions in its mempool.
        txid1 = self.nodes[0].sendtoaddress(self.nodes[1].getnewaddress(), 1)
        txid2 = self.nodes[1].sendtoaddress(self.nodes[0].getnewaddress(), 1)
        sync_mempools(self.nodes[0:2])

        self.start_node(3)
        connect_nodes_bi(self.nodes, 0, 3)
        sync_blocks(self.nodes)

        #relayed = self.nodes[0].resendwallettransactions()
        #assert_equal(set(relayed), {txid1, txid2})
        #sync_mempools(self.nodes)

        #assert(txid1 in self.nodes[3].getrawmempool())

        # Exercise balance rpcs
        assert_equal(self.nodes[0].getwalletinfo()["unconfirmed_balance"], 1)
        assert_equal(self.nodes[0].getunconfirmedbalance(), 1)

        #check if we can list zero value tx as available coins
        #1. create rawtx
        #2. hex-changed one output to 0.0
        #3. sign and send
        #4. check if recipient (node0) can list the zero value tx
        usp = self.nodes[1].listunspent()
        inputs = [{"txid":usp[0]['txid'], "vout":usp[0]['vout']}]
        outputs = {self.nodes[1].getnewaddress(): 49.998, self.nodes[0].getnewaddress(): 11.11}

        rawTx = self.nodes[1].createrawtransaction(inputs, outputs).replace("c0833842", "00000000") #replace 11.11 with 0.0 (int32)
        decRawTx = self.nodes[1].decoderawtransaction(rawTx)
        signedRawTx = self.nodes[1].signrawtransaction(rawTx)
        decRawTx = self.nodes[1].decoderawtransaction(signedRawTx['hex'])
        zeroValueTxid= decRawTx['txid']
        assert_raises_rpc_error(-25, "", self.nodes[1].sendrawtransaction, signedRawTx['hex'])

        self.sync_all([self.nodes[0:3]])
        self.nodes[1].generate(1) #mine a block
        self.sync_all([self.nodes[0:3]])

        #unspentTxs = self.nodes[0].listunspent() #zero value tx must be in listunspents output
        #found = False
        #for uTx in unspentTxs:
        #    if uTx['txid'] == zeroValueTxid:
        #        found = True
        #        assert_equal(uTx['amount'], Decimal('0'))
        #assert(found)

        #do some -walletbroadcast tests
        self.stop_nodes()
        self.start_node(0, ["-walletbroadcast=0"])
        self.start_node(1, ["-walletbroadcast=0"])
        self.start_node(2, ["-walletbroadcast=0"])
        connect_nodes_bi(self.nodes,0,1)
        connect_nodes_bi(self.nodes,1,2)
        connect_nodes_bi(self.nodes,0,2)
        self.sync_all([self.nodes[0:3]])

        txIdNotBroadcasted  = self.nodes[0].sendtoaddress(self.nodes[2].getnewaddress(), 2)
        txObjNotBroadcasted = self.nodes[0].gettransaction(txIdNotBroadcasted)
        self.nodes[1].generate(1) #mine a block, tx should not be in there
        self.sync_all([self.nodes[0:3]])
        assert_equal(self.nodes[2].getbalance(), node_2_bal + (fee * 3)) #should not be changed because tx was not broadcasted

        #now broadcast from another node, mine a block, sync, and check the balance
        self.nodes[1].sendrawtransaction(txObjNotBroadcasted['hex'])
        self.nodes[1].generate(1)
        self.sync_all([self.nodes[0:3]])
        node_2_bal += 2
        txObjNotBroadcasted = self.nodes[0].gettransaction(txIdNotBroadcasted)
        assert_equal(self.nodes[2].getbalance(), node_2_bal + (fee * 3))

        #create another tx
        txIdNotBroadcasted  = self.nodes[0].sendtoaddress(self.nodes[2].getnewaddress(), 2)

        #restart the nodes with -walletbroadcast=1
        self.stop_nodes()
        self.start_node(0)
        self.start_node(1)
        self.start_node(2)
        connect_nodes_bi(self.nodes,0,1)
        connect_nodes_bi(self.nodes,1,2)
        connect_nodes_bi(self.nodes,0,2)
        sync_blocks(self.nodes[0:3])

        self.nodes[0].generate(1)
        sync_blocks(self.nodes[0:3])
        node_2_bal += 2

        #tx should be added to balance because after restarting the nodes tx should be broadcastet
        assert_equal(self.nodes[2].getbalance(), node_2_bal + (fee * 3))

        # This will raise an exception since generate does not accept a string
        assert_raises_rpc_error(-1, "not an integer", self.nodes[0].generate, "2")

        # Import address and private key to check correct behavior of spendable unspents
        # 1. Send some coins to generate new UTXO
        address_to_import = self.nodes[2].getnewaddress()
        txid = self.nodes[0].sendtoaddress(address_to_import, 1)
        self.nodes[0].generate(1)
        self.sync_all([self.nodes[0:3]])

        # 2. Import address from node2 to node1
        self.nodes[1].importaddress(address_to_import)

        # 3. Validate that the imported address is watch-only on node1
        assert(self.nodes[1].validateaddress(address_to_import)["iswatchonly"])

        # 4. Check that the unspents after import are not spendable
        listunspent = self.nodes[1].listunspent(1, 9999999, [], 3)
        assert_array_result(listunspent,
                           {"address": address_to_import},
                           {"spendable": False})

        # 5. Import private key of the previously imported address on node1
        priv_key = self.nodes[2].dumpprivkey(address_to_import)
        self.nodes[1].importprivkey(priv_key)

        # 6. Check that the unspents are now spendable on node1
        assert_array_result(self.nodes[1].listunspent(),
                           {"address": address_to_import},
                           {"spendable": True})

        #check if wallet or blochchain maintenance changes the balance
        self.sync_all([self.nodes[0:3]])
        blocks = self.nodes[0].generate(2)
        self.sync_all([self.nodes[0:3]])
        balance_nodes = [self.nodes[i].getbalance() for i in range(3)]
        block_count = self.nodes[0].getblockcount()

        maintenance = [
            '-rescan',
            '-reindex',
            '-zapwallettxes=1',
            '-zapwallettxes=2',
            #'-salvagewallet',
        ]
        chainlimit = 6
        for m in maintenance:
            self.log.info("check " + m)
            self.stop_nodes()
            # set lower ancestor limit for later
            self.start_node(0, [m, "-limitancestorcount="+str(chainlimit)])
            self.start_node(1, [m, "-limitancestorcount="+str(chainlimit)])
            self.start_node(2, [m, "-limitancestorcount="+str(chainlimit)])
            if m == '-reindex':
                # reindex will leave rpc warm up "early"; Wait for it to finish
                wait_until(lambda: [block_count] * 3 == [self.nodes[i].getblockcount() for i in range(3)])
            assert_equal(balance_nodes, [self.nodes[i].getbalance() for i in range(3)])

        # Exercise listsinceblock with the last two blocks
        coinbase_tx_1 = self.nodes[0].listsinceblock(blocks[0])
        assert_equal(coinbase_tx_1["lastblock"], blocks[1])
        assert_equal(len(coinbase_tx_1["transactions"]), 1)
        assert_equal(coinbase_tx_1["transactions"][0]["blockhash"], blocks[1])
        assert_equal(len(self.nodes[0].listsinceblock(blocks[1])["transactions"]), 0)

if __name__ == '__main__':
    WalletTest().main()
