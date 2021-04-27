#!/usr/bin/env python3
# Copyright (c) 2014-2017 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

from test_framework.betting_opcode import *
from test_framework.authproxy import JSONRPCException
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import wait_until, rpc_port, assert_equal, assert_raises_rpc_error, sync_blocks
from distutils.dir_util import copy_tree, remove_tree
from decimal import *
import pprint
import time
import os
import ctypes

WGR_WALLET_ORACLE = { "addr": "TXuoB9DNEuZx1RCfKw3Hsv7jNUHTt4sVG1", "key": "TBwvXbNNUiq7tDkR2EXiCbPxEJRTxA1i6euNyAE9Ag753w36c1FZ" }
WGR_WALLET_EVENT = { "addr": "TFvZVYGdrxxNunQLzSnRSC58BSRA7si6zu", "key": "TCDjD2i4e32kx2Fc87bDJKGBedEyG7oZPaZfp7E1PQG29YnvArQ8" }
WGR_WALLET_DEV = { "addr": "TLuTVND9QbZURHmtuqD5ESECrGuB9jLZTs", "key": "TFCrxaUt3EjHzMGKXeBqA7sfy3iaeihg5yZPSrf9KEyy4PHUMWVe" }
WGR_WALLET_OMNO = { "addr": "THofaueWReDjeZQZEECiySqV9GP4byP3qr", "key": "TDJnwRkSk8JiopQrB484Ny9gMcL1x7bQUUFFFNwJZmmWA7U79uRk" }

sport_names = ["Sport1", "Horse racing", "F1 racing"]
round_names = ["round0", "round1",]
tournament_names = ["Tournament1", "The BMW stakes", "F1 Cup"]
contender_names = ["cont1", "cont2", "horse1", "horse2", "horse3", "horse4", "horse5", "Alexander Albon", "Daniil Kvyat", "Pierre Gasly", "Romain Grosjean", "Antonio Maria Giovinazzi"]

other_group = 1
animal_racing_group = 2

ODDS_DIVISOR = 10000
BETX_PERMILLE = 60

def check_bet_payouts_info(listbets, listpayoutsinfo):
    for bet in listbets:
        info_found = False
        for info in listpayoutsinfo:
            info_type = info['payoutInfo']['payoutType']
            if info_type == 'Betting Payout' or info_type == 'Betting Refund':
                if info['payoutInfo']['betBlockHeight'] == bet['betBlockHeight']:
                    if info['payoutInfo']['betTxHash'] == bet['betTxHash']:
                        if info['payoutInfo']['betTxOut'] == bet['betTxOut']:
                            info_found = True
        assert(info_found)

