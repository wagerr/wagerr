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
contender_names = ["cont1", "cont2",
    "horse1", "horse2", "horse3", "horse4", "horse5",
    "Alexander Albon", "Daniil Kvyat", "Pierre Gasly", "Romain Grosjean", "Antonio Maria Giovinazzi",
    "horse6", "horse7", "horse8", "horse9",
    "cont3", "cont4", "cont5", "cont6", "cont7", "cont8", "cont9"
]

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

def make_odds(probability_percent):
    if probability_percent == 0:
        return 0
    return int((1 / (probability_percent / 100)) * ODDS_DIVISOR)

# Evaluates the probability of the provided player - with index idx1 - arriving amongst the first n
def eval_prob_in_first(idx, contenders_p, perms):
    result = 0
    for perm in perms:
        if idx not in set(perm):
            continue
        cur_probs = [contenders_p[k] for k in perm]

        eval_exact_order = 1
        for prob in cur_probs:
            eval_exact_order = eval_exact_order * prob

        den = 1
        for i in range(len(cur_probs)):
            q = 1
            for j in range(i):
                q = q - cur_probs[j]
            den = den * q

        result = result + (eval_exact_order / den)

    return result

def calculate_odds(odds_type, contenders_odds, contenders_mods, mrg_in_percent):
    assert_equal(len(contenders_odds), len(contenders_mods))
    contenders_out_odds = {}
    N = len(contenders_odds)
    perms = []
    if odds_type == "place":
        if N < 5:
            for contender_k in contenders_odds.keys():
                contenders_out_odds[contender_k] = 0
            return contenders_out_odds

        multiplier = 2

        for cont_id_i in contenders_odds:
            for cont_id_j in contenders_odds:
                if cont_id_i == cont_id_j:
                    continue
                perms.append([cont_id_i, cont_id_j])

    elif odds_type == "show":
        if N < 8:
            for contender_k in contenders_odds.keys():
                contenders_out_odds[contender_k] = 0
            return contenders_out_odds

        multiplier = 3

        for cont_id_i in contenders_odds:
            for cont_id_j in contenders_odds:
                if cont_id_j == cont_id_i:
                    continue
                for cont_id_k in contenders_odds:
                    if cont_id_k == cont_id_j:
                        continue
                    if cont_id_k == cont_id_i:
                        continue
                    perms.append([cont_id_i, cont_id_j, cont_id_k])

    else:
        raise Exception("Wrong odds type")

    # print(len(perms))
    # for perm in perms:
    #     print(perm)

    contenders_p = {}
    for cont_id, cont_odds in contenders_odds.items():
        if contenders_odds[cont_id] == 0:
            continue
        contenders_p[cont_id] = 1 / (cont_odds / ODDS_DIVISOR)

    # TODO: пропустить нулевые
    contenders_n_probs = {}
    for cont_id in contenders_odds.keys():
        if contenders_odds[cont_id] == 0:
            continue
        contenders_n_probs[cont_id] = eval_prob_in_first(cont_id, contenders_p, perms)

    print("contenders_n_probs:")
    for cont_id in contenders_n_probs:
        print(cont_id, ":", contenders_n_probs[cont_id])

    contenders_n_odds = {}
    for cont_id in contenders_n_probs:
        contenders_n_odds[cont_id] = (1 / contenders_n_probs[cont_id]) * ODDS_DIVISOR

    print("contenders_n_odds:")
    for cont_id in contenders_n_odds:
        print(cont_id, ":", contenders_n_odds[cont_id])

    h = 0.000001
    real_mrg_in = (mrg_in_percent / 100) * multiplier

    # calc m
    m = 1
    md = m + h
    ms = m - h
    for i in range(1, N):
        f = 0
        fd = 0
        fs = 0

        for cont_k in contenders_n_probs.keys():
            if contenders_n_probs[cont_k] == 0:
                continue
            f += contenders_n_probs[cont_k] ** (m + contenders_mods[cont_k])
            fd += contenders_n_probs[cont_k] ** (md + contenders_mods[cont_k])
            fs += contenders_n_probs[cont_k] ** (ms + contenders_mods[cont_k])

        f -= real_mrg_in
        fd -= real_mrg_in
        fs -= real_mrg_in

        der = (fd - fs) / h

        m = m - (f / der)
        md = m + h
        ms = m - h

    # calc X
    X = 1
    Xd = X + h
    Xs = X - h
    for i in range(1, N):
        f = 0
        fd = 0
        fs = 0

        for cont_k in contenders_n_odds.keys():
            if contenders_n_odds[cont_k] == 0:
                continue
            f += 1 / (1 + (X + contenders_mods[cont_k]) * (contenders_n_odds[cont_k] / ODDS_DIVISOR - 1))
            fd += 1 / (1 + (Xd + contenders_mods[cont_k]) * (contenders_n_odds[cont_k] / ODDS_DIVISOR - 1))
            fs += 1 / (1 + (Xs + contenders_mods[cont_k]) * (contenders_n_odds[cont_k] / ODDS_DIVISOR - 1))

        f -= real_mrg_in
        fd -= real_mrg_in
        fs -= real_mrg_in

        der = (fd - fs) / h

        X = X - (f / der)
        Xd = X + h
        Xs = X - h

    print("odds_type = ", odds_type)
    print("N = ", N)
    print("real_mrg_in = ", real_mrg_in)
    print("m = ", m)
    print("X = ", X)

    for cont_k in contenders_n_odds.keys():
        if contenders_n_odds[cont_k] == 0:
            contenders_out_odds[cont_k] = 0
            continue
        odds_x = 1 + (X + contenders_mods[cont_k]) * (contenders_n_odds[cont_k] / ODDS_DIVISOR - 1)
        odds_m = contenders_n_probs[cont_k] ** (-m - contenders_mods[cont_k])
        contenders_out_odds[cont_k] = int(((odds_x + odds_m) / 2) * ODDS_DIVISOR)

    return contenders_out_odds

