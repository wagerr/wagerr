#!/usr/bin/env python3
# Copyright (c) 2014-2017 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test running bitcoind with -reindex and -reindex-chainstate options.

- Start a single node and generate 3 blocks.
- Stop the node and restart it with -reindex. Verify that the node has reindexed up to block 3.
- Stop the node and restart it with -reindex-chainstate. Verify that the node has reindexed up to block 3.
"""

from test_framework.betting_opcode import *
from test_framework.authproxy import JSONRPCException
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import wait_until, rpc_port, assert_equal, assert_raises_rpc_error
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

sport_names = ["Football", "MMA", "CSGO", "DOTA2", "Test Sport", "V2-V3 Sport", "ML Sport One", "Spread Sport"]
round_names = ["round1", "round2", "round3", "round4"]
tournament_names = ["UEFA Champions League", "UFC244", "PGL Major Krakow", "EPICENTER Major", "Test Tournament", "V2-V3 Tournament", "ML Tournament One", "Spread Tournament"]
team_names = ["Real Madrid", "Barcelona", "Jorge Masvidal", "Nate Diaz", "Astralis", "Gambit", "Virtus Pro", "Team Liquid", "Test Team1", "Test Team2","V2-V3 Team1", "V2-V3 Team2", "ML Team One", "ML Team Two", "Spread Team One", "Spread Team Two"]

outcome_home_win = 1
outcome_away_win = 2
outcome_draw = 3
outcome_spread_home = 4
outcome_spread_away = 5
outcome_total_over = 6
outcome_total_under = 7

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
    def get_cache_dir_name(self, node_index, block_count):
        return ".test-chain-{0}-{1}-.node{2}".format(self.num_nodes, block_count, node_index)

    def get_node_setting(self, node_index, setting_name):
        with open(os.path.join(self.nodes[node_index].datadir, "wagerr.conf"), 'r', encoding='utf8') as f:
            for line in f:
                if line.startswith(setting_name + "="):
                    return line.split("=")[1].strip("\n")
        return None

    def get_local_peer(self, node_index, is_rpc=False):
        port = self.get_node_setting(node_index, "rpcport" if is_rpc else "port")
        return "127.0.0.1:" + str(rpc_port(node_index) if port is None else port)

    def sync_node_datadir(self, node_index, left, right):
        node = self.nodes[node_index]
        node.stop_node()
        node.wait_until_stopped()
        if not left:
            left = self.nodes[node_index].datadir
        if not right:
            right = self.nodes[node_index].datadir
        if os.path.isdir(right):
            remove_tree(right)
        copy_tree(left, right)
        node.rpchost = self.get_local_peer(node_index, True)
        node.start(self.extra_args)
        node.wait_for_rpc_connection()

    def set_test_params(self):
        self.extra_args = None
        #self.extra_args = [["-debug"], ["-debug"], ["-debug"], ["-debug"]]
        self.setup_clean_chain = True
        self.num_nodes = 4
        self.players = []

    def connect_network(self):
        for pair in [[n, n + 1 if n + 1 < self.num_nodes else 0] for n in range(self.num_nodes)]:
            for i in range(len(pair)):
                assert i < 2
                self.nodes[pair[i]].addnode(self.get_local_peer(pair[1 - i]), "onetry")
                wait_until(lambda:  all(peer['version'] != 0 for peer in self.nodes[pair[i]].getpeerinfo()))
        self.sync_all()
        for n in range(self.num_nodes):
            idx_l = n
            idx_r = n + 1 if n + 1 < self.num_nodes else 0
            assert_equal(self.nodes[idx_l].getblockcount(), self.nodes[idx_r].getblockcount())

    def setup_network(self):
        self.log.info("Setup Network")
        self.setup_nodes()
        self.connect_network()

    def save_cache(self, force=False):
        dir_names = dict()
        for n in range(self.num_nodes):
            dir_name = self.get_cache_dir_name(n, self.nodes[n].getblockcount())
            if force or not os.path.isdir(dir_name):
                dir_names[n] = dir_name
        if len(dir_names) > 0:
            for node_index in dir_names.keys():
                self.sync_node_datadir(node_index, None, dir_names[node_index])
            self.connect_network()

    def load_cache(self, block_count):
        dir_names = dict()
        for n in range(self.num_nodes):
            dir_name = self.get_cache_dir_name(n, block_count)
            if os.path.isdir(dir_name):
                dir_names[n] = dir_name
        if len(dir_names) == self.num_nodes:
            for node_index in range(self.num_nodes):
                self.sync_node_datadir(node_index, dir_names[node_index], None)
            self.connect_network()
            return True
        return False

    def check_minting(self, block_count=250):
        self.log.info("Check Minting...")

        self.nodes[1].importprivkey(WGR_WALLET_ORACLE['key'])
        self.nodes[1].importprivkey(WGR_WALLET_EVENT['key'])
        self.nodes[1].importprivkey(WGR_WALLET_DEV['key'])
        self.nodes[1].importprivkey(WGR_WALLET_OMNO['key'])

        self.players.append(self.nodes[2].getnewaddress('Node2Addr'))
        self.players.append(self.nodes[3].getnewaddress('Node3Addr'))

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

        self.nodes[0].generate(51)

        self.sync_all()

        for n in range(self.num_nodes):
            assert_equal( self.nodes[n].getblockcount(), 300)

        # check oracle balance
        assert_equal(self.nodes[1].getbalance(), 80000)
        # check players balance
        assert_equal(self.nodes[2].getbalance(), 40000)
        assert_equal(self.nodes[3].getbalance(), 40000)

        self.log.info("Minting Success")

    def check_mapping(self):
        self.log.info("Check Mapping...")

        self.nodes[0].generate(1)
        self.sync_all()

        assert_raises_rpc_error(-1, "No mapping exist for the mapping index you provided.", self.nodes[0].getmappingid, "", "")
        assert_raises_rpc_error(-1, "No mapping exist for the mapping index you provided.", self.nodes[0].getmappingname, "abc123", 0)

        # add sports to mapping
        for id in range(len(sport_names)):
            mapping_opcode = make_mapping(SPORT_MAPPING, id, sport_names[id])
            post_opcode(self.nodes[1], mapping_opcode, WGR_WALLET_ORACLE['addr'])

        # generate block for unlocking used Oracle's UTXO
        self.sync_all()
        self.nodes[0].generate(1)
        self.sync_all()

        # add rounds to mapping
        for id in range(len(round_names)):
            mapping_opcode = make_mapping(ROUND_MAPPING, id, round_names[id])
            post_opcode(self.nodes[1], mapping_opcode, WGR_WALLET_ORACLE['addr'])

        # generate block for unlocking used Oracle's UTXO
        self.sync_all()
        self.nodes[0].generate(1)
        self.sync_all()

        # add teams to mapping
        for id in range(len(team_names)):
            mapping_opcode = make_mapping(TEAM_MAPPING, id, team_names[id])
            post_opcode(self.nodes[1], mapping_opcode, WGR_WALLET_ORACLE['addr'])

        # generate block for unlocking used Oracle's UTXO
        self.sync_all()
        self.nodes[0].generate(1)
        self.sync_all()

        # add tournaments to mapping
        for id in range(len(tournament_names)):
            mapping_opcode = make_mapping(TOURNAMENT_MAPPING, id, tournament_names[id])
            post_opcode(self.nodes[1], mapping_opcode, WGR_WALLET_ORACLE['addr'])

        self.sync_all()
        self.nodes[0].generate(1)
        self.sync_all()

        for node in self.nodes:
            # Check sports mapping
            for id in range(len(sport_names)):
                mapping = node.getmappingname("sports", id)[0]
                assert_equal(mapping['exists'], True)
                assert_equal(mapping['mapping-name'], sport_names[id])
                assert_equal(mapping['mapping-type'], "sports")
                assert_equal(mapping['mapping-index'], id)
                mappingid = node.getmappingid("sports", sport_names[id])[0]
                assert_equal(mappingid['exists'], True)
                assert_equal(mappingid['mapping-index'], "sports")
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
            for id in range(len(team_names)):
                mapping = node.getmappingname("teamnames", id)[0]
                assert_equal(mapping['exists'], True)
                assert_equal(mapping['mapping-name'], team_names[id])
                assert_equal(mapping['mapping-type'], "teamnames")
                assert_equal(mapping['mapping-index'], id)
                mappingid = node.getmappingid("teamnames", team_names[id])[0]
                assert_equal(mappingid['exists'], True)
                assert_equal(mappingid['mapping-index'], "teamnames")
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

        self.start_time = int(time.time() + 60 * 60)
        # array for odds of events
        self.odds_events = []

        # 0: Football - UEFA Champions League - Real Madrid vs Barcelona
        mlevent = make_event(0, # Event ID
                            self.start_time, # start time = current + hour
                            sport_names.index("Football"), # Sport ID
                            tournament_names.index("UEFA Champions League"), # Tournament ID
                            round_names.index("round1"), # Round ID
                            team_names.index("Real Madrid"), # Home Team
                            team_names.index("Barcelona"), # Away Team
                            15000, # home odds
                            18000, # away odds
                            13000) # draw odds
        self.odds_events.append({'homeOdds': 15000, 'awayOdds': 18000, 'drawOdds': 13000})
        post_opcode(self.nodes[1], mlevent, WGR_WALLET_EVENT['addr'])

        # 1: MMA - UFC244 - Jorge Masvidal vs Nate Diaz
        mlevent = make_event(1, # Event ID
                            self.start_time, # start time = current + hour
                            sport_names.index("MMA"), # Sport ID
                            tournament_names.index("UFC244"), # Tournament ID
                            round_names.index("round1"), # Round ID
                            team_names.index("Jorge Masvidal"), # Home Team
                            team_names.index("Nate Diaz"), # Away Team
                            14000, # home odds
                            28000, # away odds
                            50000) # draw odds
        self.odds_events.append({'homeOdds': 14000, 'awayOdds': 28000, 'drawOdds': 50000})
        post_opcode(self.nodes[1], mlevent, WGR_WALLET_EVENT['addr'])

        # 2: CSGO - PGL Major Krakow - Astralis vs Gambit
        mlevent = make_event(2, # Event ID
                            self.start_time, # start time = current + hour
                            sport_names.index("CSGO"), # Sport ID
                            tournament_names.index("PGL Major Krakow"), # Tournament ID
                            round_names.index("round1"), # Round ID
                            team_names.index("Astralis"), # Home Team
                            team_names.index("Gambit"), # Away Team
                            14000, # home odds
                            33000, # away odds
                            0) # draw odds
        self.odds_events.append({'homeOdds': 14000, 'awayOdds': 33000, 'drawOdds': 0})
        post_opcode(self.nodes[1], mlevent, WGR_WALLET_EVENT['addr'])
        # 3: DOTA2 - EPICENTER Major - Virtus Pro vs Team Liquid
        mlevent = make_event(3, # Event ID
                            self.start_time, # start time = current + hour
                            sport_names.index("DOTA2"), # Sport ID
                            tournament_names.index("EPICENTER Major"), # Tournament ID
                            round_names.index("round1"), # Round ID
                            team_names.index("Virtus Pro"), # Home Team
                            team_names.index("Team Liquid"), # Away Team
                            24000, # home odds
                            17000, # away odds
                            0) # draw odds
        self.odds_events.append({'homeOdds': 24000, 'awayOdds': 17000, 'drawOdds': 0})
        post_opcode(self.nodes[1], mlevent, WGR_WALLET_EVENT['addr'])

        self.sync_all()

        self.nodes[0].generate(1)

        self.sync_all()

        for node in self.nodes:
            list_events = node.listevents()
            found_events = 0
            for event in list_events:
                event_id = event['event_id']
                assert_equal(event['sport'], sport_names[event_id])
                assert_equal(event['tournament'], tournament_names[event_id])
                assert_equal(event['teams']['home'], team_names[2 * event_id])
                assert_equal(event['teams']['away'], team_names[(2 * event_id) + 1])

                found_events = found_events | (1 << event['event_id'])
            # check that all events found
            assert_equal(found_events, 0b1111)

        self.log.info("Event creation Success")

    def check_event_patch(self):
        self.log.info("Check Event Patch...")

        for node in self.nodes:
            events = node.listevents()
            for event in events:
                assert_equal(event['starting'], self.start_time)

        self.start_time = int(time.time() + 60 * 20) # new time - curr + 20mins

        for event in events:
            event_patch = make_event_patch(event['event_id'], self.start_time)
            post_opcode(self.nodes[1], event_patch, WGR_WALLET_EVENT['addr'])

        self.sync_all()
        self.nodes[0].generate(1)
        self.sync_all()

        for node in self.nodes:
            events = node.listevents()
            for event in events:
                assert_equal(event['starting'], self.start_time)

        self.log.info("Event Patch Success")

    def check_update_odds(self):
        self.log.info("Check updating odds...")
        for node in self.nodes:
            events = node.listevents()
            for event in events:
                event_id = event['event_id']
                # 0 mean ml odds in odds array
                assert_equal(event['odds'][0]['mlHome'], self.odds_events[event_id]['homeOdds'])
                assert_equal(event['odds'][0]['mlAway'], self.odds_events[event_id]['awayOdds'])
                assert_equal(event['odds'][0]['mlDraw'], self.odds_events[event_id]['drawOdds'])

        # Change odd for event 1 - UFC244
        event_id = tournament_names.index("UFC244")
        self.odds_events[event_id]['homeOdds'] = 16000
        self.odds_events[event_id]['awayOdds'] = 25000
        self.odds_events[event_id]['drawOdds'] = 80000
        update_odds_opcode = make_update_ml_odds(event_id,
                                                self.odds_events[event_id]['homeOdds'],
                                                self.odds_events[event_id]['awayOdds'],
                                                self.odds_events[event_id]['drawOdds'])
        post_opcode(self.nodes[1], update_odds_opcode, WGR_WALLET_EVENT['addr'])

        self.sync_all()
        self.nodes[0].generate(1)
        self.sync_all()

        for node in self.nodes:
            events = node.listevents()
            for event in events:
                event_id = event['event_id']
                # 0 mean ml odds in odds array
                assert_equal(event['odds'][0]['mlHome'], self.odds_events[event_id]['homeOdds'])
                assert_equal(event['odds'][0]['mlAway'], self.odds_events[event_id]['awayOdds'])
                assert_equal(event['odds'][0]['mlDraw'], self.odds_events[event_id]['drawOdds'])

        self.log.info("Updating odds Success")

    def check_spread_event(self):
        self.log.info("Check Spread events...")
        for node in self.nodes:
            events = node.listevents()
            for event in events:
                # 1 mean spreads odds in odds array
                assert_equal(event['odds'][1]['spreadPoints'], 0)
                assert_equal(event['odds'][1]['spreadHome'], 0)
                assert_equal(event['odds'][1]['spreadAway'], 0)
        # make spread for event 0: UEFA Champions League
        spread_event_opcode = make_spread_event(0, 1, 28000, 14000)
        post_opcode(self.nodes[1], spread_event_opcode, WGR_WALLET_EVENT['addr'])

        self.sync_all()
        self.nodes[0].generate(1)
        self.sync_all()

        for node in self.nodes:
            events = node.listevents()
            for event in events:
                if event['event_id'] == 0:
                    assert_equal(event['odds'][1]['spreadPoints'], 1)
                    assert_equal(event['odds'][1]['spreadHome'], 28000)
                    assert_equal(event['odds'][1]['spreadAway'], 14000)

        self.log.info("Spread events Success")

    def check_spread_event_v2(self):
        self.log.info("Check Spread v2 events...")
        mlevent = make_event(4, # Event ID
                             self.start_time, # start time = current + hour
                             sport_names.index("DOTA2"), # Sport ID
                             tournament_names.index("EPICENTER Major"), # Tournament ID
                             round_names.index("round1"), # Round ID
                             team_names.index("Virtus Pro"), # Home Team
                             team_names.index("Team Liquid"), # Away Team
                             10000, # home odds
                             30000, # away odds
                             0) # draw odds
        post_opcode(self.nodes[1], mlevent, WGR_WALLET_EVENT['addr'])

        self.sync_all()
        self.nodes[0].generate(1)
        self.sync_all()

        for node in self.nodes:
            events = node.listevents()
            for event in events:
                if event['event_id'] == 4:
                    assert_equal(event['odds'][1]['spreadPoints'], 0)
                    assert_equal(event['odds'][1]['spreadHome'], 0)
                    assert_equal(event['odds'][1]['spreadAway'], 0)
        # make spread for event 4: DOTA2
        spread_event_opcode = make_spread_event(4, 150, 25000, 15000)
        post_opcode(self.nodes[1], spread_event_opcode, WGR_WALLET_EVENT['addr'])

        self.sync_all()
        self.nodes[0].generate(1)
        self.sync_all()

        for node in self.nodes:
            events = node.listevents()
            for event in events:
                if event['event_id'] == 4:
                    assert_equal(event['odds'][1]['spreadPoints'], 150)
                    assert_equal(event['odds'][1]['spreadHome'], 25000)
                    assert_equal(event['odds'][1]['spreadAway'], 15000)

        # make spread for event 4: DOTA2
        spread_event_opcode_negative = make_spread_event(4, -250, 27000, 13000)
        post_opcode(self.nodes[1], spread_event_opcode_negative, WGR_WALLET_EVENT['addr'])

        self.sync_all()
        self.nodes[0].generate(1)
        self.sync_all()

        for node in self.nodes:
            events = node.listevents()
            for event in events:
                if event['event_id'] == 4:
                    tmp = event['odds'][1]['spreadPoints']
                    assert_equal(ctypes.c_long(tmp).value, -250)
                    assert_equal(event['odds'][1]['spreadHome'], 27000)
                    assert_equal(event['odds'][1]['spreadAway'], 13000)

        self.log.info("Spread v2 events Success")

    def check_total_event(self):
        self.log.info("Check Total events...")
        for node in self.nodes:
            events = node.listevents()
            for event in events:
                # 2 mean totals odds in odds array
                assert_equal(event['odds'][2]['totalsPoints'], 0)
                assert_equal(event['odds'][2]['totalsOver'], 0)
                assert_equal(event['odds'][2]['totalsUnder'], 0)

        # make totals for event 2: PGL Major Krakow
        totals_event_opcode = make_total_event(2, 26, 21000, 23000)
        post_opcode(self.nodes[1], totals_event_opcode, WGR_WALLET_EVENT['addr'])

        self.sync_all()
        self.nodes[0].generate(1)
        self.sync_all()

        for node in self.nodes:
            events = node.listevents()
            for event in events:
                if event['event_id'] == 2:
                    assert_equal(event['odds'][2]['totalsPoints'], 26)
                    assert_equal(event['odds'][2]['totalsOver'], 21000)
                    assert_equal(event['odds'][2]['totalsUnder'], 23000)

        self.log.info("Total events Success")

    def check_ml_bet(self):
        self.log.info("Check Money Line Bets...")

        global player1_total_bet
        player1_total_bet = 0
        global player2_total_bet
        player2_total_bet = 0

        # place bet to ml event 3 with incorrect amounts
        assert_raises_rpc_error(-31, "Incorrect bet amount. Please ensure your bet is between 25 - 10000 WGR inclusive.", self.nodes[2].placebet, 3, outcome_away_win, 24)
        assert_raises_rpc_error(-31, "Incorrect bet amount. Please ensure your bet is between 25 - 10000 WGR inclusive.", self.nodes[2].placebet, 3, outcome_away_win, 10001)
        # place bet to ml event 3: DOTA2 - EPICENTER Major - Virtus Pro vs Team Liquid
        # player 1 bet to Team Liquid with odds 17000
        player1_bet = 100
        player1_total_bet = player1_total_bet + player1_bet
        self.nodes[2].placebet(3, outcome_away_win, player1_bet)
        winnings = Decimal(player1_bet * self.odds_events[3]['awayOdds'])
        player1_expected_win = (winnings - ((winnings - player1_bet * ODDS_DIVISOR) / 1000 * BETX_PERMILLE)) / ODDS_DIVISOR

        # change odds
        self.odds_events[3]['homeOdds'] = 28000
        self.odds_events[3]['awayOdds'] = 14000
        self.odds_events[3]['drawOdds'] = 0
        update_odds_opcode = make_update_ml_odds(3,
                                                self.odds_events[3]['homeOdds'],
                                                self.odds_events[3]['awayOdds'],
                                                self.odds_events[3]['drawOdds'])
        post_opcode(self.nodes[1], update_odds_opcode, WGR_WALLET_EVENT['addr'])

        self.sync_all()
        self.nodes[0].generate(1)
        self.sync_all()

        #player 2 bet to Team Liquid with incorrect bets.
        assert_raises_rpc_error(-31, "Incorrect bet amount. Please ensure your bet is between 25 - 10000 WGR inclusive.", self.nodes[3].placebet, 3, outcome_away_win, 24)
        assert_raises_rpc_error(-31, "Incorrect bet amount. Please ensure your bet is between 25 - 10000 WGR inclusive.", self.nodes[3].placebet, 3, outcome_away_win, 10001)
        #player 2 bet to Team Liquid with odds 14000
        player2_bet = 200
        player2_total_bet = player2_total_bet + player2_bet
        self.nodes[3].placebet(3, outcome_away_win, player2_bet)
        winnings = Decimal(player2_bet * self.odds_events[3]['awayOdds'])
        player2_expected_win = (winnings - ((winnings - player2_bet * ODDS_DIVISOR) / 1000 * BETX_PERMILLE)) / ODDS_DIVISOR

        #self.log.info("Event Liability")
        #pprint.pprint(self.nodes[0].geteventliability(3))
        liability=self.nodes[0].geteventliability(3)
        gotliability=liability["moneyline-away-liability"]
        #self.log.info("Monyline Away %s" % liability["moneyline-away-liability"])
        assert_equal(gotliability, Decimal(165))
        # place result for event 3: Team Liquid wins.
        result_opcode = make_result(3, STANDARD_RESULT, 0, 1)
        post_opcode(self.nodes[1], result_opcode, WGR_WALLET_EVENT['addr'])

        self.sync_all()
        self.nodes[0].generate(1)
        self.sync_all()

        player1_balance_before = Decimal(self.nodes[2].getbalance())
        player2_balance_before = Decimal(self.nodes[3].getbalance())

        listbets = self.nodes[0].listbetsdb(False)

        # generate block with payouts
        blockhash = self.nodes[0].generate(1)[0]
        block = self.nodes[0].getblock(blockhash)
        height = block['height']

        self.sync_all()
        #print("Player 1 Total Bet", player1_total_bet)
        #print("Player 2 Total Bet", player2_total_bet)

        payoutsInfo = self.nodes[0].getpayoutinfosince(1)
        #self.log.info("Listbets")
        #pprint.pprint(listbets)
        #self.log.info("Payouts Info")
        #pprint.pprint(payoutsInfo)

        check_bet_payouts_info(listbets, payoutsInfo)

        player1_balance_after = Decimal(self.nodes[2].getbalance())
        player2_balance_after = Decimal(self.nodes[3].getbalance())

        assert_equal(player1_balance_before + player1_expected_win, player1_balance_after)
        assert_equal(player2_balance_before + player2_expected_win, player2_balance_after)

        # create losing bet
        player1_balance_before = Decimal(self.nodes[2].getbalance())
        player1_bet = 200
        player1_total_bet = player1_total_bet + player1_bet
        player1_txid=(self.nodes[2].placebet(1, outcome_home_win, player1_bet))
        player1_expected_loss = Decimal(200)
        player1_transaction=self.nodes[2].gettransaction(player1_txid)
        player1_bet_cost=0
        for transaction in player1_transaction['details']:
            player1_bet_cost=player1_bet_cost + transaction['fee']

        #self.log.info("Bet Cost %s" % player1_bet_cost)
        #self.log.info("Expected Loss  %d" % player1_expected_loss)

        # create draw bet (will show up as a win)
        player2_bet = 150
        player2_total_bet = player2_total_bet + player2_bet

        self.nodes[3].placebet(1, outcome_draw, player2_bet)
        winnings = Decimal(player2_bet * self.odds_events[1]['drawOdds'])
        player2_expected_win = (winnings - ((winnings - player2_bet * ODDS_DIVISOR) / 1000 * BETX_PERMILLE)) / ODDS_DIVISOR
        #self.log.info("Winnings %d" % winnings)
        #self.log.info("Expected Win %s" % player2_expected_win)

        self.nodes[3].generate(1)
        self.sync_all()

        #self.log.info("Event Liability")
        #pprint.pprint(self.nodes[0].geteventliability(1))
        liability=self.nodes[0].geteventliability(1)
        gotliability=liability["moneyline-draw-liability"]
        assert_equal(gotliability, Decimal(1137))
        gotliability=liability["moneyline-home-liability"]
        assert_equal(gotliability, Decimal(312))

        # close event 1
        result_opcode = make_result(1, STANDARD_RESULT, 1, 1)
        post_opcode(self.nodes[1], result_opcode, WGR_WALLET_EVENT['addr'])

        self.sync_all()
        self.nodes[0].generate(1)
        self.sync_all()

        player2_balance_before = Decimal(self.nodes[3].getbalance())

        listbets = self.nodes[0].listbetsdb(False)

       # generate block with payouts
        blockhash = self.nodes[0].generate(1)[0]
        block = self.nodes[0].getblock(blockhash)
        height = block['height']

        self.sync_all()
        #print("Player 1 Total Bet", player1_total_bet)
        #print("Player 2 Total Bet", player2_total_bet)


        payoutsInfo = self.nodes[0].getpayoutinfosince(1)

        #self.log.info("Listbets")
        #pprint.pprint(listbets)
        #self.log.info("Payouts Info")
        #pprint.pprint(payoutsInfo)
        # no payout info for losing bet
        # check_bet_payouts_info(listbets, payoutsInfo)

        player1_balance_after = Decimal(self.nodes[2].getbalance())
        player2_balance_after = Decimal(self.nodes[3].getbalance())

        #self.log.info("Player 1 Balance Before %s" % player1_balance_before)
        #self.log.info("Player 1 Balance After %s" % player1_balance_after)
        #self.log.info("Player 2 Balance Before %s" % player2_balance_before)
        #self.log.info("Player 2 Balance After %s" % player2_balance_after)
        #self.log.info("Player 2 Expected Win %d" % player2_balance_before + player2_expected_win)

        assert_equal((player1_balance_before - player1_expected_loss + player1_bet_cost), player1_balance_after)
        assert_equal(player2_balance_before + player2_expected_win, player2_balance_after)

        self.log.info("Money Line Bets Success")

    def check_spreads_bet(self):
        self.log.info("Check Spread Bets...")

        global player1_total_bet
        global player2_total_bet

        assert_raises_rpc_error(-31, "Incorrect bet amount. Please ensure your bet is between 25 - 10000 WGR inclusive.", self.nodes[2].placebet, 0, outcome_spread_home, 24)
        assert_raises_rpc_error(-31, "Incorrect bet amount. Please ensure your bet is between 25 - 10000 WGR inclusive.", self.nodes[2].placebet, 0, outcome_spread_home, 10001)
        # place spread bet to event 0: UEFA Champions League, expect that event result will be 2:0 for home team
        # player 1 bet to spread home, mean that home will win with spread points for away = 1
        player1_bet = 300
        player1_total_bet = player1_total_bet + player1_bet
        self.nodes[2].placebet(0, outcome_spread_home, player1_bet)
        winnings = Decimal(player1_bet * 28000)
        player1_expected_win = (winnings - ((winnings - player1_bet * ODDS_DIVISOR) / 1000 * BETX_PERMILLE)) / ODDS_DIVISOR

        # change spread condition for event 0
        spread_event_opcode = make_spread_event(0, 2, 23000, 15000)
        post_opcode(self.nodes[1], spread_event_opcode, WGR_WALLET_EVENT['addr'])

        self.sync_all()
        self.nodes[0].generate(1)
        self.sync_all()

        assert_raises_rpc_error(-31, "Incorrect bet amount. Please ensure your bet is between 25 - 10000 WGR inclusive.", self.nodes[3].placebet, 0, outcome_spread_home, 24)
        assert_raises_rpc_error(-31, "Incorrect bet amount. Please ensure your bet is between 25 - 10000 WGR inclusive.", self.nodes[3].placebet, 0, outcome_spread_home, 10001)
        # player 2 bet to spread home, mean that home will win with spread points for away = 2
        # for our results it means refund
        player2_bet = 200
        player2_total_bet = player2_total_bet + player2_bet
        self.nodes[3].placebet(0, outcome_spread_home, player2_bet)
        winnings = Decimal(player2_bet * ODDS_DIVISOR)
        player2_expected_win = (winnings - ((winnings - player2_bet * ODDS_DIVISOR) / 1000 * BETX_PERMILLE)) / ODDS_DIVISOR

        #self.log.info("Event Liability")
        #pprint.pprint(self.nodes[0].geteventliability(0))
        liability=self.nodes[0].geteventliability(0)
        gotliability=liability["moneyline-draw-liability"]
        assert_equal(gotliability, Decimal(1137))
        gotliability=liability["moneyline-home-liability"]
        assert_equal(gotliability, Decimal(312))


        # place result for event 0: RM wins BARC with 2:0.
        result_opcode = make_result(0, STANDARD_RESULT, 2, 0)
        post_opcode(self.nodes[1], result_opcode, WGR_WALLET_EVENT['addr'])

        self.sync_all()
        self.nodes[0].generate(1)
        self.sync_all()

        player1_balance_before = Decimal(self.nodes[2].getbalance())
        player2_balance_before = Decimal(self.nodes[3].getbalance())

        listbets = self.nodes[0].listbetsdb(False)

        # generate block with payouts
        blockhash = self.nodes[0].generate(1)[0]
        block = self.nodes[0].getblock(blockhash)
        height = block['height']


        self.sync_all()
        #print("Player 1 Total Bet", player1_total_bet)
        #print("Player 2 Total Bet", player2_total_bet)

        payoutsInfo = self.nodes[0].getpayoutinfosince(1)

        check_bet_payouts_info(listbets, payoutsInfo)

        player1_balance_after = Decimal(self.nodes[2].getbalance())
        player2_balance_after = Decimal(self.nodes[3].getbalance())

        assert_equal(player1_balance_before + player1_expected_win, player1_balance_after)
        assert_equal(player2_balance_before + player2_expected_win, player2_balance_after)

        self.log.info("Spread Bets Success")

    def check_spreads_bet_v2(self):
        self.log.info("Check Spread Bets v2...")

        global player1_total_bet
        global player2_total_bet

        assert_raises_rpc_error(-31, "Incorrect bet amount. Please ensure your bet is between 25 - 10000 WGR inclusive.", self.nodes[2].placebet, 4, outcome_spread_away, 24)
        assert_raises_rpc_error(-31, "Incorrect bet amount. Please ensure your bet is between 25 - 10000 WGR inclusive.", self.nodes[2].placebet, 4, outcome_spread_away, 10001)

        # place spread bet to event 4: EPICENTER Major, expect that event result will be 2:0 for away team
        # current spread event is: points=-250, homeOdds=27000, awayOdds=13000
        # player 1 bet to spread away, mean that away will not lose with diff more then 1 score
        player1_bet = 300
        player1_total_bet = player1_total_bet + player1_bet
        self.nodes[2].placebet(4, outcome_spread_away, player1_bet)
        winnings = Decimal(player1_bet * 13000)
        player1_expected_win = (winnings - ((winnings - player1_bet * ODDS_DIVISOR) / 1000 * BETX_PERMILLE)) / ODDS_DIVISOR

        # change spread condition for event 4
        spread_event_opcode = make_spread_event(4, -200, 29000, 12000)
        post_opcode(self.nodes[1], spread_event_opcode, WGR_WALLET_EVENT['addr'])

        self.sync_all()
        self.nodes[0].generate(1)
        self.sync_all()

        assert_raises_rpc_error(-31, "Incorrect bet amount. Please ensure your bet is between 25 - 10000 WGR inclusive.", self.nodes[3].placebet, 4, outcome_spread_home, 24)
        assert_raises_rpc_error(-31, "Incorrect bet amount. Please ensure your bet is between 25 - 10000 WGR inclusive.", self.nodes[3].placebet, 4, outcome_spread_home, 10001)
        # player 2 bet to spread home, mean that home will win with 2 extra scores
        # for our results it means refund
        player2_bet = 200
        player2_total_bet = player2_total_bet + player2_bet
        self.nodes[3].placebet(4, outcome_spread_home, player2_bet)
        winnings = Decimal(player2_bet * ODDS_DIVISOR)
        player2_expected_win = (winnings - ((winnings - player2_bet * ODDS_DIVISOR) / 1000 * BETX_PERMILLE)) / ODDS_DIVISOR

        self.sync_all()
        self.nodes[0].generate(1)
        self.sync_all()

        #self.log.info("Event Liability")
        #pprint.pprint(self.nodes[0].geteventliability(4))
        liability=self.nodes[0].geteventliability(4)
        gotliability=liability["spread-away-liability"]
        assert_equal(gotliability, Decimal(384))
        gotliability=liability["spread-home-liability"]
        assert_equal(gotliability, Decimal(557))
        gotliability=liability["spread-push-liability"]
        assert_equal(gotliability, Decimal(500))
 
        # place result for event 4:
        result_opcode = make_result(4, STANDARD_RESULT, 200, 0)
        post_opcode(self.nodes[1], result_opcode, WGR_WALLET_EVENT['addr'])

        self.sync_all()
        self.nodes[0].generate(1)
        self.sync_all()

        player1_balance_before = Decimal(self.nodes[2].getbalance())
        player2_balance_before = Decimal(self.nodes[3].getbalance())

        # print("player1 balance before: ", player1_balance_before)
        # print("player1 exp win: ", player1_expected_win)
        # print("player2 balance before: ", player2_balance_before)
        # print("player2 exp win: ", player2_expected_win)

        # generate block with payouts
        blockhash = self.nodes[0].generate(1)[0]
        block = self.nodes[0].getblock(blockhash)

        self.sync_all()
        #print("Player 1 Total Bet", player1_total_bet)
        #print("Player 2 Total Bet", player2_total_bet)

        # print(pprint.pformat(block))

        player1_balance_after = Decimal(self.nodes[2].getbalance())
        player2_balance_after = Decimal(self.nodes[3].getbalance())

        # print("player1 balance after: ", player1_balance_after)
        # print("player2 balance after: ", player2_balance_after)

        assert_equal(player1_balance_before + player1_expected_win, player1_balance_after)
        assert_equal(player2_balance_before + player2_expected_win, player2_balance_after)

        # Check edge cases
        sprevent = make_event(11, # Event ID
                    self.start_time, # start time = current + hour
                    sport_names.index("Spread Sport"), # Sport ID
                    tournament_names.index("Spread Tournament"), # Tournament ID
                    round_names.index("round2"), # Round ID
                    team_names.index("Spread Team One"), # Home Team
                    team_names.index("Spread Team Two"), # Away Team
                    15000, # home odds
                    18000, # away odds
                    13000) # draw odds

        post_opcode(self.nodes[1], sprevent, WGR_WALLET_EVENT['addr'])

        self.sync_all()
        self.nodes[0].generate(1)
        self.sync_all()

        # create spread event
        spread_event_opcode = make_spread_event(11, -125, 14000, 26000)
        post_opcode(self.nodes[1], spread_event_opcode, WGR_WALLET_EVENT['addr'])

        self.sync_all()
        self.nodes[0].generate(1)
        self.sync_all()

        self.log.info("Spread Bets v2 Success")

    def check_totals_bet(self):
        self.log.info("Check Total Bets...")

        global player1_total_bet
        global player2_total_bet

        assert_raises_rpc_error(-31, "Incorrect bet amount. Please ensure your bet is between 25 - 10000 WGR inclusive.", self.nodes[2].placebet, 2, outcome_total_over, 24)
        assert_raises_rpc_error(-31, "Incorrect bet amount. Please ensure your bet is between 25 - 10000 WGR inclusive.", self.nodes[2].placebet, 2, outcome_total_over, 10001)
        # place spread bet to event 2: PGL Major Krakow
        # player 1 bet to total over with odds 21000
        player1_bet = 200
        player1_total_bet = player1_total_bet + player1_bet
        self.nodes[2].placebet(2, outcome_total_over, player1_bet)
        winnings = Decimal(player1_bet * 21000)
        player1_expected_win = (winnings - ((winnings - player1_bet * ODDS_DIVISOR) / 1000 * BETX_PERMILLE)) / ODDS_DIVISOR

        # change totals condition for event 2
        total_event_opcode = make_total_event(2, 28, 28000, 17000)
        post_opcode(self.nodes[1], total_event_opcode, WGR_WALLET_EVENT['addr'])

        self.sync_all()
        self.nodes[0].generate(1)
        self.sync_all()

        assert_raises_rpc_error(-31, "Incorrect bet amount. Please ensure your bet is between 25 - 10000 WGR inclusive.", self.nodes[3].placebet, 2, outcome_total_under, 24)
        assert_raises_rpc_error(-31, "Incorrect bet amount. Please ensure your bet is between 25 - 10000 WGR inclusive.", self.nodes[2].placebet, 2, outcome_total_under, 10001)
        # player 2 bet to total under with odds 17000
        player2_bet = 200
        player2_total_bet = player2_total_bet + player2_bet
        self.nodes[3].placebet(2, outcome_total_under, player2_bet)
        winnings = Decimal(player2_bet * 17000)
        player2_expected_win = (winnings - ((winnings - player2_bet * ODDS_DIVISOR) / 1000 * BETX_PERMILLE)) / ODDS_DIVISOR

        #self.log.info("Event Liability")
        #pprint.pprint(self.nodes[0].geteventliability(2))
        liability=self.nodes[0].geteventliability(2)
        gotliability=liability["total-over-liability"]
        assert_equal(gotliability, Decimal(406))
        gotliability=liability["total-push-liability"]
        assert_equal(gotliability, Decimal(200))

        # place result for event 2: Gambit wins with score 11:16
        result_opcode = make_result(2, STANDARD_RESULT, 11, 16)
        post_opcode(self.nodes[1], result_opcode, WGR_WALLET_EVENT['addr'])

        self.sync_all()
        self.nodes[0].generate(1)
        self.sync_all()

        player1_balance_before = Decimal(self.nodes[2].getbalance())
        player2_balance_before = Decimal(self.nodes[3].getbalance())

        listbets = self.nodes[0].listbetsdb(False)

        # generate block with payouts
        blockhash = self.nodes[0].generate(1)[0]
        block = self.nodes[0].getblock(blockhash)
        height = block['height']

        self.sync_all()
        #print("Player 1 Total Bet", player1_total_bet)
        #print("Player 2 Total Bet", player2_total_bet)

        payoutsInfo = self.nodes[0].getpayoutinfosince(1)

        check_bet_payouts_info(listbets, payoutsInfo)

        player1_balance_after = Decimal(self.nodes[2].getbalance())
        player2_balance_after = Decimal(self.nodes[3].getbalance())

        assert_equal(player1_balance_before + player1_expected_win, player1_balance_after)
        assert_equal(player2_balance_before + player2_expected_win, player2_balance_after)

        self.log.info("Total Bets Success")

    def check_parlays_bet(self):
        self.log.info("Check Parlay Bets...")

        global player1_total_bet
        global player2_total_bet

        # add new events
        # 4: CSGO - PGL Major Krakow - Astralis vs Gambit round2
        mlevent = make_event(5, # Event ID
                            self.start_time, # start time = current + hour
                            sport_names.index("CSGO"), # Sport ID
                            tournament_names.index("PGL Major Krakow"), # Tournament ID
                            round_names.index("round2"), # Round ID
                            team_names.index("Astralis"), # Home Team
                            team_names.index("Gambit"), # Away Team
                            14000, # home odds
                            33000, # away odds
                            0) # draw odds
        self.odds_events.append({'homeOdds': 14000, 'awayOdds': 33000, 'drawOdds': 0})
        post_opcode(self.nodes[1], mlevent, WGR_WALLET_EVENT['addr'])

        # 5: CSGO - PGL Major Krakow - Astralis vs Gambit round3
        mlevent = make_event(6, # Event ID
                            self.start_time, # start time = current + hour
                            sport_names.index("CSGO"), # Sport ID
                            tournament_names.index("PGL Major Krakow"), # Tournament ID
                            round_names.index("round3"), # Round ID
                            team_names.index("Astralis"), # Home Team
                            team_names.index("Gambit"), # Away Team
                            14000, # home odds
                            33000, # away odds
                            0) # draw odds
        self.odds_events.append({'homeOdds': 14000, 'awayOdds': 33000, 'drawOdds': 0})
        post_opcode(self.nodes[1], mlevent, WGR_WALLET_EVENT['addr'])

        # 6: Football - UEFA Champions League - Barcelona vs Real Madrid round2
        mlevent = make_event(7, # Event ID
                            self.start_time, # start time = current + hour
                            sport_names.index("Football"), # Sport ID
                            tournament_names.index("UEFA Champions League"), # Tournament ID
                            round_names.index("round2"), # Round ID
                            team_names.index("Barcelona"), # Home Team
                            team_names.index("Real Madrid"), # Away Team
                            14000, # home odds
                            33000, # away odds
                            0) # draw odds
        self.odds_events.append({'homeOdds': 14000, 'awayOdds': 33000, 'drawOdds': 0})
        post_opcode(self.nodes[1], mlevent, WGR_WALLET_EVENT['addr'])

        self.sync_all()
        self.nodes[0].generate(1)
        self.sync_all()

        # player 1 make express to events 4, 5, 6 - home win
        player1_bet = 200
        player1_total_bet = player1_total_bet + player1_bet
        # 26051 - it is early calculated effective odds for this parlay bet
        player1_expected_win = Decimal(player1_bet * 26051) / ODDS_DIVISOR
        self.nodes[2].placeparlaybet([{'eventId': 5, 'outcome': outcome_home_win}, {'eventId': 6, 'outcome': outcome_home_win}, {'eventId': 7, 'outcome': outcome_home_win}], player1_bet)

        # player 2 make express to events 4, 5, 6 - home win
        player2_bet = 500
        player2_total_bet = player2_total_bet + player2_bet
        # 26051 - it is early calculated effective odds for this parlay bet
        player2_expected_win = Decimal(player2_bet * 26051) / ODDS_DIVISOR
        self.nodes[3].placeparlaybet([{'eventId': 5, 'outcome': outcome_home_win}, {'eventId': 6, 'outcome': outcome_home_win}, {'eventId': 7, 'outcome': outcome_home_win}], player2_bet)

        self.sync_all()
        self.nodes[0].generate(1)
        self.sync_all()

        #self.log.info("Event Liability")
        #pprint.pprint(self.nodes[0].geteventliability(5))
        liability=self.nodes[0].geteventliability(5)
        gotliability=liability["moneyline-home-bets"]
        assert_equal(gotliability, Decimal(2))
        #self.log.info("Event Liability")
        #pprint.pprint(self.nodes[0].geteventliability(6))
        liability=self.nodes[0].geteventliability(6)
        gotliability=liability["moneyline-home-bets"]
        assert_equal(gotliability, Decimal(2))
        #pprint.pprint(self.nodes[0].geteventliability(7))
        liability=self.nodes[0].geteventliability(7)
        gotliability=liability["moneyline-home-bets"]
        assert_equal(gotliability, Decimal(2))


        # place result for event 4: Astralis wins with score 16:9
        result_opcode = make_result(5, STANDARD_RESULT, 16, 9)
        post_opcode(self.nodes[1], result_opcode, WGR_WALLET_EVENT['addr'])
        # place result for event 5: Astralis wins with score 16:14
        result_opcode = make_result(6, STANDARD_RESULT, 16, 14)
        post_opcode(self.nodes[1], result_opcode, WGR_WALLET_EVENT['addr'])
        # place result for event 6: Barcelona wins with score 3:2
        result_opcode = make_result(7, STANDARD_RESULT, 3, 2)
        post_opcode(self.nodes[1], result_opcode, WGR_WALLET_EVENT['addr'])

        self.sync_all()
        self.nodes[0].generate(1)
        self.sync_all()

        player1_balance_before = Decimal(self.nodes[2].getbalance())
        player2_balance_before = Decimal(self.nodes[3].getbalance())

        listbets = self.nodes[0].listbetsdb(False)

        # generate block with payouts
        blockhash = self.nodes[0].generate(1)[0]
        block = self.nodes[0].getblock(blockhash)
        height = block['height']

        self.sync_all()
        #print("Player 1 Total Bet", player1_total_bet)
        #print("Player 2 Total Bet", player2_total_bet)

        payoutsInfo = self.nodes[0].getpayoutinfosince(1)

        check_bet_payouts_info(listbets, payoutsInfo)

        player1_balance_after = Decimal(self.nodes[2].getbalance())
        player2_balance_after = Decimal(self.nodes[3].getbalance())

        assert_equal(player1_balance_before + player1_expected_win, player1_balance_after)
        assert_equal(player2_balance_before + player2_expected_win, player2_balance_after)

        self.log.info("Parlay Bets Success")

    def check_mempool_accept(self):
        self.log.info("Check Mempool Accepting")
        # bets to resulted events shouldn't accepted to memory pool after parlay starting height
        assert_raises_rpc_error(-4, "Error: The transaction was rejected! This might happen if some of the coins in your wallet were already spent, such as if you used a copy of wallet.dat and coins were spent in the copy but not marked as spent here.", self.nodes[2].placebet, 3, outcome_away_win, 1000)
        # bets to nonexistent events shouldn't accepted to memory pool
        assert_raises_rpc_error(-4, "Error: The transaction was rejected! This might happen if some of the coins in your wallet were already spent, such as if you used a copy of wallet.dat and coins were spent in the copy but not marked as spent here.", self.nodes[3].placeparlaybet, [{'eventId': 7, 'outcome': outcome_home_win}, {'eventId': 8, 'outcome': outcome_home_win}, {'eventId': 9, 'outcome': outcome_home_win}], 5000)

        # creating existed mapping
        mapping_opcode = make_mapping(TEAM_MAPPING, 0, "anotherTeamName")
        assert_raises_rpc_error(-25, "", post_opcode, self.nodes[1], mapping_opcode, WGR_WALLET_ORACLE['addr'])

        # creating exited event shouldn't accepted to memory pool
        test_event = make_event(0, # Event ID
                            self.start_time, # start time = current + hour
                            sport_names.index("Football"), # Sport ID
                            tournament_names.index("UEFA Champions League"), # Tournament ID
                            round_names.index("round2"), # Round ID
                            team_names.index("Barcelona"), # Home Team
                            team_names.index("Real Madrid"), # Away Team
                            14000, # home odds
                            33000, # away odds
                            0) # draw odds
        assert_raises_rpc_error(-25, "", post_opcode, self.nodes[1], test_event, WGR_WALLET_EVENT['addr'])

        # creating result for resulted event shouldn't accepted to memory pool
        result_opcode = make_result(4, STANDARD_RESULT, 1, 1)
        assert_raises_rpc_error(-25, "", post_opcode, self.nodes[1], result_opcode, WGR_WALLET_EVENT['addr'])

        # creating totals for not existed event shouldn't accepted to memory pool
        totals_event_opcode = make_total_event(1000, 26, 21000, 23000)
        assert_raises_rpc_error(-25, "", post_opcode, self.nodes[1], totals_event_opcode, WGR_WALLET_EVENT['addr'])

        self.log.info("Mempool Accepting Success")

    def check_timecut_refund(self):
        self.log.info("Check Timecut Refund...")

        global player1_total_bet
        global player2_total_bet

        # add new event with time = 2 mins to go
        self.start_time = int(time.time() + 60 * 2)
        # 7: CSGO - PGL Major Krakow - Astralis vs Gambit round3
        mlevent = make_event(8, # Event ID
                            self.start_time, # start time = current + 2 mins
                            sport_names.index("CSGO"), # Sport ID
                            tournament_names.index("PGL Major Krakow"), # Tournament ID
                            round_names.index("round3"), # Round ID
                            team_names.index("Gambit"), # Home Team
                            team_names.index("Astralis"), # Away Team
                            34000, # home odds
                            14000, # away odds
                            0) # draw odds
        post_opcode(self.nodes[1], mlevent, WGR_WALLET_EVENT['addr'])

        self.sync_all()
        self.nodes[0].generate(1)
        self.sync_all()
        # player 1 bet to Team Gambit with odds 34000 but bet will be refunded
        player1_bet = 1000
        player1_total_bet = player1_total_bet + player1_bet
        self.nodes[2].placebet(8, outcome_home_win, player1_bet)
        winnings = Decimal(player1_bet * ODDS_DIVISOR)
        player1_expected_win = (winnings - ((winnings - player1_bet * ODDS_DIVISOR) / 1000 * BETX_PERMILLE)) / ODDS_DIVISOR

        self.sync_all()
        self.nodes[0].generate(1)
        self.sync_all()

        # place result for event 7: Gambit wins with score 16:14
        result_opcode = make_result(8, STANDARD_RESULT, 160, 140)
        post_opcode(self.nodes[1], result_opcode, WGR_WALLET_EVENT['addr'])

        self.sync_all()
        self.nodes[0].generate(1)
        self.sync_all()

        player1_balance_before = Decimal(self.nodes[2].getbalance())

        listbets = self.nodes[0].listbetsdb(False)

        # generate block with payouts
        blockhash = self.nodes[0].generate(1)[0]
        block = self.nodes[0].getblock(blockhash)
        height = block['height']

        self.sync_all()
        #print("Player 1 Total Bet", player1_total_bet)
        #print("Player 2 Total Bet", player2_total_bet)

        payoutsInfo = self.nodes[0].getpayoutinfosince(1)

        check_bet_payouts_info(listbets, payoutsInfo)

        player1_balance_after = Decimal(self.nodes[2].getbalance())

        assert_equal(player1_balance_before + player1_expected_win, player1_balance_after)

        # return back
        self.start_time = int(time.time() + 60 * 60)

        self.log.info("Timecut Refund Success")

    def check_asian_spreads_bet(self):
        self.log.info("Check Asian Spread Bets...")

        global player1_total_bet
        global player2_total_bet

        # make new event, expected result is 1:0
        mlevent = make_event(9, # Event ID
                    self.start_time, # start time = current + hour
                    sport_names.index("Test Sport"), # Sport ID
                    tournament_names.index("Test Tournament"), # Tournament ID
                    round_names.index("round1"), # Round ID
                    team_names.index("Test Team1"), # Home Team
                    team_names.index("Test Team2"), # Away Team
                    15000, # home odds
                    18000, # away odds
                    13000) # draw odds

        post_opcode(self.nodes[1], mlevent, WGR_WALLET_EVENT['addr'])

        self.sync_all()
        self.nodes[0].generate(1)
        self.sync_all()

        # create asian spread event
        spread_event_opcode = make_spread_event(9, -125, 14000, 26000)
        post_opcode(self.nodes[1], spread_event_opcode, WGR_WALLET_EVENT['addr'])

        self.sync_all()
        self.nodes[0].generate(1)
        self.sync_all()

        # place spread bet to spread home
        # in our result it mean 50% bet lose, 50% bet refund
        player1_bet = 400
        player1_total_bet = player1_total_bet + player1_bet
        self.nodes[2].placebet(9, outcome_spread_home, player1_bet)
        winnings = Decimal(player1_bet * 0.5 * ODDS_DIVISOR)
        player1_expected_win = winnings / ODDS_DIVISOR

        # change spread condition for event
        spread_event_opcode = make_spread_event(9, -75, 13000, 27000)
        post_opcode(self.nodes[1], spread_event_opcode, WGR_WALLET_EVENT['addr'])

        self.sync_all()
        self.nodes[0].generate(1)
        self.sync_all()

        player2_bet = 600
        player2_total_bet = player2_total_bet + player2_bet
        # place spread bet to spread away
        # in our result it mean 50% bet lose, 50% bet refund
        self.nodes[3].placebet(9, outcome_spread_away, player2_bet)
        winnings = Decimal(player2_bet * 0.5 * ODDS_DIVISOR)
        player2_expected_win = winnings / ODDS_DIVISOR

        #self.log.info("Event Liability")
        #pprint.pprint(self.nodes[0].geteventliability(9))
        liability=self.nodes[0].geteventliability(9)
        gotliability=liability["spread-home-liability"]
        assert_equal(gotliability, Decimal(550))
        gotliability=liability["spread-push-liability"]
        assert_equal(gotliability, Decimal(400))

        # place result for event 9:
        result_opcode = make_result(9, STANDARD_RESULT, 100, 0)
        post_opcode(self.nodes[1], result_opcode, WGR_WALLET_EVENT['addr'])

        self.sync_all()
        self.nodes[0].generate(1)
        self.sync_all()

        player1_balance_before = Decimal(self.nodes[2].getbalance())
        player2_balance_before = Decimal(self.nodes[3].getbalance())

        # print("player1 balance before: ", player1_balance_before)
        # print("player1 exp win: ", player1_expected_win)
        # print("player2 balance before: ", player2_balance_before)
        # print("player2 exp win: ", player2_expected_win)

        # generate block with payouts
        blockhash = self.nodes[0].generate(1)[0]
        block = self.nodes[0].getblock(blockhash)

        self.sync_all()
        #print("Player 1 Total Bet", player1_total_bet)
        #print("Player 2 Total Bet", player2_total_bet)

        # print(pprint.pformat(block))

        player1_balance_after = Decimal(self.nodes[2].getbalance())
        player2_balance_after = Decimal(self.nodes[3].getbalance())

        # print("player1 balance after: ", player1_balance_after)
        # print("player2 balance after: ", player2_balance_after)

        assert_equal(player1_balance_before + player1_expected_win, player1_balance_after)
        assert_equal(player2_balance_before + player2_expected_win, player2_balance_after)

        self.log.info("Asian Spread Bets Success")


    #
    def check_v2_v3_bet(self):
        self.log.info("Check V2 to V3 Bets...")
        # generate so we get to block 300 after event creation & first round bets but before payout sent
        # change this number to change where generate block 300 takes place generate(26) is block 301 for payout
        self.nodes[0].generate(26)
        player1_expected_win = 0
        player2_expected_win = 0
        global player1_total_bet
        global player2_total_bet

        # make new event, expected result is 1:0
        self.odds_events = []
        mlevent = make_event(10, # Event ID
                    self.start_time, # start time = current + hour
                    sport_names.index("V2-V3 Sport"), # Sport ID
                    tournament_names.index("V2-V3 Tournament"), # Tournament ID
                    round_names.index("round1"), # Round ID
                    team_names.index("V2-V3 Team1"), # Home Team
                    team_names.index("V2-V3 Team2"), # Away Team
                    15000, # home odds
                    18000, # away odds
                    13000) # draw odds
        self.odds_events.append({'homeOdds': 15000, 'awayOdds': 18000, 'drawOdds': 13000})
        post_opcode(self.nodes[1], mlevent, WGR_WALLET_EVENT['addr'])

        self.sync_all()
        self.nodes[0].generate(1)
        self.sync_all()

        # create spread event
        spread_event_opcode = make_spread_event(10, -125, 14000, 26000)
        post_opcode(self.nodes[1], spread_event_opcode, WGR_WALLET_EVENT['addr'])

        # create totals
        totals_event_opcode = make_total_event(10, 26, 21000, 23000)
        post_opcode(self.nodes[1], totals_event_opcode, WGR_WALLET_EVENT['addr'])

        self.sync_all()
        self.nodes[0].generate(1)
        self.sync_all()
        #self.log.info("Events")
        #pprint.pprint(self.nodes[1].listevents())

        #events_before=self.nodes[0].listevents()
        #pprint.pprint(events_before[10])

        # Place Bet to ML Home Win
        player1_bet = 100
        player1_total_bet = player1_total_bet + player1_bet
        self.nodes[2].placebet(10, outcome_home_win, player1_bet)
        winnings = Decimal(player1_bet * self.odds_events[0]['homeOdds'])
        player1_expected_win = player1_expected_win + ((winnings - ((winnings - player1_bet * ODDS_DIVISOR) / 1000 * BETX_PERMILLE)) / ODDS_DIVISOR)

        # Place bet for total over win
        player1_bet = 200
        player1_total_bet = player1_total_bet + player1_bet
        self.nodes[2].placebet(10, outcome_total_over, player1_bet)
        winnings = Decimal(player1_bet * 21000)
        player1_expected_win = player1_expected_win + ((winnings - ((winnings - player1_bet * ODDS_DIVISOR) / 1000 * BETX_PERMILLE)) / ODDS_DIVISOR)

        # place spread bet to spread home
        # in our result it mean 50% bet lose, 50% bet refund
        player1_bet = 400
        player1_total_bet = player1_total_bet + player1_bet
        self.nodes[2].placebet(10, outcome_spread_home, player1_bet)
        winnings = Decimal(player1_bet * 0.5 * ODDS_DIVISOR)
        player1_expected_win = player1_expected_win + (winnings / ODDS_DIVISOR)

        # change spread condition for event 10
        spread_event_opcode = make_spread_event(10, -75, 13000, 27000)
        post_opcode(self.nodes[1], spread_event_opcode, WGR_WALLET_EVENT['addr'])

        self.log.info("Events before odds updating")
        pprint.pprint(self.nodes[0].listevents())

        # change odds for ML bet
        event_id = tournament_names.index("V2-V3 Tournament")
        self.log.info("Event %s" % event_id)
        pprint.pprint(self.odds_events)
        self.odds_events[0]['homeOdds'] = 14000
        self.odds_events[0]['awayOdds'] = 25000
        self.odds_events[0]['drawOdds'] = 31000
        update_odds_opcode = make_update_ml_odds(10,
                                                self.odds_events[0]['homeOdds'],
                                                self.odds_events[0]['awayOdds'],
                                                self.odds_events[0]['drawOdds'])
        post_opcode(self.nodes[1], update_odds_opcode, WGR_WALLET_EVENT['addr'])

        #self.odds_events.append({'homeOdds': 14000, 'awayOdds': 25000, 'drawOdds': 31000})
        self.sync_all()
        self.nodes[0].generate(1)
        self.sync_all()
        self.log.info("Events after updating")
        pprint.pprint(self.nodes[0].listevents())

        # Place Bet to ML Home Win
        player1_bet = 150
        player1_total_bet = player1_total_bet + player1_bet
        self.nodes[2].placebet(10, outcome_home_win, player1_bet)
        winnings = Decimal(player1_bet * self.odds_events[0]['homeOdds'])
        pprint.pprint(winnings)
        pprint.pprint(player1_expected_win)
        player1_expected_win = player1_expected_win + ((winnings - ((winnings - player1_bet * ODDS_DIVISOR) / 1000 * BETX_PERMILLE)) / ODDS_DIVISOR)

        self.sync_all()
        self.nodes[0].generate(1)
        self.sync_all()

        #should be block height 300
        self.log.info("Block Height %s" % self.nodes[0].getblockcount())
        player1_bet = 300
        player1_total_bet = player1_total_bet + player1_bet
        self.nodes[2].placebet(10, outcome_spread_away, player1_bet)
        winnings = Decimal(player1_bet * 0.5 * ODDS_DIVISOR)
        player1_expected_win = player1_expected_win + (winnings / ODDS_DIVISOR)

        player2_bet = 600
        player2_total_bet = player2_total_bet + player2_bet
        # place spread bet to spread away
        # in our result it mean 50% bet lose, 50% bet refund
        self.nodes[3].placebet(10, outcome_spread_away, player2_bet)
        winnings = Decimal(player2_bet * 0.5 * ODDS_DIVISOR)
        player2_expected_win = player2_expected_win + (winnings / ODDS_DIVISOR)

        # change odds for ML bet
        event_id = tournament_names.index("V2-V3 Tournament")
        self.log.info("Event %s" % event_id)
        pprint.pprint(self.odds_events)
        self.odds_events[0]['homeOdds'] = 16000
        self.odds_events[0]['awayOdds'] = 22000
        self.odds_events[0]['drawOdds'] = 41000
        update_odds_opcode = make_update_ml_odds(10,
                                                self.odds_events[0]['homeOdds'],
                                                self.odds_events[0]['awayOdds'],
                                                self.odds_events[0]['drawOdds'])
        post_opcode(self.nodes[1], update_odds_opcode, WGR_WALLET_EVENT['addr'])

        # Update totals odds
        totals_event_opcode = make_total_event(10, 26, 26000, 24000)
        post_opcode(self.nodes[1], totals_event_opcode, WGR_WALLET_EVENT['addr'])

        # Change spread event odds
        spread_event_opcode = make_spread_event(10, -22, 15000, 22000)
        post_opcode(self.nodes[1], spread_event_opcode, WGR_WALLET_EVENT['addr'])

        self.sync_all()
        self.nodes[0].generate(1)
        self.sync_all()

        # Place bet for total over win
        player2_bet = 300
        player2_total_bet = player2_total_bet + player2_bet
        self.nodes[3].placebet(10, outcome_total_over, player2_bet)
        winnings = Decimal(player2_bet * 26000)
        player2_expected_win = player2_expected_win + ((winnings - ((winnings - player2_bet * ODDS_DIVISOR) / 1000 * BETX_PERMILLE)) / ODDS_DIVISOR)

        # Place Bet to ML Home Win
        player1_bet = 225
        player1_total_bet = player1_total_bet + player1_bet
        self.nodes[2].placebet(10, outcome_home_win, player1_bet)
        winnings = Decimal(player1_bet * self.odds_events[0]['homeOdds'])
        player1_expected_win = player1_expected_win + ((winnings - ((winnings - player1_bet * ODDS_DIVISOR) / 1000 * BETX_PERMILLE)) / ODDS_DIVISOR)

        ##player2_bet = 400
        ##player2_total_bet = player2_total_bet + player2_bet
        # place spread bet to spread away
        # in our result it mean 50% bet lose, 50% bet refund
        ##self.nodes[3].placebet(10, outcome_spread_home, player2_bet)
        ##winnings = Decimal(player2_bet * 0.5 * ODDS_DIVISOR)
        ##player2_expected_win = player2_expected_win + (winnings / ODDS_DIVISOR)

        self.sync_all()
        self.nodes[0].generate(1)
        self.sync_all()

        # place result for event 10
        result_opcode = make_result(10, STANDARD_RESULT, 100, 0)
        post_opcode(self.nodes[1], result_opcode, WGR_WALLET_EVENT['addr'])

        self.sync_all()
        self.nodes[0].generate(1)
        self.sync_all()

        player1_balance_before = Decimal(self.nodes[2].getbalance())
        player2_balance_before = Decimal(self.nodes[3].getbalance())

        #print("player1 balance before: ", player1_balance_before)
        #print("player1 exp win: ", player1_expected_win)
        #print("player2 balance before: ", player2_balance_before)
        #print("player2 exp win: ", player2_expected_win)

        # generate block with payouts
        blockhash = self.nodes[0].generate(1)[0]
        block = self.nodes[0].getblock(blockhash)
        #should be block height 304
        #self.log.info("Block Height %s " % self.nodes[0].getblockcount())

        self.sync_all()
        #time.sleep(2000)
        #print("Player 1 Total Bet", player1_total_bet)
        #print("Player 2 Total Bet", player2_total_bet)

        # print(pprint.pformat(block))

        player1_balance_after = Decimal(self.nodes[2].getbalance())
        player2_balance_after = Decimal(self.nodes[3].getbalance())

        #print("player1 balance after: ", player1_balance_after)
        #print("Player 1 total bet: ", player1_total_bet)
        #print("player2 balance after: ", player2_balance_after)
        #print("Player 2 total bet: ", player2_total_bet)


        assert_equal(player1_balance_before + player1_expected_win, player1_balance_after)
        assert_equal(player2_balance_before + player2_expected_win, player2_balance_after)

        self.log.info("V2 to V3 Bets Success")

    def check_bets(self):
        self.log.info("Check Bets")
        #time.sleep(2000)
        betam1 = 0
        betpay1 = 0
        betam2 = 0
        betpay2 = 0
        for bets in range(self.num_nodes):
            if bets == 0:
                mybets=self.nodes[bets].getmybets()
                #self.log.info("Bets Node %d" % bets)
                assert_equal(mybets, [])
            elif bets == 1:
                mybets=self.nodes[bets].getmybets()
                #self.log.info("Bets Node %d" % bets)
                assert_equal(mybets, [])
            elif bets == 2:
                mybets=self.nodes[bets].getmybets("Node2Addr", 100)
                #self.log.info("Bets Node %d" % bets)
                #self.log.info("Bet length %d" % len(mybets))
                for bet in range(len(mybets)):
                    #self.log.info("Bet Result %s " % mybets[bet]['betResultType'])
                    betam1 = betam1 + mybets[bet]['amount']
                    #self.log.info("Bet Amount %d " % mybets[bet]['amount'])
                    #self.log.info("Bet Payout %d " % mybets[bet]['payout'])
                    betpay1 = betpay1 + mybets[bet]['payout']
            elif bets == 3:
                mybets=self.nodes[bets].getmybets("Node3Addr", 100)
                #self.log.info("Bets Node %d" % bets)
                #self.log.info("Bet length %d" % len(mybets))
                for bet in range(len(mybets)):
                    #self.log.info("Bet Result %s " % mybets[bet]['betResultType'])
                    #self.log.info("Bet Amount %d " % mybets[bet]['amount'])
                    betam2 = betam2 + mybets[bet]['amount']
                    #self.log.info("Bet Payout %d " % mybets[bet]['payout'])
                    betpay2 = betpay2 + mybets[bet]['payout']
            else:
                self.log.info("Too Many Nodes")

        #self.log.info("Total Amount Bet Player 1 %s" % player1_total_bet)
        assert_equal(betam1, player1_total_bet)
        #self.log.info("Total Amount Won Player 1 %s" % betpay1)
        assert_equal(round(Decimal(betpay1), 8), round(Decimal(2678.22000000), 8))
        #self.log.info("Total Amount Bet Player 2 %s" % player2_total_bet)
        assert_equal(betam2, player2_total_bet)
        #self.log.info("Total Amount Won Player 2 %s" % betpay2)
        assert_equal(round(Decimal(betpay2), 8), round(Decimal(3546.35000000), 8))

        self.log.info("Debug Events")
        pprint.pprint(self.nodes[0].listeventsdebug())

        self.log.info("All Bets")
        allbets=self.nodes[0].getallbets()
        #pprint.pprint(len(allbets))
        bettxid=allbets[0]["betTxHash"]

        self.log.info("Bet 0 by getbet")
        bet0=self.nodes[0].getbet(bettxid, True)
        pprint.pprint(bet0)
        self.log.info("Bet 0 by getbetbytxid")
        bet1=self.nodes[0].getbetbytxid(bettxid)
        pprint.pprint(bet1)
        assert_equal(bet0["amount"], bet1[0]["amount"])
        assert_equal(bet0["result"], bet1[0]["betResultType"])
        assert_equal(bet0["tx-id"], bet1[0]["betTxHash"])

        #time.sleep(2000)
        ###
        ## Not wroking TODO Fix it
        ###
        #self.log.info("Get Payout Info")
        #payinfo={"txhash":bettxid, "nOut":1}
        #pprint.pprint(self.nodes[0].getpayoutinfo([payinfo]))

        self.log.info("Check Bets Success")

    def run_test(self):
        self.check_minting()
        self.check_mapping()
        self.check_event()
        self.check_event_patch()
        self.check_update_odds()
        self.check_spread_event()
        self.check_spread_event_v2()
        self.check_total_event()
        self.check_ml_bet()
        # disable check spreads bets v1, becouse new spread system
        # uses spreads v1 before wagerr v3 prot, but regtest uses wagerr v3 prot
        # since first PoS block and we always have wagerr v3 prot
        # self.check_spreads_bet()
        self.check_totals_bet()
        # Chain Games are discontinued
        # self.check_chain_games()
        # not neeeded anymore
        # self.check_v2_v3_bet()
        self.check_spreads_bet_v2()
        self.check_parlays_bet()
        self.check_mempool_accept()
        self.check_timecut_refund()
        self.check_asian_spreads_bet()
        self.check_bets()

if __name__ == '__main__':
    BettingTest().main()
