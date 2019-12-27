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

WGR_WALLET_ORACLE = { "addr": "TXuoB9DNEuZx1RCfKw3Hsv7jNUHTt4sVG1", "key": "TBwvXbNNUiq7tDkR2EXiCbPxEJRTxA1i6euNyAE9Ag753w36c1FZ" }
WGR_WALLET_EVENT = { "addr": "TFvZVYGdrxxNunQLzSnRSC58BSRA7si6zu", "key": "TCDjD2i4e32kx2Fc87bDJKGBedEyG7oZPaZfp7E1PQG29YnvArQ8" }
WGR_WALLET_DEV = { "addr": "TLuTVND9QbZURHmtuqD5ESECrGuB9jLZTs", "key": "TFCrxaUt3EjHzMGKXeBqA7sfy3iaeihg5yZPSrf9KEyy4PHUMWVe" }
WGR_WALLET_OMNO = { "addr": "THofaueWReDjeZQZEECiySqV9GP4byP3qr", "key": "TDJnwRkSk8JiopQrB484Ny9gMcL1x7bQUUFFFNwJZmmWA7U79uRk" }

sport_names = ["Football", "MMA", "CSGO", "DOTA2"]
round_names = ["round1", "round2", "round3", "round4"]
tournament_names = ["UEFA Champions League", "UFC244", "PGL Major Krakow", "EPICENTER Major"]
team_names = ["Real Madrid", "Barcelona", "Jorge Masvidal", "Nate Diaz", "Astralis", "Gambit", "Virtus Pro", "Team Liquid"]

outcome_home_win = 1
outcome_away_win = 2
outcome_draw = 3
outcome_spread_home = 4
outcome_spread_away = 5
outcome_total_over = 6
outcome_total_under = 7