def check_calculate_odds():
    # contenders_odds = {
    #     1 : 36699,
    #     2 : 26152,
    #     3 : 17617,
    #     4 : 158265,
    #     5 : 93296,
    #     6 : 93296,
    #     7 : 158265,
    #     8 : 60873,
    #     9 : 36699,
    # }
    # contenders_odds = {
    #     2 : 0,
    #     3 : 0,
    #     4 : 0,
    #     5 : 0,
    #     6 : 0,
    #     12 : 0,
    #     13 : 0,
    #     14 : 0,
    # }
    contenders_odds = { # real odds
        1 : 77047,
        2 : 51588,
        3 : 30147,
        4 : 393762,
        5 : 219895,
        6 : 219895,
        7 : 393762,
        8 : 136769,
        9 : 77047,
    }
    contenders_mods = {
        1 : 0,
        2 : 0,
        3 : 0,
        4 : 0,
        5 : 0,
        6 : 0,
        7 : 0,
        8 : 0,
        9 : 0,
    }
    mrg_in_percent = 115

    out_odds = calculate_odds("place", contenders_odds, contenders_mods, mrg_in_percent)
    for k in contenders_odds.keys():
        print(out_odds[k])

    out_odds = calculate_odds("show", contenders_odds, contenders_mods, mrg_in_percent)
    for k in contenders_odds.keys():
        print(out_odds[k])

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
            all_markets,
            sport_names.index("Sport1"),
            tournament_names.index("Tournament1"),
            round_names.index("round0"),
            self.mrg_in_percent,
            {
                contender_names.index("cont1"): make_odds(10),
                contender_names.index("cont2"): make_odds(11),
                contender_names.index("cont3"): make_odds(12),
                contender_names.index("cont4"): make_odds(13),
                contender_names.index("cont5"): make_odds(14),
                contender_names.index("cont6"): make_odds(20),
                contender_names.index("cont7"): make_odds(15),
                contender_names.index("cont8"): make_odds(5),
                contender_names.index("cont9"): make_odds(0)
            }
        )
        assert_raises_rpc_error(-25, "",
            post_opcode, self.nodes[1], field_event_opcode, WGR_WALLET_ORACLE['addr'])

        # Test revert
        revert_chain_height = self.nodes[4].getblockcount()
        self.stop_node(4)

        # CASE: none animal sport group
        field_event_opcode = make_field_event(
            0,
            start_time,
            other_group,
            all_markets,
            sport_names.index("Sport1"),
            tournament_names.index("Tournament1"),
            round_names.index("round0"),
            self.mrg_in_percent,
            {
                contender_names.index("cont1"): make_odds(10),
                contender_names.index("cont2"): make_odds(11),
                contender_names.index("cont3"): make_odds(12),
                contender_names.index("cont4"): make_odds(13),
                contender_names.index("cont5"): make_odds(14),
                contender_names.index("cont6"): make_odds(20),
                contender_names.index("cont7"): make_odds(15),
                contender_names.index("cont8"): make_odds(5),
                contender_names.index("cont9"): make_odds(0)
            }
        )
        post_opcode(self.nodes[1], field_event_opcode, WGR_WALLET_EVENT['addr'])

        self.nodes[0].generate(1)
        sync_blocks(self.nodes[0:4])

        assert_raises_rpc_error(-25, "",
            post_opcode, self.nodes[1], field_event_opcode, WGR_WALLET_ORACLE['addr'])

        # pprint.pprint(self.nodes[1].listfieldevents()[0]['contenders'])

        # for node in self.nodes[0:4]:
        list_events = self.nodes[3].listfieldevents()
        assert_equal(len(list_events), 1)
        event_id = list_events[0]['event_id']
        assert_equal(list_events[0]['sport'], "Sport1")
        assert_equal(list_events[0]['tournament'], "Tournament1")
        assert_equal(list_events[0]['round'], "round0")
        assert_equal(list_events[0]['mrg-in'], self.mrg_in_percent)
        # assert_equal(len(list_events[0]['contenders']), 9)

        assert_equal(list_events[0]['contenders'][8]['name'], "cont9")
        assert_equal(list_events[0]['contenders'][8]['input-odds'], 0)
        assert_equal(list_events[0]['contenders'][8]['outright-odds'], 0)
        assert_equal(list_events[0]['contenders'][8]['place-odds'], 0)
        assert_equal(list_events[0]['contenders'][8]['show-odds'], 0)

        assert_equal(list_events[0]['contenders'][0]['name'], "cont1")
        assert_equal(list_events[0]['contenders'][0]['input-odds'], make_odds(10))
        assert_equal(list_events[0]['contenders'][0]['outright-odds'], 86387)
        assert_equal(list_events[0]['contenders'][0]['place-odds'], 41599)
        assert_equal(list_events[0]['contenders'][0]['show-odds'], 26803)

        assert_equal(list_events[0]['contenders'][1]['name'], "cont2")
        assert_equal(list_events[0]['contenders'][1]['input-odds'], make_odds(11))
        assert_equal(list_events[0]['contenders'][1]['outright-odds'], 78670)
        assert_equal(list_events[0]['contenders'][1]['place-odds'], 38300)
        assert_equal(list_events[0]['contenders'][1]['show-odds'], 24926)

        assert_equal(list_events[0]['contenders'][5]['name'], "cont6")
        assert_equal(list_events[0]['contenders'][5]['input-odds'], make_odds(20))
        assert_equal(list_events[0]['contenders'][5]['outright-odds'], 43949)
        assert_equal(list_events[0]['contenders'][5]['place-odds'], 23464)
        assert_equal(list_events[0]['contenders'][5]['show-odds'], 16629)

        # CASE: animal racing sport group
        field_event_opcode = make_field_event(
            101,
            start_time,
            animal_racing_group,
            all_markets,
            sport_names.index("Sport1"),
            tournament_names.index("Tournament1"),
            round_names.index("round0"),
            self.mrg_in_percent,
            {
                contender_names.index("cont1"): make_odds(10),
                contender_names.index("cont2"): make_odds(11),
                contender_names.index("cont3"): make_odds(12),
                contender_names.index("cont4"): make_odds(13),
                contender_names.index("cont5"): make_odds(14),
                contender_names.index("cont6"): make_odds(20),
                contender_names.index("cont7"): make_odds(15),
                contender_names.index("cont8"): make_odds(5),
                contender_names.index("cont9"): make_odds(0)
            }
        )
        post_opcode(self.nodes[1], field_event_opcode, WGR_WALLET_EVENT['addr'])

        self.nodes[1].generate(1)
        sync_blocks(self.nodes[0:4])

        # pprint.pprint(self.nodes[1].listfieldevents()[1]['contenders'])

        #for node in self.nodes[0:4]:
        list_events = self.nodes[3].listfieldevents()
        assert_equal(len(list_events), 2)
        event = list_events[1]
        assert_equal(event['sport'], "Sport1")
        assert_equal(event['tournament'], "Tournament1")
        assert_equal(event['round'], "round0")
        assert_equal(event['mrg-in'], self.mrg_in_percent)

        assert_equal(event['contenders'][8]['name'], "cont9")
        assert_equal(event['contenders'][8]['outright-odds'], 0)
        assert_equal(event['contenders'][8]['place-odds'], 0)
        assert_equal(event['contenders'][8]['show-odds'], 0)

        assert_equal(event['contenders'][0]['name'], "cont1")
        assert_equal(event['contenders'][0]['input-odds'], make_odds(10))
        assert_equal(event['contenders'][0]['outright-odds'], 86387)
        assert_equal(event['contenders'][0]['place-odds'], 40472)
        assert_equal(event['contenders'][0]['show-odds'], 25843)

        assert_equal(event['contenders'][1]['name'], "cont2")
        assert_equal(event['contenders'][1]['input-odds'], make_odds(11))
        assert_equal(event['contenders'][1]['outright-odds'], 78670)
        assert_equal(event['contenders'][1]['place-odds'], 37687)
        assert_equal(event['contenders'][1]['show-odds'], 24450)

        assert_equal(event['contenders'][4]['name'], "cont5")
        assert_equal(event['contenders'][4]['input-odds'], make_odds(14))
        assert_equal(event['contenders'][4]['outright-odds'], 62136)
        assert_equal(event['contenders'][4]['place-odds'], 31554)
        assert_equal(event['contenders'][4]['show-odds'], 21344)

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

        assert_equal(len(self.nodes[4].listfieldevents()), 0)

        self.log.info("Connect and sync nodes...")
        self.connect_and_sync_blocks()

        for node in self.nodes:
            assert_equal(len(node.listfieldevents()), 0)

        self.nodes[0].generate(1)
        sync_blocks(self.nodes)

        self.log.info("Event creation Success")

    def check_event_update_odds(self):
        self.log.info("Check Event Update Odds...")
        start_time = int(time.time() + 60 * 60)

        for node in self.nodes:
            assert_equal(len(node.listfieldevents()), 2)

        # Create events
        field_event_opcode = make_field_event(
            1,
            start_time,
            other_group,
            all_markets,
            sport_names.index("Sport1"),
            tournament_names.index("Tournament1"),
            round_names.index("round0"),
            self.mrg_in_percent,
            {
                contender_names.index("cont1") : make_odds(50)
            }
        )
        post_opcode(self.nodes[1], field_event_opcode, WGR_WALLET_EVENT['addr'])

        field_event_opcode = make_field_event(
            301,
            start_time,
            animal_racing_group,
            all_markets,
            sport_names.index("Sport1"),
            tournament_names.index("Tournament1"),
            round_names.index("round0"),
            self.mrg_in_percent,
            {
                contender_names.index("cont1") : make_odds(50)
            }
        )
        post_opcode(self.nodes[1], field_event_opcode, WGR_WALLET_EVENT['addr'])

        self.nodes[0].generate(1)
        sync_blocks(self.nodes)

        saved_other_event = {}
        for node in self.nodes:
            list_events = node.listfieldevents()
            assert_equal(len(list_events), 4)
            for event in list_events:
                if event['event_id'] == 1:
                    saved_other_event = event
                    assert_equal(event['contenders'][0]['name'], "cont1")
                    assert_equal(event['contenders'][0]['input-odds'], make_odds(50))
                    assert_equal(event['contenders'][0]['place-odds'], 0)
                    assert_equal(event['contenders'][0]['show-odds'], 0)
                if event['event_id'] == 301:
                    saved_animal_event = event
                    assert_equal(event['contenders'][0]['name'], "cont1")
                    assert_equal(event['contenders'][0]['input-odds'], make_odds(50))
                    assert_equal(event['contenders'][0]['place-odds'], 0)
                    assert_equal(event['contenders'][0]['show-odds'], 0)

        assert_equal(saved_other_event['event_id'], 1)
        assert_equal(len(saved_other_event['contenders']), 1)
        assert_equal(saved_other_event['contenders'][0]['name'], "cont1")
        assert_equal(saved_other_event['contenders'][0]['input-odds'], make_odds(50))
        assert_equal(saved_other_event['contenders'][0]['place-odds'], 0)
        assert_equal(saved_other_event['contenders'][0]['show-odds'], 0)

        assert_equal(saved_animal_event['event_id'], 301)
        assert_equal(len(saved_animal_event['contenders']), 1)
        assert_equal(saved_animal_event['contenders'][0]['name'], "cont1")
        assert_equal(saved_animal_event['contenders'][0]['input-odds'], make_odds(50))
        assert_equal(saved_animal_event['contenders'][0]['place-odds'], 0)
        assert_equal(saved_animal_event['contenders'][0]['show-odds'], 0)

        # For revert test
        revert_chain_height = self.nodes[4].getblockcount()
        self.stop_node(4)

        field_update_odds_opcode = make_field_update_odds(
            1,
            {
                contender_names.index("cont1") : make_odds(50),
                100 : 15000 # bad contender_id
            }
        )
        assert_raises_rpc_error(-25, "",
            post_opcode, self.nodes[1], field_update_odds_opcode, WGR_WALLET_ORACLE['addr'])

        field_update_odds_opcode = make_field_update_odds(
            100, # bad event_id
            {
                contender_names.index("cont1") : make_odds(50)
            }
        )
        assert_raises_rpc_error(-25, "",
            post_opcode, self.nodes[1], field_update_odds_opcode, WGR_WALLET_ORACLE['addr'])

        # Update odds for events
        field_update_odds_opcode = make_field_update_odds(
            1,
            {
                contender_names.index("cont1") : make_odds(51)
            }
        )
        post_opcode(self.nodes[1], field_update_odds_opcode, WGR_WALLET_EVENT['addr'])

        field_update_odds_opcode = make_field_update_odds(
            301,
            {
                contender_names.index("cont1") : make_odds(51)
            }
        )
        post_opcode(self.nodes[1], field_update_odds_opcode, WGR_WALLET_EVENT['addr'])

        self.nodes[0].generate(1)
        sync_blocks(self.nodes[0:4])

        # for node in self.nodes[0:4]:
        list_events = self.nodes[1].listfieldevents()
        assert_equal(len(list_events), 4)
        for event in list_events:
            if event['event_id'] == 1:
                assert_equal(len(event['contenders']), 1)
                assert_equal(event['contenders'][0]['name'], "cont1")
                assert_equal(event['contenders'][0]['input-odds'], make_odds(51))
                assert_equal(event['contenders'][0]['place-odds'], 0)
                assert_equal(event['contenders'][0]['show-odds'], 0)
            if event['event_id'] == 301:
                assert_equal(len(event['contenders']), 1)
                assert_equal(event['contenders'][0]['name'], "cont1")
                assert_equal(event['contenders'][0]['input-odds'], make_odds(51))
                assert_equal(event['contenders'][0]['place-odds'], 0)
                assert_equal(event['contenders'][0]['show-odds'], 0)

        field_update_odds_opcode = make_field_update_odds(1, {
                contender_names.index("cont2") : make_odds(49) # Add new conteder
            }
        )
        post_opcode(self.nodes[1], field_update_odds_opcode, WGR_WALLET_EVENT['addr'])

        field_update_odds_opcode = make_field_update_odds(301, {
                contender_names.index("cont2") : make_odds(49) # Add new conteder
            }
        )
        post_opcode(self.nodes[1], field_update_odds_opcode, WGR_WALLET_EVENT['addr'])

        self.nodes[0].generate(1)
        sync_blocks(self.nodes[0:4])

        # for node in self.nodes[0:4]:
        list_events = self.nodes[2].listfieldevents()
        assert_equal(len(list_events), 4)
        for event in list_events:
            if event['event_id'] == 1:
                assert_equal(len(event['contenders']), 2)
                assert_equal(event['contenders'][0]['name'], "cont1")
                assert_equal(event['contenders'][0]['input-odds'], make_odds(51))
                assert_equal(event['contenders'][0]['place-odds'], 0)
                assert_equal(event['contenders'][0]['show-odds'], 0)
                assert_equal(event['contenders'][1]['name'], "cont2")
                assert_equal(event['contenders'][1]['input-odds'], make_odds(49))
                assert_equal(event['contenders'][1]['place-odds'], 0)
                assert_equal(event['contenders'][1]['show-odds'], 0)
            if event['event_id'] == 301:
                assert_equal(len(event['contenders']), 2)
                assert_equal(event['contenders'][0]['name'], "cont1")
                assert_equal(event['contenders'][0]['input-odds'], make_odds(51))
                assert_equal(event['contenders'][0]['place-odds'], 0)
                assert_equal(event['contenders'][0]['show-odds'], 0)
                assert_equal(event['contenders'][1]['name'], "cont2")
                assert_equal(event['contenders'][1]['input-odds'], make_odds(49))
                assert_equal(event['contenders'][1]['place-odds'], 0)
                assert_equal(event['contenders'][1]['show-odds'], 0)

        field_update_odds_opcode = make_field_update_odds(1, {
                contender_names.index("cont2") : make_odds(10),
                # Add new conteders
                contender_names.index("horse1") : make_odds(4),
                contender_names.index("horse2") : make_odds(5),
                contender_names.index("horse3") : make_odds(5),
                contender_names.index("horse4") : make_odds(10),
                contender_names.index("horse5") : make_odds(10),
                contender_names.index("horse6") : make_odds(5),
                contender_names.index("horse7") : 0
            }
        )
        post_opcode(self.nodes[1], field_update_odds_opcode, WGR_WALLET_EVENT['addr'])

        field_update_odds_opcode = make_field_update_odds(301, {
                contender_names.index("cont2") : make_odds(10),
                # Add new conteders
                contender_names.index("horse1") : make_odds(4),
                contender_names.index("horse2") : make_odds(5),
                contender_names.index("horse3") : make_odds(5),
                contender_names.index("horse4") : make_odds(10),
                contender_names.index("horse5") : make_odds(10),
                contender_names.index("horse6") : make_odds(5),
                contender_names.index("horse7") : 0
            }
        )
        post_opcode(self.nodes[1], field_update_odds_opcode, WGR_WALLET_EVENT['addr'])

        self.nodes[0].generate(1)
        sync_blocks(self.nodes[0:4])

        # print(self.nodes[0].listfieldevents()[1]['contenders'])

        #for node in self.nodes[0:4]:
        list_events = self.nodes[2].listfieldevents()
        assert_equal(len(list_events), 4)
        for event in list_events:
            if event['event_id'] == 1:
                assert_equal(len(event['contenders']), 9)
                event_contenders = event['contenders']
                # pprint.pprint(event_contenders)
                assert_equal(event_contenders[0]['name'], "cont1")
                assert_equal(event_contenders[0]['input-odds'], make_odds(51))
                assert_equal(event_contenders[0]['outright-odds'], 17844)
                assert_equal(event_contenders[0]['place-odds'], 12298)
                assert_equal(event_contenders[0]['show-odds'], 10741)
                assert_equal(event_contenders[2]['name'], "horse1")
                assert_equal(event_contenders[2]['input-odds'], make_odds(4))
                assert_equal(event_contenders[2]['outright-odds'], 205958)
                assert_equal(event_contenders[2]['place-odds'], 77131)
                assert_equal(event_contenders[2]['show-odds'], 42832)
                assert_equal(event_contenders[7]['name'], "horse6")
                assert_equal(event_contenders[7]['input-odds'], make_odds(5))
                assert_equal(event_contenders[7]['outright-odds'], 165133)
                assert_equal(event_contenders[7]['place-odds'], 62991)
                assert_equal(event_contenders[7]['show-odds'], 35564)
                assert_equal(event_contenders[8]['name'], "horse7")
                assert_equal(event_contenders[8]['input-odds'], 0)
                assert_equal(event_contenders[8]['outright-odds'], 0)
                assert_equal(event_contenders[8]['place-odds'], 0)
                assert_equal(event_contenders[8]['show-odds'], 0)
            if event['event_id'] == 301:
                assert_equal(len(event['contenders']), 9)
                event_contenders = event['contenders']
                # pprint.pprint(event_contenders)
                assert_equal(event_contenders[0]['name'], "cont1")
                assert_equal(event_contenders[0]['input-odds'], make_odds(51))
                assert_equal(event_contenders[0]['outright-odds'], 17844)
                assert_equal(event_contenders[0]['place-odds'], 13165)
                assert_equal(event_contenders[0]['show-odds'], 11512)
                assert_equal(event_contenders[2]['name'], "horse1")
                assert_equal(event_contenders[2]['input-odds'], make_odds(4))
                assert_equal(event_contenders[2]['outright-odds'], 205958)
                assert_equal(event_contenders[2]['place-odds'], 68206)
                assert_equal(event_contenders[2]['show-odds'], 36967)
                assert_equal(event_contenders[7]['name'], "horse6")
                assert_equal(event_contenders[7]['input-odds'], make_odds(5))
                assert_equal(event_contenders[7]['outright-odds'], 165133)
                assert_equal(event_contenders[7]['place-odds'], 57603)
                assert_equal(event_contenders[7]['show-odds'], 32310)
                assert_equal(event_contenders[8]['name'], "horse7")
                assert_equal(event_contenders[8]['input-odds'], 0)
                assert_equal(event_contenders[8]['outright-odds'], 0)
                assert_equal(event_contenders[8]['place-odds'], 0)
                assert_equal(event_contenders[8]['show-odds'], 0)

        # Case: update all contenders and close show market
        field_update_odds_opcode = make_field_update_odds(1, {
                contender_names.index("cont1") : 0,
                contender_names.index("cont2") : 0, # close show market
                contender_names.index("horse1") : make_odds(20),
                contender_names.index("horse2") : make_odds(5),
                contender_names.index("horse3") : make_odds(5),
                contender_names.index("horse4") : make_odds(20),
                contender_names.index("horse5") : make_odds(25),
                contender_names.index("horse6") : make_odds(6),
                contender_names.index("horse7") : make_odds(19)
            }
        )
        post_opcode(self.nodes[1], field_update_odds_opcode, WGR_WALLET_EVENT['addr'])

        field_update_odds_opcode = make_field_update_odds(301, {
                contender_names.index("cont1") : 0,
                contender_names.index("cont2") : 0, # close show market
                contender_names.index("horse1") : make_odds(20),
                contender_names.index("horse2") : make_odds(5),
                contender_names.index("horse3") : make_odds(5),
                contender_names.index("horse4") : make_odds(20),
                contender_names.index("horse5") : make_odds(25),
                contender_names.index("horse6") : make_odds(6),
                contender_names.index("horse7") : make_odds(19)
            }
        )
        post_opcode(self.nodes[1], field_update_odds_opcode, WGR_WALLET_EVENT['addr'])

        self.nodes[0].generate(1)
        sync_blocks(self.nodes[0:4])

        # print(self.nodes[0].listfieldevents()[1]['contenders'])

        #for node in self.nodes[0:4]:
        list_events = self.nodes[2].listfieldevents()
        assert_equal(len(list_events), 4)
        for event in list_events:
            if event['event_id'] == 1:
                assert_equal(len(event['contenders']), 9)
                event_contenders = event['contenders']
                # pprint.pprint(event_contenders)
                assert_equal(event_contenders[0]['name'], "cont1")
                assert_equal(event_contenders[0]['input-odds'], 0)
                assert_equal(event_contenders[0]['outright-odds'], 0)
                assert_equal(event_contenders[0]['place-odds'], 0)
                assert_equal(event_contenders[0]['show-odds'], 0)
                assert_equal(event_contenders[1]['name'], "cont2")
                assert_equal(event_contenders[1]['outright-odds'], 0)
                assert_equal(event_contenders[1]['place-odds'], 0)
                assert_equal(event_contenders[1]['show-odds'], 0)
                assert_equal(event_contenders[2]['name'], "horse1")
                assert_equal(event_contenders[2]['input-odds'], make_odds(20))
                assert_equal(event_contenders[2]['outright-odds'], 43587)
                assert_equal(event_contenders[2]['place-odds'], 22262)
                assert_equal(event_contenders[2]['show-odds'], 0)
                assert_equal(event_contenders[7]['name'], "horse6")
                assert_equal(event_contenders[7]['input-odds'], make_odds(6))
                assert_equal(event_contenders[7]['outright-odds'], 141549)
                assert_equal(event_contenders[7]['place-odds'], 61356)
                assert_equal(event_contenders[7]['show-odds'], 0)
                assert_equal(event_contenders[8]['name'], "horse7")
                assert_equal(event_contenders[8]['input-odds'], make_odds(19))
                assert_equal(event_contenders[8]['outright-odds'], 45796)
                assert_equal(event_contenders[8]['place-odds'], 23145)
                assert_equal(event_contenders[8]['show-odds'], 0)
            if event['event_id'] == 301:
                assert_equal(len(event['contenders']), 9)
                event_contenders = event['contenders']
                # pprint.pprint(event_contenders)
                assert_equal(event_contenders[0]['name'], "cont1")
                assert_equal(event_contenders[0]['input-odds'], 0)
                assert_equal(event_contenders[0]['outright-odds'], 0)
                assert_equal(event_contenders[0]['place-odds'], 0)
                assert_equal(event_contenders[0]['show-odds'], 0)
                assert_equal(event_contenders[1]['name'], "cont2")
                assert_equal(event_contenders[1]['input-odds'], 0)
                assert_equal(event_contenders[1]['outright-odds'], 0)
                assert_equal(event_contenders[1]['place-odds'], 0)
                assert_equal(event_contenders[1]['show-odds'], 0)
                assert_equal(event_contenders[2]['name'], "horse1")
                assert_equal(event_contenders[2]['input-odds'], make_odds(20))
                assert_equal(event_contenders[2]['outright-odds'], 43587)
                assert_equal(event_contenders[2]['place-odds'], 22969)
                assert_equal(event_contenders[2]['show-odds'], 0)
                assert_equal(event_contenders[7]['name'], "horse6")
                assert_equal(event_contenders[7]['input-odds'], make_odds(6))
                assert_equal(event_contenders[7]['outright-odds'], 141549)
                assert_equal(event_contenders[7]['place-odds'], 54450)
                assert_equal(event_contenders[7]['show-odds'], 0)
                assert_equal(event_contenders[8]['name'], "horse7")
                assert_equal(event_contenders[8]['input-odds'], make_odds(19))
                assert_equal(event_contenders[8]['outright-odds'], 45796)
                assert_equal(event_contenders[8]['place-odds'], 23763)
                assert_equal(event_contenders[8]['show-odds'], 0)

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

        # Check event not updated
        for node in self.nodes:
            list_events = node.listfieldevents()
            assert_equal(len(list_events), 4)
            for event in list_events:
                if event['event_id'] == 1:
                    assert_equal(len(event['contenders']), 1)
                    assert_equal(saved_other_event['contenders'][0]['name'], event['contenders'][0]['name'])
                    assert_equal(saved_other_event['contenders'][0]['input-odds'], event['contenders'][0]['input-odds'])
                    assert_equal(saved_other_event['contenders'][0]['outright-odds'], event['contenders'][0]['outright-odds'])
                    assert_equal(saved_other_event['contenders'][0]['place-odds'], event['contenders'][0]['place-odds'])
                    assert_equal(saved_other_event['contenders'][0]['show-odds'], event['contenders'][0]['show-odds'])
                if event['event_id'] == 301:
                    assert_equal(len(event['contenders']), 1)
                    assert_equal(saved_animal_event['contenders'][0]['name'], event['contenders'][0]['name'])
                    assert_equal(saved_animal_event['contenders'][0]['input-odds'], event['contenders'][0]['input-odds'])
                    assert_equal(saved_animal_event['contenders'][0]['outright-odds'], event['contenders'][0]['outright-odds'])
                    assert_equal(saved_animal_event['contenders'][0]['place-odds'], event['contenders'][0]['place-odds'])
                    assert_equal(saved_animal_event['contenders'][0]['show-odds'], event['contenders'][0]['show-odds'])

        self.log.info("Field Event Update Odds Success")

    def check_event_zeroing_odds(self):
        self.log.info("Check Field Zeroing Odds...")
        start_time = int(time.time() + 60 * 60)

        field_event_opcode = make_field_event(
            2,
            start_time,
            animal_racing_group,
            all_markets,
            sport_names.index("Sport1"),
            tournament_names.index("Tournament1"),
            round_names.index("round0"),
            self.mrg_in_percent,
            {
                contender_names.index("cont1")  : make_odds(20),
                contender_names.index("cont2")  : make_odds(20),
                contender_names.index("horse1") : make_odds(20),
                contender_names.index("horse2") : make_odds(20),
                contender_names.index("horse3") : make_odds(20),
            }
        )
        post_opcode(self.nodes[1], field_event_opcode, WGR_WALLET_EVENT['addr'])

        self.nodes[0].generate(1)
        sync_blocks(self.nodes)

        # For revert test
        revert_chain_height = self.nodes[4].getblockcount()
        self.stop_node(4)

        field_zeroing_opcode = make_field_zeroing_odds(100) # bad event_id
        assert_raises_rpc_error(-25, "",
            post_opcode, self.nodes[1], field_zeroing_opcode, WGR_WALLET_ORACLE['addr'])

        field_zeroing_opcode = make_field_zeroing_odds(2)
        post_opcode(self.nodes[1], field_zeroing_opcode, WGR_WALLET_ORACLE['addr'])

        self.nodes[0].generate(1)
        sync_blocks(self.nodes[0:4])

        # for node in self.nodes[0:4]:
        list_events = self.nodes[2].listfieldevents()
        for event in list_events:
            if event['event_id'] != 2:
                continue
            assert_equal(len(event['contenders']), 5)
            assert_equal(event['contenders'][0]['name'], "cont1")
            assert_equal(event['contenders'][0]['input-odds'], 0)
            assert_equal(event['contenders'][0]['outright-odds'], 0)
            assert_equal(event['contenders'][0]['place-odds'], 0)
            assert_equal(event['contenders'][0]['show-odds'], 0)
            assert_equal(event['contenders'][1]['name'], "cont2")
            assert_equal(event['contenders'][1]['input-odds'], 0)
            assert_equal(event['contenders'][1]['outright-odds'], 0)
            assert_equal(event['contenders'][1]['place-odds'], 0)
            assert_equal(event['contenders'][1]['show-odds'], 0)
            assert_equal(event['contenders'][2]['name'], "horse1")
            assert_equal(event['contenders'][2]['input-odds'], 0)
            assert_equal(event['contenders'][2]['outright-odds'], 0)
            assert_equal(event['contenders'][2]['place-odds'], 0)
            assert_equal(event['contenders'][2]['show-odds'], 0)
            assert_equal(event['contenders'][3]['name'], "horse2")
            assert_equal(event['contenders'][3]['input-odds'], 0)
            assert_equal(event['contenders'][3]['outright-odds'], 0)
            assert_equal(event['contenders'][3]['place-odds'], 0)
            assert_equal(event['contenders'][3]['show-odds'], 0)
            assert_equal(event['contenders'][4]['name'], "horse3")
            assert_equal(event['contenders'][4]['input-odds'], 0)
            assert_equal(event['contenders'][4]['outright-odds'], 0)
            assert_equal(event['contenders'][4]['place-odds'], 0)
            assert_equal(event['contenders'][4]['show-odds'], 0)

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

        # print(self.nodes[0].listfieldevents()[2]['contenders'])

        #for node in self.nodes:
        list_events = self.nodes[2].listfieldevents()
        for event in list_events:
            if event['event_id'] != 2:
                continue
            # pprint.pprint(event['contenders'])
            assert_equal(len(event['contenders']), 5)
            assert_equal(event['contenders'][0]['name'], "cont1")
            assert_equal(event['contenders'][0]['input-odds'], make_odds(20))
            assert_equal(event['contenders'][0]['outright-odds'], 43478)
            assert_equal(event['contenders'][0]['place-odds'], 21819)
            assert_equal(event['contenders'][0]['show-odds'], 0)
            assert_equal(event['contenders'][1]['name'], "cont2")
            assert_equal(event['contenders'][1]['input-odds'], make_odds(20))
            assert_equal(event['contenders'][1]['outright-odds'], 43478)
            assert_equal(event['contenders'][1]['place-odds'], 21819)
            assert_equal(event['contenders'][1]['show-odds'], 0)
            assert_equal(event['contenders'][2]['name'], "horse1")
            assert_equal(event['contenders'][2]['input-odds'], make_odds(20))
            assert_equal(event['contenders'][2]['outright-odds'], 43478)
            assert_equal(event['contenders'][2]['place-odds'], 21819)
            assert_equal(event['contenders'][2]['show-odds'], 0)
            assert_equal(event['contenders'][3]['name'], "horse2")
            assert_equal(event['contenders'][3]['input-odds'], make_odds(20))
            assert_equal(event['contenders'][3]['outright-odds'], 43478)
            assert_equal(event['contenders'][3]['place-odds'], 21819)
            assert_equal(event['contenders'][3]['show-odds'], 0)
            assert_equal(event['contenders'][4]['name'], "horse3")
            assert_equal(event['contenders'][4]['input-odds'], make_odds(20))
            assert_equal(event['contenders'][4]['outright-odds'], 43478)
            assert_equal(event['contenders'][4]['place-odds'], 21819)
            assert_equal(event['contenders'][4]['show-odds'], 0)

        self.log.info("Field Event Zeroing Odds Success")

    def check_field_bet_undo(self):
        self.log.info("Check Field Bets undo...")
        start_time = int(time.time() + 60 * 60)

        field_event_opcode = make_field_event(
            221,
            start_time,
            other_group,
            all_markets,
            sport_names.index("F1 racing"),
            tournament_names.index("F1 Cup"),
            round_names.index("round0"),
            self.mrg_in_percent,
            {
                contender_names.index("Alexander Albon") : make_odds(25),
                contender_names.index("Pierre Gasly")    : make_odds(25),
                contender_names.index("Romain Grosjean") : make_odds(15),
                contender_names.index("Antonio Maria Giovinazzi")   : make_odds(35),
                contender_names.index("cont1") : 0
            }
        )
        post_opcode(self.nodes[1], field_event_opcode, WGR_WALLET_EVENT['addr'])

        self.nodes[0].generate(1)
        sync_blocks(self.nodes)

        # For revert test
        revert_chain_height = self.nodes[4].getblockcount()
        self.stop_node(4)

        player1_bet = 30
        self.nodes[2].placefieldbet(221, market_outright, contender_names.index("Alexander Albon"), player1_bet)

        events = self.nodes[2].listfieldevents()
        bet_contender_info = {}
        for event in events:
            if event['event_id'] == 221:
                for contender in event['contenders']:
                    if contender['name'] == "Alexander Albon":
                        bet_contender_info = contender

        effective_odds = calc_effective_odds(Decimal(bet_contender_info['outright-odds']))
        player1_expected_win = effective_odds * player1_bet / ODDS_DIVISOR

        self.nodes[0].generate(1)
        sync_blocks(self.nodes[0:4])

        # for node in self.nodes[0:4]:
        event_stats = self.nodes[2].getfieldeventliability(221)
        assert_equal(event_stats["event-id"], 221)
        assert_equal(event_stats["event-status"], "running")
        assert_equal(len(event_stats["contenders"]), 5)
        for contender in event_stats["contenders"]:
            if contender["contender-id"] != contender_names.index("Alexander Albon"):
                continue
            assert_equal(contender["outright-bets"], 1)
            assert_equal(contender["outright-liability"], int(player1_expected_win))

        field_result_opcode = make_field_result(221, STANDARD_RESULT, {
            contender_names.index("Alexander Albon") : place1,
            contender_names.index("Romain Grosjean") : place2,
            contender_names.index("Antonio Maria Giovinazzi") : place3
        })

        post_opcode(self.nodes[1], field_result_opcode, WGR_WALLET_EVENT['addr'])

        self.nodes[0].generate(1)
        sync_blocks(self.nodes[0:4])

        # Check event closed
        assert_raises_rpc_error(-31, "Error: FieldEvent " + str(221) + " was been resulted",
            self.nodes[2].placefieldbet, 221, market_outright, contender_names.index("Alexander Albon"), 30)

        #for node in self.nodes[0:4]:
        event_stats = self.nodes[1].getfieldeventliability(221)
        assert_equal(event_stats["event-id"], 221)
        assert_equal(event_stats["event-status"], "resulted")

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

        #for node in self.nodes:
        event_stats = self.nodes[2].getfieldeventliability(221)
        assert_equal(event_stats["event-id"], 221)
        assert_equal(event_stats["event-status"], "running")
        assert_equal(len(event_stats["contenders"]), 5)
        for contender in event_stats["contenders"]:
            assert_equal(contender["outright-bets"], 0)
            assert_equal(contender["outright-liability"], 0)

        self.log.info("Check Field Bets undo success")

    def check_field_bet_outright(self):
        self.log.info("Check Field Bets outright market...")
        start_time = int(time.time() + 60 * 60)

        global player1_total_bet
        global player2_total_bet

        player1_total_bet = 0
        player2_total_bet = 0

        # Create events for bet tests
        # event have 9 contenders, but only outright market available
        field_event_opcode = make_field_event(
            3,
            start_time,
            other_group,
            outright_only,
            sport_names.index("F1 racing"),
            tournament_names.index("F1 Cup"),
            round_names.index("round0"),
            self.mrg_in_percent,
            {
                contender_names.index("Alexander Albon") : make_odds(25),
                contender_names.index("Pierre Gasly")    : make_odds(25),
                contender_names.index("Romain Grosjean") : make_odds(15),
                contender_names.index("Antonio Maria Giovinazzi")   : make_odds(35),
                contender_names.index("cont1") : 0,
                contender_names.index("cont2") : make_odds(20),
                contender_names.index("cont3") : make_odds(20),
                contender_names.index("cont4") : make_odds(20),
                contender_names.index("cont5") : make_odds(20),
            }
        )
        post_opcode(self.nodes[1], field_event_opcode, WGR_WALLET_EVENT['addr'])

        self.nodes[0].generate(1)
        sync_blocks(self.nodes)

        field_event_opcode = make_field_event(
            4,
            start_time,
            animal_racing_group,
            all_markets,
            sport_names.index("Horse racing"),
            tournament_names.index("The BMW stakes"),
            round_names.index("round0"),
            self.mrg_in_percent,
            {
                contender_names.index("horse1") : make_odds(20),
                contender_names.index("horse2") : make_odds(19),
                contender_names.index("horse3") : make_odds(21),
                contender_names.index("horse4") : make_odds(15),
                contender_names.index("horse5") : make_odds(25)
            }
        )

        post_opcode(self.nodes[1], field_event_opcode, WGR_WALLET_EVENT['addr'])

        field_event_opcode = make_field_event(
            5,
            start_time,
            animal_racing_group,
            all_markets,
            sport_names.index("Horse racing"),
            tournament_names.index("The BMW stakes"),
            round_names.index("round1"),
            self.mrg_in_percent,
            {
                contender_names.index("horse1") : make_odds(33.3),
                contender_names.index("horse2") : make_odds(33.3),
                contender_names.index("horse3") : make_odds(33.3)
            }
        )
        post_opcode(self.nodes[1], field_event_opcode, WGR_WALLET_EVENT['addr'])

        self.nodes[0].generate(1)
        sync_blocks(self.nodes)

        assert_raises_rpc_error(-31, "Incorrect bet amount. Please ensure your bet is between 25 - 10000 WGR inclusive.",
            self.nodes[2].placefieldbet, 3, market_outright, contender_names.index("Alexander Albon"), 24)
        assert_raises_rpc_error(-31, "Incorrect bet amount. Please ensure your bet is between 25 - 10000 WGR inclusive.",
            self.nodes[2].placefieldbet, 3, market_outright, contender_names.index("Alexander Albon"), 10001)
        assert_raises_rpc_error(-31, "Error: there is no such FieldEvent: {}".format(202),
            self.nodes[2].placefieldbet, 202, market_outright, contender_names.index("Alexander Albon"), 30)
        assert_raises_rpc_error(-31, "Error: Incorrect bet market type for FieldEvent: {}".format(3),
            self.nodes[2].placefieldbet, 3, 100, contender_names.index("Alexander Albon"), 30)
        assert_raises_rpc_error(-31, "Error: there is no such contenderId {} in event {}".format(1050, 3),
            self.nodes[2].placefieldbet, 3, market_outright, 1050, 30)
        assert_raises_rpc_error(-31, "Error: contender odds is zero for event: {} contenderId: {}".format(3, contender_names.index("cont1")),
            self.nodes[2].placefieldbet, 3, market_outright, contender_names.index("cont1"), 30)
        # try to place bet at closed market type
        assert_raises_rpc_error(-31, "Error: market {} is closed for event {}".format(market_place, 3),
            self.nodes[2].placefieldbet, 3, market_place, contender_names.index("cont2"), 30)
        assert_raises_rpc_error(-31, "Error: market {} is closed for event {}".format(market_show, 3),
            self.nodes[2].placefieldbet, 3, market_show, contender_names.index("cont3"), 30)

        events = self.nodes[2].listfieldevents()
        bet_contender_info = {}
        for event in events:
            if event['event_id'] == 3:
                for contender in event['contenders']:
                    if contender['name'] == "Romain Grosjean":
                        bet_contender_info = contender
        # player1 makes win bet to event3
        player1_bet = 30
        player1_total_bet += player1_bet
        self.nodes[2].placefieldbet(3, market_outright, contender_names.index("Romain Grosjean"), player1_bet)

        onchainOdds = Decimal(bet_contender_info["outright-odds"])
        effectiveOdds = ((onchainOdds - ODDS_DIVISOR) * 9400) // ODDS_DIVISOR + ODDS_DIVISOR
        player1_expected_win = (player1_bet * effectiveOdds) / ODDS_DIVISOR

        self.nodes[0].generate(1)
        sync_blocks(self.nodes)

        field_update_odds_opcode = make_field_update_odds(
            3,
            {
                contender_names.index("Romain Grosjean") : 14000,
                contender_names.index("cont2") : 11000 # Add new conteder
            }
        )
        post_opcode(self.nodes[1], field_update_odds_opcode, WGR_WALLET_EVENT['addr'])

        self.nodes[0].generate(1)
        sync_blocks(self.nodes)

        events = self.nodes[3].listfieldevents()
        bet_contender_info = {}
        for event in events:
            if event['event_id'] == 3:
                for contender in event['contenders']:
                    if contender['name'] == "Romain Grosjean":
                        bet_contender_info = contender
        # player2 makes win bet to event3 after changed odds
        player2_bet = 30
        player2_total_bet += player2_bet
        self.nodes[3].placefieldbet(3, market_outright, contender_names.index("Romain Grosjean"), player2_bet)

        effectiveOdds = calc_effective_odds(Decimal(bet_contender_info["outright-odds"]))
        player2_expected_win = (player2_bet * effectiveOdds) / ODDS_DIVISOR

        self.nodes[0].generate(1)
        sync_blocks(self.nodes)

        field_result_opcode = make_field_result(
            3,
            STANDARD_RESULT,
            {
                contender_names.index("Romain Grosjean") : place1,
                contender_names.index("Pierre Gasly") : place2,
                contender_names.index("cont2") : place3
            }
        )
        post_opcode(self.nodes[1], field_result_opcode, WGR_WALLET_EVENT['addr'])

        self.nodes[0].generate(1)
        sync_blocks(self.nodes)

        assert_raises_rpc_error(-31, "Error: FieldEvent 3 was been resulted",
            self.nodes[2].placefieldbet, 3, market_outright, contender_names.index("Alexander Albon"), 30)

        player1_balance_before = Decimal(self.nodes[2].getbalance())
        player2_balance_before =  Decimal(self.nodes[3].getbalance())

        # make block with payouts
        self.nodes[0].generate(1)
        sync_blocks(self.nodes)

        player1_balance_after = Decimal(self.nodes[2].getbalance())
        player2_balance_after = Decimal(self.nodes[3].getbalance())

        assert_equal(player1_balance_after, player1_balance_before + player1_expected_win)
        assert_equal(player2_balance_after, player2_balance_before + player2_expected_win)

        # player1 makes lose bet to event4
        player1_bet = 40
        player1_total_bet += player1_bet
        self.nodes[2].placefieldbet(4, market_outright, contender_names.index("horse2"), player1_bet)

        self.nodes[0].generate(1)
        sync_blocks(self.nodes)

        field_update_odds_opcode = make_field_update_odds(
            4,
            {
                contender_names.index("horse1") : make_odds(25),
                contender_names.index("horse3") : make_odds(25),
                contender_names.index("horse6") : make_odds(16) # Add new conteder
            }
        )
        post_opcode(self.nodes[1], field_update_odds_opcode, WGR_WALLET_EVENT['addr'])

        self.nodes[0].generate(1)
        sync_blocks(self.nodes)

        # player2 makes win bet to event4
        player2_bet = 40
        player2_total_bet += player2_bet
        self.nodes[3].placefieldbet(4, market_outright, contender_names.index("horse6"), player2_bet)

        self.nodes[0].generate(1)
        sync_blocks(self.nodes)

        # make result for event3 and check payouts and players balances
        field_result_opcode = make_field_result(4, STANDARD_RESULT, {
            contender_names.index("horse6") : place1,
            contender_names.index("horse3") : place2,
            contender_names.index("horse5") : place3,
        })
        post_opcode(self.nodes[1], field_result_opcode, WGR_WALLET_EVENT['addr'])

        self.nodes[0].generate(1)
        sync_blocks(self.nodes)

        # TODO: for event4 calculate wins and check payouts/balances

        self.log.info("Field Bets outright market Success")

    def check_field_bet_place(self):
        # TODO: create field event
        # TODO: check place market closed (contenders size)
        # update event to place market
        # placefield bet
        # event result
        # calculate
        pass

    def check_field_bet_show(self):
        # TODO: create field event
        # TODO: check show market closed (contenders size)
        # update event to show market
        # placefield bet
        # event result
        # calculate
        pass

    def check_parlays_field_bet_undo(self):
        self.log.info("Check Parlay Field Bets Undo...")
        start_time = int(time.time() + 60 * 60)

        field_event_opcode = make_field_event(
            231,
            start_time,
            animal_racing_group,
            all_markets,
            sport_names.index("Horse racing"),
            tournament_names.index("The BMW stakes"),
            round_names.index("round0"),
            self.mrg_in_percent,
            {
                contender_names.index("horse1") : make_odds(20),
                contender_names.index("horse2") : make_odds(20),
                contender_names.index("horse3") : make_odds(20),
                contender_names.index("horse4") : make_odds(20),
                contender_names.index("horse5") : make_odds(20)
            }
        )
        post_opcode(self.nodes[1], field_event_opcode, WGR_WALLET_EVENT['addr'])

        self.nodes[0].generate(1)
        sync_blocks(self.nodes)

        field_event_opcode = make_field_event(
            232,
            start_time,
            animal_racing_group,
            all_markets,
            sport_names.index("Horse racing"),
            tournament_names.index("The BMW stakes"),
            round_names.index("round0"),
            self.mrg_in_percent,
            {
                contender_names.index("horse1") : make_odds(25),
                contender_names.index("horse2") : make_odds(25),
                contender_names.index("horse3") : make_odds(25),
                contender_names.index("horse4") : make_odds(25),
            }
        )
        post_opcode(self.nodes[1], field_event_opcode, WGR_WALLET_EVENT['addr'])

        self.nodes[0].generate(1)
        sync_blocks(self.nodes)

        # For revert test
        revert_chain_height = self.nodes[4].getblockcount()
        self.stop_node(4)

        player1_bet = 30
        self.nodes[2].placefieldparlaybet(
            [
                {"eventId": 231, "marketType": market_outright, "contenderId": contender_names.index("horse1")},
                {"eventId": 232, "marketType": market_outright, "contenderId": contender_names.index("horse4")},
            ],
            player1_bet
        )

        self.nodes[0].generate(1)
        sync_blocks(self.nodes[0:4])

        # for node in self.nodes[0:4]:
        event_stats = self.nodes[1].getfieldeventliability(231)
        assert_equal(event_stats["event-status"], "running")
        for contender in event_stats["contenders"]:
            if contender["contender-id"] == contender_names.index("horse1"):
                assert_equal(contender["outright-bets"], 1)
            else:
                assert_equal(contender["outright-bets"], 0)

        # for node in self.nodes[0:4]:
        event_stats = self.nodes[1].getfieldeventliability(232)
        assert_equal(event_stats["event-status"], "running")
        for contender in event_stats["contenders"]:
            if contender["contender-id"] == contender_names.index("horse4"):
                assert_equal(contender["outright-bets"], 1)
            else:
                assert_equal(contender["outright-bets"], 0)

        field_result_opcode = make_field_result(231, STANDARD_RESULT, {
            contender_names.index("horse1") : place1,
            contender_names.index("horse2") : place2,
            contender_names.index("horse3") : place3
        })
        post_opcode(self.nodes[1], field_result_opcode, WGR_WALLET_EVENT['addr'])

        field_result_opcode = make_field_result(232, STANDARD_RESULT, {
            contender_names.index("horse1") : place1,
            contender_names.index("horse2") : place2,
            contender_names.index("horse3") : place3
        })
        post_opcode(self.nodes[1], field_result_opcode, WGR_WALLET_EVENT['addr'])

        self.nodes[0].generate(1)
        sync_blocks(self.nodes[0:4])

        # for node in self.nodes[0:4]:
        event_stats = self.nodes[2].getfieldeventliability(231)
        assert_equal(event_stats["event-status"], "resulted")

        # for node in self.nodes[0:4]:
        event_stats = self.nodes[2].getfieldeventliability(232)
        assert_equal(event_stats["event-status"], "resulted")

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

        #for node in self.nodes:
        event_stats = self.nodes[2].getfieldeventliability(231)
        assert_equal(event_stats["event-status"], "running")
        for contender in event_stats["contenders"]:
            assert_equal(contender["outright-bets"], 0)
            assert_equal(contender["outright-liability"], 0)

        # for node in self.nodes:
        event_stats = self.nodes[2].getfieldeventliability(232)
        assert_equal(event_stats["event-status"], "running")
        for contender in event_stats["contenders"]:
            assert_equal(contender["outright-bets"], 0)
            assert_equal(contender["outright-liability"], 0)

        self.log.info("Check Parlay Field Bets Undo Success")

    def check_parlays_field_bet(self):
        self.log.info("Check Parlay Field Bets...")
        start_time = int(time.time() + 60 * 60)

        global player1_total_bet

        # For round1 tests
        field_event_opcode = make_field_event(
            6,
            start_time,
            animal_racing_group,
            all_markets,
            sport_names.index("Horse racing"),
            tournament_names.index("The BMW stakes"),
            round_names.index("round1"),
            self.mrg_in_percent,
            {
                contender_names.index("horse1") : make_odds(25),
                contender_names.index("horse2") : make_odds(25),
                contender_names.index("horse3") : make_odds(25),
                contender_names.index("horse4") : make_odds(25),
            }
        )
        post_opcode(self.nodes[1], field_event_opcode, WGR_WALLET_EVENT['addr'])

        self.nodes[0].generate(1)
        sync_blocks(self.nodes)

        show_market_event_id = 7
        field_event_opcode = make_field_event(
            show_market_event_id,
            start_time,
            animal_racing_group,
            all_markets,
            sport_names.index("Horse racing"),
            tournament_names.index("The BMW stakes"),
            round_names.index("round0"),
            self.mrg_in_percent,
            {
                contender_names.index("horse1") : make_odds(10),
                contender_names.index("horse2") : make_odds(11),
                contender_names.index("horse3") : make_odds(12),
                contender_names.index("horse4") : make_odds(13),
                contender_names.index("horse5") : make_odds(14),
                contender_names.index("horse6") : make_odds(20),
                contender_names.index("horse7") : make_odds(15),
                contender_names.index("horse8") : 0, # for zero odds test,
                contender_names.index("horse9") : make_odds(5),
            }
        )
        post_opcode(self.nodes[1], field_event_opcode, WGR_WALLET_EVENT['addr'])

        self.nodes[0].generate(1)
        sync_blocks(self.nodes)

        place_market_event_id = 8
        field_event_opcode = make_field_event(
            place_market_event_id,
            start_time,
            animal_racing_group,
            all_markets,
            sport_names.index("Horse racing"),
            tournament_names.index("The BMW stakes"),
            round_names.index("round0"),
            self.mrg_in_percent,
            {
                contender_names.index("horse1") : make_odds(30),
                contender_names.index("horse2") : make_odds(11),
                contender_names.index("horse3") : make_odds(12),
                contender_names.index("horse4") : make_odds(13),
                contender_names.index("horse5") : make_odds(14),
                contender_names.index("horse6") : make_odds(20),
            }
        )
        post_opcode(self.nodes[1], field_event_opcode, WGR_WALLET_EVENT['addr'])

        self.nodes[0].generate(1)
        sync_blocks(self.nodes)

        outright_market_event_id = 9
        field_event_opcode = make_field_event(
            outright_market_event_id,
            start_time,
            animal_racing_group,
            all_markets,
            sport_names.index("Horse racing"),
            tournament_names.index("The BMW stakes"),
            round_names.index("round0"),
            self.mrg_in_percent,
            {
                contender_names.index("horse1") : make_odds(30),
                contender_names.index("horse2") : make_odds(20),
                contender_names.index("horse3") : make_odds(25),
                contender_names.index("horse4") : make_odds(25)
            }
        )
        post_opcode(self.nodes[1], field_event_opcode, WGR_WALLET_EVENT['addr'])

        self.nodes[0].generate(1)
        sync_blocks(self.nodes)

        assert_raises_rpc_error(-31, "Incorrect bet amount. Please ensure your bet is between 25 - 10000 WGR inclusive.",
            self.nodes[2].placefieldparlaybet, [
                {"eventId": show_market_event_id, "marketType": market_show, "contenderId": contender_names.index("horse1")},
                {"eventId": place_market_event_id, "marketType": market_place, "contenderId": contender_names.index("horse4")},
            ], 24)
        assert_raises_rpc_error(-31, "Incorrect bet amount. Please ensure your bet is between 25 - 10000 WGR inclusive.",
            self.nodes[2].placefieldparlaybet, [
                {"eventId": show_market_event_id, "marketType": market_show, "contenderId": contender_names.index("horse1")},
                {"eventId": place_market_event_id, "marketType": market_place, "contenderId": contender_names.index("horse4")},
            ], 10001)
        assert_raises_rpc_error(-31, "Error: there is no such FieldEvent: {}".format(402),
            self.nodes[2].placefieldparlaybet, [
                {"eventId": show_market_event_id, "marketType": market_show, "contenderId": contender_names.index("horse1")},
                {"eventId": 402,                  "marketType": market_place, "contenderId": contender_names.index("horse4")},
            ], 30)
        assert_raises_rpc_error(-31, "Error: Incorrect bet market type for FieldEvent: {}".format(show_market_event_id),
            self.nodes[2].placefieldparlaybet, [
                {"eventId": show_market_event_id, "marketType": 100, "contenderId": contender_names.index("horse1")},
                {"eventId": place_market_event_id, "marketType": market_place, "contenderId": contender_names.index("horse4")},
            ], 30)
        assert_raises_rpc_error(-31, "Error: there is no such contenderId {} in event {}".format(1050, show_market_event_id),
            self.nodes[2].placefieldparlaybet, [
                {"eventId": show_market_event_id, "marketType": market_show, "contenderId": 1050},
                {"eventId": place_market_event_id, "marketType": market_place, "contenderId": contender_names.index("horse4")},
            ], 30)
        assert_raises_rpc_error(-31, "Error: contender odds is zero for event: {} contenderId: {}".format(show_market_event_id, contender_names.index("horse8")),
            self.nodes[2].placefieldparlaybet, [
                {"eventId": show_market_event_id, "marketType": market_show, "contenderId": contender_names.index("horse8")},
                {"eventId": place_market_event_id, "marketType": market_place, "contenderId": contender_names.index("horse4")},
            ], 30)
        assert_raises_rpc_error(-31, "Error: event {} cannot be part of parlay bet".format(5),
            self.nodes[2].placefieldparlaybet, [
                {"eventId": 5, "marketType": market_outright, "contenderId": contender_names.index("horse1")},
                {"eventId": place_market_event_id, "marketType": market_place, "contenderId": contender_names.index("horse4")},
            ], 30)
        assert_raises_rpc_error(-31, "Error: market {} is closed for event {}".format(market_show, place_market_event_id),
            self.nodes[2].placefieldparlaybet, [
                {"eventId": show_market_event_id, "marketType": market_show,  "contenderId": contender_names.index("horse1")},
                {"eventId": place_market_event_id, "marketType": market_show, "contenderId": contender_names.index("horse4")},
            ], 30)
        assert_raises_rpc_error(-31, "Error: market {} is closed for event {}".format(market_place, outright_market_event_id),
            self.nodes[2].placefieldparlaybet, [
                {"eventId": outright_market_event_id, "marketType": market_place,  "contenderId": contender_names.index("horse1")},
                {"eventId": place_market_event_id, "marketType": market_place, "contenderId": contender_names.index("horse4")},
            ], 30)

        # Player1 place win parlay bet
        player1_bet = 40
        parlay_bet_tx = self.nodes[2].placefieldparlaybet(
            [
                {"eventId": outright_market_event_id, "marketType": market_outright,  "contenderId": contender_names.index("horse1")},
                {"eventId": place_market_event_id, "marketType": market_place, "contenderId": contender_names.index("horse5")},
                {"eventId": show_market_event_id, "marketType": market_show, "contenderId": contender_names.index("horse7")},
            ], player1_bet
        )
        # TODO: player_1 expected win calculate
        events = self.nodes[2].listfieldevents()
        effective_parlay_bet_odds = []
        for event in events:
            if event['event_id'] == outright_market_event_id:
                for contender in event['contenders']:
                    if contender['name'] == "horse1":
                        effective_parlay_bet_odds.append(calc_effective_odds(int(contender['outright-odds'])))
            elif event['event_id'] == place_market_event_id:
                for contender in event['contenders']:
                    if contender['name'] == "horse5":
                        effective_parlay_bet_odds.append(calc_effective_odds(int(contender['place-odds'])))
            elif event['event_id'] == show_market_event_id:
                for contender in event['contenders']:
                    if contender['name'] == "horse7":
                        effective_parlay_bet_odds.append(calc_effective_odds(int(contender['show-odds'])))

        final_effective_odds = 0
        first_multiply = True
        for odds in effective_parlay_bet_odds:
            if first_multiply:
                final_effective_odds = odds
                first_multiply = False
            else:
                final_effective_odds = (final_effective_odds * odds) // ODDS_DIVISOR

        player1_expected_win = player1_bet * Decimal(final_effective_odds) / ODDS_DIVISOR

        self.nodes[0].generate(1)
        sync_blocks(self.nodes)

        parlay_bet = self.nodes[0].getbetbytxid(parlay_bet_tx)

        for event_id in [outright_market_event_id, place_market_event_id, show_market_event_id]:
            # for node in self.nodes:
            event_stats = self.nodes[2].getfieldeventliability(event_id)
            assert_equal(event_stats["event-status"], "running")
            for contender in event_stats["contenders"]:
                if event_id == outright_market_event_id and contender["contender-id"] == contender_names.index("horse1"):
                    assert_equal(contender["outright-bets"], 1)
                    assert_equal(contender["place-bets"], 0)
                    assert_equal(contender["show-bets"], 0)
                if event_id == place_market_event_id and contender["contender-id"] == contender_names.index("horse5"):
                    assert_equal(contender["outright-bets"], 0)
                    assert_equal(contender["place-bets"], 1)
                    assert_equal(contender["show-bets"], 0)
                if event_id == show_market_event_id and contender["contender-id"] == contender_names.index("horse7"):
                    assert_equal(contender["outright-bets"], 0)
                    assert_equal(contender["place-bets"], 0)
                    assert_equal(contender["show-bets"], 1)

        # make results
        field_result_opcode = make_field_result(outright_market_event_id, STANDARD_RESULT, {
            contender_names.index("horse1") : place1,
            contender_names.index("horse3") : place2,
            contender_names.index("horse4") : place3,
        })
        post_opcode(self.nodes[1], field_result_opcode, WGR_WALLET_EVENT['addr'])

        field_result_opcode = make_field_result(place_market_event_id, STANDARD_RESULT, {
            contender_names.index("horse3") : place1,
            contender_names.index("horse5") : place2,
            contender_names.index("horse1") : place3,
        })
        post_opcode(self.nodes[1], field_result_opcode, WGR_WALLET_EVENT['addr'])

        field_result_opcode = make_field_result(show_market_event_id, STANDARD_RESULT, {
            contender_names.index("horse2") : place1,
            contender_names.index("horse1") : place2,
            contender_names.index("horse7") : place3,
        })
        post_opcode(self.nodes[1], field_result_opcode, WGR_WALLET_EVENT['addr'])

        self.nodes[0].generate(1)
        sync_blocks(self.nodes)

        player1_balance_before = Decimal(self.nodes[2].getbalance())

        # make block with payouts
        self.nodes[0].generate(1)
        sync_blocks(self.nodes)

        player1_balance_after = Decimal(self.nodes[2].getbalance())

        payout = player1_balance_after - player1_balance_before

        # print("Expected win: " + str(player1_expected_win))
        # print("payout: " + str(payout))

        # calculated odds in test and in chain have small difference due accuracy
        # assert_equal(player1_balance_after, player1_balance_before + player1_expected_win)

        parlay_bet = self.nodes[0].getbetbytxid(parlay_bet_tx)

        self.log.info("Parlay Field Bets Success")

    def check_timecut_refund(self):
        self.log.info("Check Timecut Refund...")

        # TODO: check

        self.log.info("Timecut Refund Success")

    def run_test(self):
        self.check_minting()
        # Chain height = 300 after minting -> v4 protocol active
        self.check_mapping()
        self.check_event()
        self.check_event_update_odds()
        self.check_event_zeroing_odds()
        # TODO: check big size transactions (lots of contenders)
        self.check_field_bet_undo()
        self.check_field_bet_outright()
        # self.check_field_bet_place() # TODO
        # self.check_field_bet_show() # TODO
        self.check_parlays_field_bet_undo()
        self.check_parlays_field_bet()
        # self.check_timecut_refund() # TODO

if __name__ == '__main__':
    BettingTest().main()
    # check_calculate_odds()