class BettingTest(BitcoinTestFramework):
    def get_node_setting(self, node_index, setting_name):
        with open(os.path.join(self.nodes[node_index].datadir, "wagerr.conf"), 'r', encoding='utf8') as f:
            for line in f:
                if line.startswith(setting_name + "="):
                    return line.split("=")[1].strip("\n")
        return None

    def get_local_peer(self, node_index, is_rpc=False):
        port = self.get_node_setting(node_index, "rpcport" if is_rpc else "port")
        return "127.0.0.1:" + str(rpc_port(node_index) if port is None else port)

    def set_test_params(self):
        # 0 - main node
        # 1 - oracle
        # 2 - player1
        # 3 - player2
        # 4 - revert testing
        self.num_nodes = 5
        self.extra_args = [[] for n in range(self.num_nodes)]
        self.setup_clean_chain = True
        self.players = []

    def connect_network(self):
        for i in range(len(self.nodes)):
            for j in range(len(self.nodes)):
                if i == j:
                    continue
                self.nodes[i].addnode(self.get_local_peer(j), "onetry")
                wait_until(lambda:  all(peer['version'] != 0 for peer in self.nodes[i].getpeerinfo()))
        self.sync_all()
        for n in range(self.num_nodes):
            idx_l = n
            idx_r = n + 1 if n + 1 < self.num_nodes else 0
            assert_equal(self.nodes[idx_l].getblockcount(), self.nodes[idx_r].getblockcount())

    def connect_and_sync_blocks(self):
        for i in range(len(self.nodes)):
            for j in range(len(self.nodes)):
                if i == j:
                    continue
                self.nodes[i].addnode(self.get_local_peer(j), "onetry")
                wait_until(lambda:  all(peer['version'] != 0 for peer in self.nodes[i].getpeerinfo()))
        sync_blocks(self.nodes)

    def setup_network(self):
        self.log.info("Setup Network")
        self.setup_nodes()
        self.connect_network()

    def check_minting(self, block_count=250):
        self.log.info("Check Minting...")

        self.nodes[1].importprivkey(WGR_WALLET_ORACLE['key'])
        self.nodes[1].importprivkey(WGR_WALLET_EVENT['key'])
        self.nodes[1].importprivkey(WGR_WALLET_DEV['key'])
        self.nodes[1].importprivkey(WGR_WALLET_OMNO['key'])

        self.players.append(self.nodes[2].getnewaddress('Node2Addr'))
        self.players.append(self.nodes[3].getnewaddress('Node3Addr'))
        node4Addr = self.nodes[4].getnewaddress('Node4Addr')

        for i in range(block_count - 1):
            blocks = self.nodes[0].generate(1)
            blockinfo = self.nodes[0].getblock(blocks[0])
            # get coinbase tx
            rawTx = self.nodes[0].getrawtransaction(blockinfo['tx'][0])
            decodedTx = self.nodes[0].decoderawtransaction(rawTx)
            address = decodedTx['vout'][0]['scriptPubKey']['addresses'][0]
            if (i > 0):
                # minting must process to sigle address
                assert_equal(address, prevAddr)
            prevAddr = address

        for i in range(20):
            self.nodes[0].sendtoaddress(WGR_WALLET_ORACLE['addr'], 2000)
            self.nodes[0].sendtoaddress(WGR_WALLET_EVENT['addr'], 2000)
            self.nodes[0].sendtoaddress(self.players[0], 2000)
            self.nodes[0].sendtoaddress(self.players[1], 2000)
            self.nodes[0].sendtoaddress(node4Addr, 2000)

        self.nodes[0].generate(51)
        self.sync_all()

        for n in range(self.num_nodes):
            assert_equal( self.nodes[n].getblockcount(), 300)

        assert_equal(self.nodes[1].getbalance(), 80000) # oracle
        assert_equal(self.nodes[2].getbalance(), 40000) # player1
        assert_equal(self.nodes[3].getbalance(), 40000) # player2
        assert_equal(self.nodes[4].getbalance(), 40000) # revert node

        self.log.info("Minting Success")

    def check_mapping(self):
        self.log.info("Check Mapping...")

        self.nodes[0].generate(1)
        sync_blocks(self.nodes)

        assert_raises_rpc_error(-1, "No mapping exist for the mapping index you provided.", self.nodes[0].getmappingid, "", "")
        assert_raises_rpc_error(-1, "No mapping exist for the mapping index you provided.", self.nodes[0].getmappingname, "abc123", 0)

        for id in range(len(sport_names)):
            mapping_opcode = make_mapping(INDIVIDUAL_SPORT_MAPPING, id, sport_names[id])
            post_opcode(self.nodes[1], mapping_opcode, WGR_WALLET_ORACLE['addr'])
        self.nodes[0].generate(1)
        sync_blocks(self.nodes)

        for id in range(len(round_names)):
            mapping_opcode = make_mapping(ROUND_MAPPING, id, round_names[id])
            post_opcode(self.nodes[1], mapping_opcode, WGR_WALLET_ORACLE['addr'])
        self.nodes[0].generate(1)
        sync_blocks(self.nodes)

        for id in range(len(contender_names)):
            mapping_opcode = make_mapping(CONTENDER_MAPPING, id, contender_names[id])
            post_opcode(self.nodes[1], mapping_opcode, WGR_WALLET_ORACLE['addr'])
        self.nodes[0].generate(1)
        sync_blocks(self.nodes)

        for id in range(len(tournament_names)):
            mapping_opcode = make_mapping(TOURNAMENT_MAPPING, id, tournament_names[id])
            post_opcode(self.nodes[1], mapping_opcode, WGR_WALLET_ORACLE['addr'])
        self.nodes[0].generate(1)
        sync_blocks(self.nodes)

        for node in self.nodes:
            # Check sports mapping
            for id in range(len(sport_names)):
                mapping = node.getmappingname("individualSports", id)[0]
                assert_equal(mapping['exists'], True)
                assert_equal(mapping['mapping-name'], sport_names[id])
                assert_equal(mapping['mapping-type'], "individualSports")
                assert_equal(mapping['mapping-index'], id)
                mappingid = node.getmappingid("individualSports", sport_names[id])[0]
                assert_equal(mappingid['exists'], True)
                assert_equal(mappingid['mapping-index'], "individualSports")
                assert_equal(mappingid['mapping-id'], id)

            # Check rounds mapping
            for id in range(len(round_names)):
                mapping = node.getmappingname("rounds", id)[0]
                assert_equal(mapping['exists'], True)
                assert_equal(mapping['mapping-name'], round_names[id])
                assert_equal(mapping['mapping-type'], "rounds")
                assert_equal(mapping['mapping-index'], id)
                mappingid = node.getmappingid("rounds", round_names[id])[0]
                assert_equal(mappingid['exists'], True)
                assert_equal(mappingid['mapping-index'], "rounds")
                assert_equal(mappingid['mapping-id'], id)

            # Check teams mapping
            for id in range(len(contender_names)):
                mapping = node.getmappingname("contenders", id)[0]
                assert_equal(mapping['exists'], True)
                assert_equal(mapping['mapping-name'], contender_names[id])
                assert_equal(mapping['mapping-type'], "contenders")
                assert_equal(mapping['mapping-index'], id)
                mappingid = node.getmappingid("contenders", contender_names[id])[0]
                assert_equal(mappingid['exists'], True)
                assert_equal(mappingid['mapping-index'], "contenders")
                assert_equal(mappingid['mapping-id'], id)

            # Check tournaments mapping
            for id in range(len(tournament_names)):
                mapping = node.getmappingname("tournaments", id)[0]
                assert_equal(mapping['exists'], True)
                assert_equal(mapping['mapping-name'], tournament_names[id])
                assert_equal(mapping['mapping-type'], "tournaments")
                assert_equal(mapping['mapping-index'], id)
                mappingid = node.getmappingid("tournaments", tournament_names[id])[0]
                assert_equal(mappingid['exists'], True)
                assert_equal(mappingid['mapping-index'], "tournaments")
                assert_equal(mappingid['mapping-id'], id)

        self.log.info("Mapping Success")

    def check_event(self):
        self.log.info("Check Event creation...")
        start_time = int(time.time() + 60 * 60)

        # Bad case
        field_event_opcode = make_field_event(
            0,
            start_time,
            100, # bad group
            sport_names.index("Sport1"),
            tournament_names.index("Tournament1"),
            round_names.index("round0"),
            {
                contender_names.index("cont1") : 15000,
                contender_names.index("cont2") : 18000,
            }
        )
        assert_raises_rpc_error(-25, "",
            post_opcode, self.nodes[1], field_event_opcode, WGR_WALLET_ORACLE['addr'])

        # Test revert
        self.stop_node(4)

        field_event_opcode = make_field_event(
            0,
            start_time,
            other_group,
            sport_names.index("Sport1"),
            tournament_names.index("Tournament1"),
            round_names.index("round0"),
            {
                contender_names.index("cont1") : 15000,
                contender_names.index("cont2") : 18000,
            }
        )
        post_opcode(self.nodes[1], field_event_opcode, WGR_WALLET_EVENT['addr'])

        self.nodes[0].generate(1)
        sync_blocks(self.nodes[0:4])

        assert_raises_rpc_error(-25, "",
            post_opcode, self.nodes[1], field_event_opcode, WGR_WALLET_ORACLE['addr'])

        for node in self.nodes[0:4]:
            list_events = node.listfieldevents()
            assert_equal(len(list_events), 1)
            event_id = list_events[0]['event_id']
            assert_equal(list_events[0]['sport'], "Sport1")
            assert_equal(list_events[0]['tournament'], "Tournament1")
            assert_equal(list_events[0]['round'], "round0")
            assert_equal(len(list_events[0]['contenders']), 2)
            assert_equal(list_events[0]['contenders'][0]['name'], "cont1")
            assert_equal(list_events[0]['contenders'][0]['odds'], 15000)
            assert_equal(list_events[0]['contenders'][1]['name'], "cont2")
            assert_equal(list_events[0]['contenders'][1]['odds'], 18000)

        self.log.info("Revering...")
        self.nodes[4].rpchost = self.get_local_peer(4, True)
        self.nodes[4].start()
        self.nodes[4].wait_for_rpc_connection()
        self.log.info("Generate blocks...")
        for i in range(5):
            self.nodes[4].generate(1)

        assert_equal(len(self.nodes[4].listfieldevents()), 0)

        self.log.info("Connect and sync nodes...")
        self.connect_and_sync_blocks() # TODO: fix sync_mempools

        for node in self.nodes:
            assert_equal(len(node.listfieldevents()), 0)

        self.log.info("Event creation Success")

    def check_event_update(self):
        self.log.info("Check Event Update...")

        # TODO: update odds for event
        # TODO: add new conteder

        self.log.info("Event Patch Success")

    def check_field_bet(self):
        self.log.info("Check Field Bets...")
        start_time = int(time.time() + 60 * 60)
        
        global player1_total_bet
        global player2_total_bet

        player1_total_bet = 0
        player2_total_bet = 0

        # Create events for bet tests
        # { event_id : { contender_id : odds, ... } }
        self.odds_events = {}

        field_event_opcode = make_field_event(
            0,
            start_time,
            other_group,
            sport_names.index("Sport1"),
            tournament_names.index("Tournament1"),
            round_names.index("round0"),
            {
                contender_names.index("cont1") : 15000,
                contender_names.index("cont2") : 18000
            }
        )
        post_opcode(self.nodes[1], field_event_opcode, WGR_WALLET_EVENT['addr'])

        self.nodes[0].generate(1)
        sync_blocks(self.nodes[0:4])

        field_event_opcode = make_field_event(
            1,
            start_time,
            animal_racing_group,
            sport_names.index("Horse racing"),
            tournament_names.index("The BMW stakes"),
            round_names.index("round0"),
            {
                contender_names.index("horse1") : 15000,
                contender_names.index("horse2") : 18000,
                contender_names.index("horse3") : 14000,
                contender_names.index("horse4") : 13000,
                contender_names.index("horse5") : 19000
            }
        )
        post_opcode(self.nodes[1], field_event_opcode, WGR_WALLET_EVENT['addr'])

        self.nodes[0].generate(1)
        sync_blocks(self.nodes[0:4])

        # TODO: field bets tests

        self.log.info("Field Bets Success")

    def check_parlays_field_bet(self):
        self.log.info("Check Parlay Field Bets...")

        global player1_total_bet
        global player2_total_bet

        # TODO: check

        self.log.info("Parlay Field Bets Success")

    def check_timecut_refund(self):
        self.log.info("Check Timecut Refund...")

        global player1_total_bet
        global player2_total_bet

        # TODO: check

        self.log.info("Timecut Refund Success")

    def run_test(self):
        self.check_minting()
        # Chain height = 300 after minting -> v4 protocol active
        self.check_mapping()
        self.check_event()
        self.check_event_update()
        self.check_field_bet()
        self.check_parlays_field_bet()
        self.check_timecut_refund()

if __name__ == '__main__':
    BettingTest().main()