ODDS_DIVISOR = 10000
BETX_PERMILLE = 60

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

        self.players.append(self.nodes[2].getnewaddress())
        self.players.append(self.nodes[3].getnewaddress())

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

        self.nodes[0].generate(1)

        self.sync_all()

        for n in range(self.num_nodes):
            assert_equal( self.nodes[n].getblockcount(), block_count)

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

        # add sports to mapping
        for id in range(len(sport_names)):
            mapping_opcode = make_mapping(SPORT_MAPPING, id, sport_names[id])
            post_opcode(self.nodes[1], mapping_opcode, WGR_WALLET_ORACLE['addr'])
        # add rounds to mapping
        for id in range(len(round_names)):
            mapping_opcode = make_mapping(ROUND_MAPPING, id, round_names[id])
            post_opcode(self.nodes[1], mapping_opcode, WGR_WALLET_ORACLE['addr'])
        # add teams to mapping
        for id in range(len(team_names)):
            mapping_opcode = make_mapping(TEAM_MAPPING, id, team_names[id])
            post_opcode(self.nodes[1], mapping_opcode, WGR_WALLET_ORACLE['addr'])
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
                assert_equal(mapping['mapping-index'], SPORT_MAPPING)

            # Check rounds mapping
            for id in range(len(round_names)):
                mapping = node.getmappingname("rounds", id)[0]
                assert_equal(mapping['exists'], True)
                assert_equal(mapping['mapping-name'], round_names[id])
                assert_equal(mapping['mapping-index'], ROUND_MAPPING)

            # Check teams mapping
            for id in range(len(team_names)):
                mapping = node.getmappingname("teamnames", id)[0]
                assert_equal(mapping['exists'], True)
                assert_equal(mapping['mapping-name'], team_names[id])
                assert_equal(mapping['mapping-index'], TEAM_MAPPING)

            # Check tournaments mapping
            for id in range(len(tournament_names)):
                mapping = node.getmappingname("tournaments", id)[0]
                assert_equal(mapping['exists'], True)
                assert_equal(mapping['mapping-name'], tournament_names[id])
                assert_equal(mapping['mapping-index'], TOURNAMENT_MAPPING)

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

        for id in range(len(tournament_names)):
            event_patch = make_event_patch(id, self.start_time)
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
        # place bet to ml event 3: DOTA2 - EPICENTER Major - Virtus Pro vs Team Liquid
        # player 1 bet to Team Liquid with odds 17000
        player1_bet = 100
        self.nodes[2].placebet(3, outcome_away_win, player1_bet)
        winnings = Decimal(player1_bet * self.odds_events[3]['awayOdds'])
        player1_expected_win = (winnings - ((winnings - player1_bet * ODDS_DIVISOR) / 1000 * BETX_PERMILLE)) / ODDS_DIVISOR

        self.sync_all()
        self.nodes[0].generate(1)
        self.sync_all()

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

        #player 2 bet to Team Liquid with odds 14000
        player2_bet = 200
        self.nodes[3].placebet(3, outcome_away_win, player2_bet)
        winnings = Decimal(player2_bet * self.odds_events[3]['awayOdds'])
        player2_expected_win = (winnings - ((winnings - player2_bet * ODDS_DIVISOR) / 1000 * BETX_PERMILLE)) / ODDS_DIVISOR

        self.sync_all()
        self.nodes[0].generate(1)
        self.sync_all()

        # print("BETS:\n", pprint.pformat(self.nodes[0].listbetsdb(True)))

        # place result for event 3: Team Liquid wins.
        result_opcode = make_result(3, STANDARD_RESULT, 0, 1)
        post_opcode(self.nodes[1], result_opcode, WGR_WALLET_EVENT['addr'])

        self.sync_all()
        self.nodes[0].generate(1)
        self.sync_all()

        player1_balance_before = Decimal(self.nodes[2].getbalance())
        player2_balance_before = Decimal(self.nodes[3].getbalance())

        print("player1 balance before: ", player1_balance_before)
        print("player1 exp win: ", player1_expected_win)
        print("player2 balance before: ", player2_balance_before)
        print("player2 exp win: ", player2_expected_win)

        # generate block with payouts
        blockhash = self.nodes[0].generate(1)[0]
        block = self.nodes[0].getblock(blockhash)

        self.sync_all()

        # print(pprint.pformat(block))

        player1_balance_after = Decimal(self.nodes[2].getbalance())
        player2_balance_after = Decimal(self.nodes[3].getbalance())

        print("player1 balance after: ", player1_balance_after)
        print("player2 balance after: ", player2_balance_after)

        assert_equal(player1_balance_before + player1_expected_win, player1_balance_after)
        assert_equal(player2_balance_before + player2_expected_win, player2_balance_after)

        self.log.info("Money Line Bets Success")

    def check_spreads_bet(self):
        self.log.info("Check Spreads Bets...")

        # place spread bet to event 0: UEFA Champions League, expect that event result will be 2:0 for home team
        # player 1 bet to spread home, mean that home will win with spread points for away = 1
        player1_bet = 300
        self.nodes[2].placebet(0, outcome_spread_home, player1_bet)
        winnings = Decimal(player1_bet * 28000)
        player1_expected_win = (winnings - ((winnings - player1_bet * ODDS_DIVISOR) / 1000 * BETX_PERMILLE)) / ODDS_DIVISOR

        self.sync_all()
        self.nodes[0].generate(1)
        self.sync_all()

        # change spread condition for event 0
        spread_event_opcode = make_spread_event(0, 2, 23000, 15000)
        post_opcode(self.nodes[1], spread_event_opcode, WGR_WALLET_EVENT['addr'])

        self.sync_all()
        self.nodes[0].generate(1)
        self.sync_all()

        # player 2 bet to spread home, mean that home will win with spread points for away = 2
        # for our results it means refund
        player2_bet = 200
        self.nodes[3].placebet(0, outcome_spread_home, player2_bet)
        winnings = Decimal(player2_bet * ODDS_DIVISOR)
        player2_expected_win = (winnings - ((winnings - player2_bet * ODDS_DIVISOR) / 1000 * BETX_PERMILLE)) / ODDS_DIVISOR

        # place result for event 0: RM wins BARC with 2:0.
        result_opcode = make_result(0, STANDARD_RESULT, 2, 0)
        post_opcode(self.nodes[1], result_opcode, WGR_WALLET_EVENT['addr'])

        self.sync_all()
        self.nodes[0].generate(1)
        self.sync_all()

        player1_balance_before = Decimal(self.nodes[2].getbalance())
        player2_balance_before = Decimal(self.nodes[3].getbalance())

        print("player1 balance before: ", player1_balance_before)
        print("player1 exp win: ", player1_expected_win)
        print("player2 balance before: ", player2_balance_before)
        print("player2 exp win: ", player2_expected_win)

        # generate block with payouts
        blockhash = self.nodes[0].generate(1)[0]
        block = self.nodes[0].getblock(blockhash)

        self.sync_all()

        # print(pprint.pformat(block))

        player1_balance_after = Decimal(self.nodes[2].getbalance())
        player2_balance_after = Decimal(self.nodes[3].getbalance())

        print("player1 balance after: ", player1_balance_after)
        print("player2 balance after: ", player2_balance_after)

        assert_equal(player1_balance_before + player1_expected_win, player1_balance_after)
        assert_equal(player2_balance_before + player2_expected_win, player2_balance_after)

        self.log.info("Spreads Bets Success")

    def check_totals_bet(self):
        self.log.info("Check Totals Bets...")
        # place spread bet to event 2: PGL Major Krakow
        # player 1 bet to total over with odds 21000
        player1_bet = 200
        self.nodes[2].placebet(2, outcome_total_over, player1_bet)
        winnings = Decimal(player1_bet * 21000)
        player1_expected_win = (winnings - ((winnings - player1_bet * ODDS_DIVISOR) / 1000 * BETX_PERMILLE)) / ODDS_DIVISOR

        self.sync_all()
        self.nodes[0].generate(1)
        self.sync_all()

        # change totals condition for event 2
        spread_event_opcode = make_total_event(2, 28, 28000, 17000)
        post_opcode(self.nodes[1], spread_event_opcode, WGR_WALLET_EVENT['addr'])

        self.sync_all()
        self.nodes[0].generate(1)
        self.sync_all()

        # player 2 bet to total under with odds 17000
        player2_bet = 200
        self.nodes[3].placebet(2, outcome_total_under, player2_bet)
        winnings = Decimal(player2_bet * 17000)
        player2_expected_win = (winnings - ((winnings - player2_bet * ODDS_DIVISOR) / 1000 * BETX_PERMILLE)) / ODDS_DIVISOR

        # place result for event 2: Gambit wins with score 11:16
        result_opcode = make_result(2, STANDARD_RESULT, 11, 16)
        post_opcode(self.nodes[1], result_opcode, WGR_WALLET_EVENT['addr'])

        self.sync_all()
        self.nodes[0].generate(1)
        self.sync_all()

        player1_balance_before = Decimal(self.nodes[2].getbalance())
        player2_balance_before = Decimal(self.nodes[3].getbalance())

        print("player1 balance before: ", player1_balance_before)
        print("player1 exp win: ", player1_expected_win)
        print("player2 balance before: ", player2_balance_before)
        print("player2 exp win: ", player2_expected_win)

        # generate block with payouts
        blockhash = self.nodes[0].generate(1)[0]
        block = self.nodes[0].getblock(blockhash)

        self.sync_all()

        # print(pprint.pformat(block))

        player1_balance_after = Decimal(self.nodes[2].getbalance())
        player2_balance_after = Decimal(self.nodes[3].getbalance())

        print("player1 balance: ", player1_balance_after)
        print("player2 balance: ", player2_balance_after)

        assert_equal(player1_balance_before + player1_expected_win, player1_balance_after)
        assert_equal(player2_balance_before + player2_expected_win, player2_balance_after)

        self.log.info("Totals Bets Success")

    def check_parlays_bet(self):
        self.log.info("Check Parlay Bets...")
        # add new events
        # 4: CSGO - PGL Major Krakow - Astralis vs Gambit round2
        mlevent = make_event(4, # Event ID
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
        mlevent = make_event(5, # Event ID
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
        mlevent = make_event(6, # Event ID
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
        winnings = Decimal(player1_bet * 27440)
        player1_expected_win = (winnings - ((winnings - player1_bet * ODDS_DIVISOR) / 1000 * BETX_PERMILLE)) / ODDS_DIVISOR
        self.nodes[2].placeparlaybet([{'eventId': 4, 'outcome': outcome_home_win}, {'eventId': 5, 'outcome': outcome_home_win}, {'eventId': 6, 'outcome': outcome_home_win}], player1_bet)

        # player 2 make express to events 4, 5, 6 - home win
        player2_bet = 500
        winnings = Decimal(player2_bet * 27440)
        player2_expected_win = (winnings - ((winnings - player2_bet * ODDS_DIVISOR) / 1000 * BETX_PERMILLE)) / ODDS_DIVISOR
        self.nodes[3].placeparlaybet([{'eventId': 4, 'outcome': outcome_home_win}, {'eventId': 5, 'outcome': outcome_home_win}, {'eventId': 6, 'outcome': outcome_home_win}], player2_bet)

        self.sync_all()
        self.nodes[0].generate(1)
        self.sync_all()

        # place result for event 4: Astralis wins with score 16:9
        result_opcode = make_result(4, STANDARD_RESULT, 16, 9)
        post_opcode(self.nodes[1], result_opcode, WGR_WALLET_EVENT['addr'])
        # place result for event 5: Astralis wins with score 16:14
        result_opcode = make_result(5, STANDARD_RESULT, 16, 14)
        post_opcode(self.nodes[1], result_opcode, WGR_WALLET_EVENT['addr'])
        # place result for event 6: Barcelona wins with score 3:2
        result_opcode = make_result(6, STANDARD_RESULT, 3, 2)
        post_opcode(self.nodes[1], result_opcode, WGR_WALLET_EVENT['addr'])

        self.sync_all()
        self.nodes[0].generate(1)
        self.sync_all()

        player1_balance_before = Decimal(self.nodes[2].getbalance())
        player2_balance_before = Decimal(self.nodes[3].getbalance())

        print("player1 balance before: ", player1_balance_before)
        print("player1 exp win: ", player1_expected_win)
        print("player2 balance before: ", player2_balance_before)
        print("player2 exp win: ", player2_expected_win)

        # generate block with payouts
        blockhash = self.nodes[0].generate(1)[0]
        block = self.nodes[0].getblock(blockhash)

        self.sync_all()

        # print(pprint.pformat(block))

        player1_balance_after = Decimal(self.nodes[2].getbalance())
        player2_balance_after = Decimal(self.nodes[3].getbalance())

        print("player1 balance: ", player1_balance_after)
        print("player2 balance: ", player2_balance_after)

        assert_equal(player1_balance_before + player1_expected_win, player1_balance_after)
        assert_equal(player2_balance_before + player2_expected_win, player2_balance_after)

        self.log.info("Parlay Bets Success")

    def run_test(self):
        self.check_minting()
        self.check_mapping()
        self.check_event()
        self.check_event_patch()
        self.check_update_odds()
        self.check_spread_event()
        self.check_total_event()
        self.check_ml_bet()
        self.check_spreads_bet()
        self.check_totals_bet()
        self.check_parlays_bet()

if __name__ == '__main__':
    BettingTest().main()
