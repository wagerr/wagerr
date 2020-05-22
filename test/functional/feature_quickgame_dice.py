#!/usr/bin/env python3
# Copyright (c) 2020 The Wagerr developers
# Distributed under the MIT/X11 software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

from test_framework.betting_opcode import *
from test_framework.script import CScript, OP_RETURN
from test_framework.messages import CTxOut
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import wait_until, rpc_port, assert_equal, assert_raises_rpc_error
from distutils.dir_util import copy_tree, remove_tree
from decimal import *
from test_framework.mininode import COIN
import os

DICE_TYPE = { "equal": QG_DICE_EQUAL,
              "not equal": QG_DICE_NOT_EQUAL,
              "total over": QG_DICE_TOTAL_OVER,
              "total under": QG_DICE_TOTAL_UNDER,
              "even": QG_DICE_EVEN,
              "odd": QG_DICE_ODD}

WGR_WALLET_ORACLE = { "addr": "TXuoB9DNEuZx1RCfKw3Hsv7jNUHTt4sVG1", "key": "TBwvXbNNUiq7tDkR2EXiCbPxEJRTxA1i6euNyAE9Ag753w36c1FZ" }
WGR_WALLET_DEV = { "addr": "TLuTVND9QbZURHmtuqD5ESECrGuB9jLZTs", "key": "TFCrxaUt3EjHzMGKXeBqA7sfy3iaeihg5yZPSrf9KEyy4PHUMWVe" }
WGR_WALLET_OMNO = { "addr": "THofaueWReDjeZQZEECiySqV9GP4byP3qr", "key": "TDJnwRkSk8JiopQrB484Ny9gMcL1x7bQUUFFFNwJZmmWA7U79uRk" }

ODDS_DIVISOR = 10000
BETX_PERMILLE = 60

class QgDiceTest(BitcoinTestFramework):
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

    def post_qg_bet(self, node, player, bet, type_game, number = 1):
        mqg_bet_opcode = make_dice_bet(type_game, number)
        scriptPubKey = CScript([OP_RETURN, bytes.fromhex(mqg_bet_opcode)])
        output = CTxOut(bet, scriptPubKey)
        post_raw_opcode(node, output, player)

    def check_dice(self):
        self.log.info("Check Dice Game...")
        player_bet = 100 * COIN

        self.nodes[1].importprivkey(WGR_WALLET_DEV['key'])
        self.nodes[3].importprivkey(WGR_WALLET_OMNO['key'])

        self.players.append(self.nodes[2].getnewaddress())

        for i in range(249):
            self.nodes[0].generate(1)

        for i in range(20):
            self.nodes[0].sendtoaddress(self.players[0], 2000)

        self.nodes[0].generate(1)
        self.sync_all()
        self.nodes[0].generate(1)
        # check players balance
        assert_equal(self.nodes[2].getbalance(), 40000)
        # check dev balance
        assert_equal(self.nodes[1].getbalance(), 0)
        # check omno balance
        assert_equal(self.nodes[3].getbalance(), 0)

        self.post_qg_bet(self.nodes[2], self.players[0], player_bet, QG_DICE_EQUAL, 7)
        self.post_qg_bet(self.nodes[2], self.players[0], player_bet, QG_DICE_NOT_EQUAL, 7)
        self.post_qg_bet(self.nodes[2], self.players[0], player_bet, QG_DICE_TOTAL_OVER, 7)
        self.post_qg_bet(self.nodes[2], self.players[0], player_bet, QG_DICE_TOTAL_UNDER, 7)
        self.post_qg_bet(self.nodes[2], self.players[0], player_bet, QG_DICE_EVEN)
        self.post_qg_bet(self.nodes[2], self.players[0], player_bet, QG_DICE_ODD)

        self.sync_all()
        blockHash = self.nodes[0].generate(1)[0]
        blockInfo = self.nodes[0].getblock(blockHash)

        self.nodes[0].generate(1)
        self.sync_all()

        all_qg_bets = self.nodes[0].getallqgbets()
        assert_equal(len(all_qg_bets), 6) # 6 dice bets

        print("all_qg_bets", all_qg_bets)
        resultBlockHash = 0
        sum = 0
        winAmount = 0
        for bet in all_qg_bets:
            assert_equal(bet['blockHeight'], blockInfo['height'])
            if resultBlockHash == 0:
                resultBlockHash = bet['resultBlockHash']
                sum = Decimal(bet['betInfo']['sum'])
            else:
                assert_equal(resultBlockHash, bet['resultBlockHash'])
                assert_equal(sum, Decimal(bet['betInfo']['sum']))
            if DICE_TYPE[bet['betInfo']['diceGameType']] == QG_DICE_EQUAL:
                print("QG_DICE_EQUAL", sum)
                if sum == 7:
                    assert_equal(bet['betResultType'], 'win')
                    winAmount = winAmount + Decimal(bet['payout'])
                else:
                    assert_equal(bet['betResultType'], 'lose')
            if DICE_TYPE[bet['betInfo']['diceGameType']] == QG_DICE_NOT_EQUAL:
                print("QG_DICE_NOT_EQUAL", sum)
                if sum != 7:
                    assert_equal(bet['betResultType'], 'win')
                    winAmount = winAmount + Decimal(bet['payout'])
                else:
                    assert_equal(bet['betResultType'], 'lose')
            if DICE_TYPE[bet['betInfo']['diceGameType']] == QG_DICE_TOTAL_OVER:
                print("QG_DICE_TOTAL_OVER", sum)
                if sum > 7:
                    assert_equal(bet['betResultType'], 'win')
                    winAmount = winAmount + Decimal(bet['payout'])
                else:
                    assert_equal(bet['betResultType'], 'lose')
            if DICE_TYPE[bet['betInfo']['diceGameType']] == QG_DICE_TOTAL_UNDER:
                print("QG_DICE_TOTAL_UNDER", sum)
                if sum <= 7:
                    assert_equal(bet['betResultType'], 'win')
                    winAmount = winAmount + Decimal(bet['payout'])
                else:
                    assert_equal(bet['betResultType'], 'lose')
            if DICE_TYPE[bet['betInfo']['diceGameType']] == QG_DICE_EVEN:
                print("QG_DICE_EVEN", sum, sum % 2)
                if sum % 2 == 0:
                    assert_equal(bet['betResultType'], 'win')
                    winAmount = winAmount + Decimal(bet['payout'])
                else:
                    assert_equal(bet['betResultType'], 'lose')
            if DICE_TYPE[bet['betInfo']['diceGameType']] == QG_DICE_ODD:
                print("QG_DICE_ODD", sum, sum % 2)
                if sum % 2 == 1:
                    assert_equal(bet['betResultType'], 'win')
                    winAmount = winAmount + Decimal(bet['payout'])
                else:
                    assert_equal(bet['betResultType'], 'lose')

        assert_equal(self.nodes[2].getbalance(), 40000 + winAmount - 600) # 6 dice bets
    def run_test(self):
        self.check_dice()


if __name__ == '__main__':
    QgDiceTest().main()