#!/usr/bin/env python3
# Copyright (c) 2021 The Wagerr  developers
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
import copy

WGR_WALLET_ORACLE = { "addr": "TXuoB9DNEuZx1RCfKw3Hsv7jNUHTt4sVG1", "key": "TBwvXbNNUiq7tDkR2EXiCbPxEJRTxA1i6euNyAE9Ag753w36c1FZ" }
WGR_WALLET_EVENT = { "addr": "TFvZVYGdrxxNunQLzSnRSC58BSRA7si6zu", "key": "TCDjD2i4e32kx2Fc87bDJKGBedEyG7oZPaZfp7E1PQG29YnvArQ8" }
WGR_WALLET_DEV = { "addr": "TLuTVND9QbZURHmtuqD5ESECrGuB9jLZTs", "key": "TFCrxaUt3EjHzMGKXeBqA7sfy3iaeihg5yZPSrf9KEyy4PHUMWVe" }
WGR_WALLET_OMNO = { "addr": "THofaueWReDjeZQZEECiySqV9GP4byP3qr", "key": "TDJnwRkSk8JiopQrB484Ny9gMcL1x7bQUUFFFNwJZmmWA7U79uRk" }

pl_sport_names = ["PeerlessSport"]
field_sport_names = ["FieldSport"]
round_names = ["round0"]
tournament_names = ["Tournament1", "Tournament2"]
team_names = ["Team1", "Team2", "Team3", "Team4"]
contender_names = ["cont1", "cont2", "cont3", "cont4", "cont5", "cont6", "cont7", "cont8", "cont9",
]

outcome_home_win = 1
outcome_away_win = 2
outcome_draw = 3
outcome_spread_home = 4
outcome_spread_away = 5
outcome_total_over = 6
outcome_total_under = 7

# Field event groups
other_group = 1
animal_racing_group = 2

# Field event market type
all_markets = 1
outright_only = 2

# Field bet market types
market_outright = 1
market_place    = 2
market_show     = 3

# Contender result types
DNF    = 0
place1 = 1
place2 = 2
place3 = 3
DNR    = 101

def make_field_odds(probability_percent):
    if probability_percent == 0:
        return 0
    return int((1 / (probability_percent / 100)) * ODDS_DIVISOR)

class HybridBettingTest(BitcoinTestFramework):
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
        # { event_id : { contender_id : odds, ... } }
        self.mrg_in_percent = 11500 # 115.00%

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
        for i in range(self.num_nodes):
            for j in range(self.num_nodes):
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
        self.log.info("node4Addr = " + str(node4Addr))
        self.log.info("player1 addr = " + str(self.players[0]))
        self.log.info("player2 addr = " + str(self.players[1]))

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

        for i in range(30):
            self.nodes[0].sendtoaddress(WGR_WALLET_ORACLE['addr'], 2000)
            self.nodes[0].sendtoaddress(WGR_WALLET_EVENT['addr'], 2000)
            self.nodes[0].sendtoaddress(self.players[0], 2000)
            self.nodes[0].sendtoaddress(self.players[1], 2000)

        for i in range(30):
            self.nodes[0].sendtoaddress(node4Addr, 2000)

        self.nodes[0].generate(51)
        self.sync_all()

        for n in range(self.num_nodes):
            assert_equal( self.nodes[n].getblockcount(), 300)

        assert_equal(self.nodes[1].getbalance(), 120000) # oracle
        assert_equal(self.nodes[2].getbalance(), 60000) # player1
        assert_equal(self.nodes[3].getbalance(), 60000) # player2
        assert_equal(self.nodes[4].getbalance(), 60000) # revert node

        self.log.info("Minting Success")

    def check_mapping(self):
        self.log.info("Check Mapping...")

        self.nodes[0].generate(1)
        sync_blocks(self.nodes)

        assert_raises_rpc_error(-1, "No mapping exist for the mapping index you provided.", self.nodes[0].getmappingid, "", "")
        assert_raises_rpc_error(-1, "No mapping exist for the mapping index you provided.", self.nodes[0].getmappingname, "abc123", 0)

        # add peerless sports to mapping
        for id in range(len(pl_sport_names)):
            mapping_opcode = make_mapping(SPORT_MAPPING, id, pl_sport_names[id])
            post_opcode(self.nodes[1], mapping_opcode, WGR_WALLET_ORACLE['addr'])
        self.nodes[0].generate(1)
        sync_blocks(self.nodes)

        # add field sports to mapping
        for id in range(len(field_sport_names)):
            mapping_opcode = make_mapping(INDIVIDUAL_SPORT_MAPPING, id, field_sport_names[id])
            post_opcode(self.nodes[1], mapping_opcode, WGR_WALLET_ORACLE['addr'])
        self.nodes[0].generate(1)
        sync_blocks(self.nodes)

        # add tournaments to mapping
        for id in range(len(tournament_names)):
            mapping_opcode = make_mapping(TOURNAMENT_MAPPING, id, tournament_names[id])
            post_opcode(self.nodes[1], mapping_opcode, WGR_WALLET_ORACLE['addr'])
        self.nodes[0].generate(1)
        sync_blocks(self.nodes)

        # add rounds to mapping
        for id in range(len(round_names)):
            mapping_opcode = make_mapping(ROUND_MAPPING, id, round_names[id])
            post_opcode(self.nodes[1], mapping_opcode, WGR_WALLET_ORACLE['addr'])
        self.nodes[0].generate(1)
        sync_blocks(self.nodes)

        # add contenders to mapping
        for id in range(len(contender_names)):
            mapping_opcode = make_mapping(CONTENDER_MAPPING, id, contender_names[id])
            post_opcode(self.nodes[1], mapping_opcode, WGR_WALLET_ORACLE['addr'])
        self.nodes[0].generate(1)
        sync_blocks(self.nodes)

        # add teams to mapping
        for id in range(len(team_names)):
            mapping_opcode = make_mapping(TEAM_MAPPING, id, team_names[id])
            post_opcode(self.nodes[1], mapping_opcode, WGR_WALLET_ORACLE['addr'])

        self.nodes[0].generate(1)
        sync_blocks(self.nodes)

        self.log.info("Mapping Success")

    def check_hybrid_parlay_bets(self):

        self.start_time = int(time.time() + 60 * 60)
        # make pl event 1
        plEvent = make_event(0, # Event ID
                            self.start_time, # start time = current + hour
                            pl_sport_names.index("PeerlessSport"), # Sport ID
                            tournament_names.index("Tournament1"), # Tournament ID
                            round_names.index("round0"), # Round ID
                            team_names.index("Team1"), # Home Team
                            team_names.index("Team2"), # Away Team
                            15000, # home odds
                            18000, # away odds
                            13000) # draw odds
        post_opcode(self.nodes[1], plEvent, WGR_WALLET_EVENT['addr'])

        # make pl event 2
        plEvent = make_event(1, # Event ID
                            self.start_time, # start time = current + hour
                            pl_sport_names.index("PeerlessSport"), # Sport ID
                            tournament_names.index("Tournament1"), # Tournament ID
                            round_names.index("round0"), # Round ID
                            team_names.index("Team3"), # Home Team
                            team_names.index("Team4"), # Away Team
                            10000, # home odds
                            20000, # away odds
                            18000) # draw odds
        post_opcode(self.nodes[1], plEvent, WGR_WALLET_EVENT['addr'])

        # make field event 1
        field_event_opcode = make_field_event(
            0,
            self.start_time,
            animal_racing_group,
            all_markets,
            field_sport_names.index("FieldSport"),
            tournament_names.index("Tournament1"),
            round_names.index("round0"),
            self.mrg_in_percent,
            {
                contender_names.index("cont1"): make_field_odds(10),
                contender_names.index("cont2"): make_field_odds(11),
                contender_names.index("cont3"): make_field_odds(12),
                contender_names.index("cont4"): make_field_odds(13),
                contender_names.index("cont5"): make_field_odds(14),
                contender_names.index("cont6"): make_field_odds(20),
                contender_names.index("cont7"): make_field_odds(15),
                contender_names.index("cont8"): make_field_odds(5),
                contender_names.index("cont9"): make_field_odds(0)
            }
        )
        post_opcode(self.nodes[1], field_event_opcode, WGR_WALLET_EVENT['addr'])

        # make field event 2
        field_event_opcode = make_field_event(
            1,
            self.start_time,
            animal_racing_group,
            all_markets,
            field_sport_names.index("FieldSport"),
            tournament_names.index("Tournament2"),
            round_names.index("round0"),
            self.mrg_in_percent,
            {
                contender_names.index("cont1"): make_field_odds(15),
                contender_names.index("cont2"): make_field_odds(5),
                contender_names.index("cont3"): make_field_odds(13),
                contender_names.index("cont4"): make_field_odds(16),
                contender_names.index("cont5"): make_field_odds(5),
                contender_names.index("cont6"): make_field_odds(3),
                contender_names.index("cont7"): make_field_odds(4),
                contender_names.index("cont8"): make_field_odds(0),
                contender_names.index("cont9"): make_field_odds(0)
            }
        )
        post_opcode(self.nodes[1], field_event_opcode, WGR_WALLET_EVENT['addr'])

        self.nodes[1].generate(1)
        sync_blocks(self.nodes)

        player1_legs = [
            {
                "legType": "peerless",
                "legObj": {
                    "eventId": 0, # pl event 0
                    "outcome": outcome_home_win # Team1 win
                }
            },
            {
                "legType": "peerless",
                "legObj": {
                    "eventId": 1, # pl event 1
                    "outcome": outcome_away_win # Team4 win
                }
            },
            {
                "legType": "field",
                "legObj": {
                    "eventId": 0, # field event 0
                    "marketType": market_outright, # contender1 will take 1st place
                    "contenderId": contender_names.index("cont1")
                }
            },
            {
                "legType": "field",
                "legObj": {
                    "eventId": 1, # field event 0
                    "marketType": market_place, # contender2 will take 1st or 2nd place
                    "contenderId": contender_names.index("cont2")
                }
            }
        ]

        # try to place hybrid bet with one leg
        assert_raises_rpc_error(-31, "Error: Incorrect legs count.",
            self.nodes[2].placehybridparlaybet, player1_legs[:1], 100)

        # invalid amount
        assert_raises_rpc_error(-31, "Incorrect bet amount. Please ensure your bet is between 25 - 10000 WGR inclusive.",
            self.nodes[2].placehybridparlaybet, player1_legs, 24)
        assert_raises_rpc_error(-31, "Incorrect bet amount. Please ensure your bet is between 25 - 10000 WGR inclusive.",
            self.nodes[2].placehybridparlaybet, player1_legs, 10001)

        # invalid pl event
        failed_legs = copy.deepcopy(player1_legs)
        failed_legs[0]['legObj']['eventId'] = 101
        assert_raises_rpc_error(-31, "Error: there is no such Event: {}".format(101),
            self.nodes[2].placehybridparlaybet, failed_legs, 30)
        # invalid field event
        failed_legs = copy.deepcopy(player1_legs)
        failed_legs[3]['legObj']['eventId'] = 201
        assert_raises_rpc_error(-31, "Error: there is no such FieldEvent: {}".format(201),
            self.nodes[2].placehybridparlaybet, failed_legs, 30)

        revert_chain_height = self.nodes[4].getblockcount()
        self.stop_node(4)

        player1_bet_tx = self.nodes[2].placehybridparlaybet(player1_legs, 100)

        player2_legs = copy.deepcopy(player1_legs)
        # make for player2 3rd leg as loss
        # cont2 will not take 1st place
        player2_legs[2]['legObj']['contenderId'] = contender_names.index("cont2")
        player2_bet_tx = self.nodes[3].placehybridparlaybet(player2_legs, 200)

        self.nodes[1].generate(1)
        sync_blocks(self.nodes[0:4])

        player1_bet = self.nodes[0].getbetbytxid(player1_bet_tx)
        player2_bet = self.nodes[0].getbetbytxid(player2_bet_tx)

        # pl event 0, home team win
        result_opcode = make_result(0, STANDARD_RESULT, 2, 0)
        post_opcode(self.nodes[1], result_opcode, WGR_WALLET_EVENT['addr'])
        # pl event 1, away team win
        result_opcode = make_result(1, STANDARD_RESULT, 0, 1)
        post_opcode(self.nodes[1], result_opcode, WGR_WALLET_EVENT['addr'])
        # field event 0
        field_result_opcode = make_field_result(0, STANDARD_RESULT, {
            contender_names.index("cont1") : place1,
            contender_names.index("cont2") : place2,
            contender_names.index("cont3") : place3
        })
        # field event 1
        post_opcode(self.nodes[1], field_result_opcode, WGR_WALLET_EVENT['addr'])
        field_result_opcode = make_field_result(1, STANDARD_RESULT, {
            contender_names.index("cont1") : place1,
            contender_names.index("cont2") : place2,
            contender_names.index("cont3") : place3
        })
        post_opcode(self.nodes[1], field_result_opcode, WGR_WALLET_EVENT['addr'])

        self.nodes[1].generate(1)
        sync_blocks(self.nodes[0:4])

        player1_balance_before = self.nodes[2].getbalance()
        player2_balance_before = self.nodes[3].getbalance()

        # generate payouts
        self.nodes[1].generate(1)
        sync_blocks(self.nodes[0:4])

        player1_balance_after = self.nodes[2].getbalance()
        player2_balance_after = self.nodes[3].getbalance()

        print("player1_balance_before: ", player1_balance_before)
        print("player1_balance_after: ", player1_balance_after)

        player1_payout = player1_balance_after - player1_balance_before
        print("player1_bet payout: ", player1_payout)

        print("player2_balance_before: ", player2_balance_before)
        print("player2_balance_after: ", player2_balance_after)
        player2_payout = player2_balance_after - player2_balance_before
        print("player2_bet payout: ", player2_payout)

        player1_bet = self.nodes[0].getbetbytxid(player1_bet_tx)
        player2_bet = self.nodes[0].getbetbytxid(player2_bet_tx)

        assert_equal(player1_bet[0]['payout'], Decimal(player1_payout))
        assert_equal(Decimal(player2_payout), Decimal(0))
        assert_equal(player2_bet[0]['payout'], Decimal(player2_payout))


        self.log.info("Revering...")
        current_chain_height = self.nodes[0].getblockcount()
        self.nodes[4].rpchost = self.get_local_peer(4, True)
        self.nodes[4].start()
        self.nodes[4].wait_for_rpc_connection()
        self.log.info("Generate blocks...")
        blocks = current_chain_height - revert_chain_height + 1
        for i in range(blocks):
            self.nodes[4].generate(1)
            time.sleep(0.5)

        self.log.info("Connect and sync nodes...")
        self.connect_and_sync_blocks()

        assert_equal(player1_balance_before, self.nodes[2].getbalance())
        assert_equal(player2_balance_before, self.nodes[3].getbalance())

        assert_raises_rpc_error(-8, "Can't find bet's transaction id: {} in chain.".format(player1_bet_tx),
            self.nodes[0].getbetbytxid, player1_bet_tx)
        assert_raises_rpc_error(-8, "Can't find bet's transaction id: {} in chain.".format(player2_bet_tx),
            self.nodes[0].getbetbytxid, player2_bet_tx)



    def run_test(self):
        self.check_minting()
        # Chain height = 300 after minting -> v4 protocol active
        self.check_mapping()
        self.check_hybrid_parlay_bets()

if __name__ == '__main__':
    HybridBettingTest().main()
    # check_calculate_odds()