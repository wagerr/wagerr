#!/usr/bin/env python
# -*- coding: utf-8 -*-

import os
import struct
import subprocess
from test_framework.util import bytes_to_hex_str
from test_framework.mininode import COIN

OPCODE_PREFIX = 42

OPCODE_BTX_MAPPING = 0x01
OPCODE_BTX_EVENT = 0x02
OPCODE_BTX_BET = 0x03
OPCODE_BTX_RESULT = 0x04
OPCODE_BTX_UPDATE_ODDS = 0x05
OPCODE_BTX_CG_EVENT = 0x06
OPCODE_BTX_CG_BET = 0x07
OPCODE_BTX_CG_RESULT = 0x08
OPCODE_BTX_SPREAD_EVENT = 0x09
OPCODE_BTX_TOTALS_EVENT = 0x0a
OPCODE_BTX_EVENT_PATCH = 0x0b
OPCODE_BTX_PARLAY_BET = 0x0c
OPCODE_BTX_QG_BET = 0x0d
OPCODE_BTX_ZERO_ODDS = 0x0e
OPCODE_BTX_FIELD_EVENT = 0x0f
OPCODE_BTX_FIELD_UPDATE_ODDS = 0x10
OPCODE_BTX_FIELD_ZEROING_ODDS = 0x11
OPCODE_BTX_FIELD_RESULT = 0x12

OPCODE_QG_DICE = 0x00

SPORT_MAPPING      = 0x01
ROUND_MAPPING      = 0x02
TEAM_MAPPING       = 0x03
TOURNAMENT_MAPPING = 0x04
INDIVIDUAL_SPORT_MAPPING    = 0x05
CONTENDER_MAPPING           = 0x06

STANDARD_RESULT = 0x01
EVENT_REFUND    = 0x02
ML_REFUND       = 0x03
SPREADS_REFUND  = 0x04
TOTALS_REFUND   = 0x05
EVENT_CLOSED    = 0x06

WGR_TX_FEE = 0.001

QG_DICE_EQUAL = 0x00
QG_DICE_NOT_EQUAL = 0x01
QG_DICE_TOTAL_OVER = 0x02
QG_DICE_TOTAL_UNDER = 0x03
QG_DICE_EVEN = 0x04
QG_DICE_ODD = 0x05

ODDS_DIVISOR = 10000
BETX_PERMILLE = 60

# Encode an unsigned int in hexadecimal and little endian byte order. The function expects the value and the size in
# bytes as parameters.
def encode_int_little_endian(value: int, size: int):
    if size == 1:
        return struct.pack('<B', int(value)).hex()
    elif size == 2:
        return struct.pack('<H', int(value)).hex()
    elif size == 4:
        return struct.pack('<I', int(value)).hex()
    else:
        raise RuntimeError("Incorrect byte size was specified for encoding.")

def encode_signed_int_little_endian(value: int, size: int):
    if size == 1:
        return struct.pack('<b', int(value)).hex()
    elif size == 2:
        return struct.pack('<h', int(value)).hex()
    elif size == 4:
        return struct.pack('<i', int(value)).hex()
    else:
        raise RuntimeError("Incorrect byte size was specified for encoding.")

# Encode a string in hexadecimal notation. Byte order does not matter for UTF-8 encoded strings, however, I have kept
# used similar encoding methods as I would to encode in little-endian byte order to keep things consistent with int
# handling. The end result for a string is the same for little/big endian.
#
# There is no limit to the size of the encoded string as the function will use the length of the string.
def encode_str_hex(value: str):
    value = bytes(value.encode('utf8'))
    return struct.pack("<%ds" % (len(value)), value).hex()

# Create a common opcode.
def make_common_header(btx_type, version = 1):
    prefix = str(OPCODE_PREFIX)
    version_hex = encode_int_little_endian(version, 1)
    btx_type_hex = encode_int_little_endian(btx_type, 1)
    return prefix + version_hex + btx_type_hex

# Create a mapping opcode.
def make_mapping(namespace_id, mapping_id, mapping_name):
    mapping_id_size = 2 if namespace_id != 3 and namespace_id != 6 else 4
    result = make_common_header(OPCODE_BTX_MAPPING)
    result = result + encode_int_little_endian(namespace_id, 1)
    result = result + encode_int_little_endian(mapping_id, mapping_id_size)
    for sym in mapping_name:
        result = result + encode_int_little_endian(ord(sym), 1)
    return result

# Create a moneyline patch opcode.
def make_event_patch(event_id, timestamp):
    result = make_common_header(OPCODE_BTX_EVENT_PATCH)
    result = result + encode_int_little_endian(event_id, 4)
    result = result + encode_int_little_endian(timestamp, 4)
    return result

# Create a moneyline event opcode.
def make_event(event_id, timestamp, sport, tournament, round, home_team, away_team, home_odds, away_odds, draw_odds):
    result = make_common_header(OPCODE_BTX_EVENT)
    result = result + encode_int_little_endian(event_id, 4)
    result = result + encode_int_little_endian(timestamp, 4)
    result = result + encode_int_little_endian(sport, 2)
    result = result + encode_int_little_endian(tournament, 2)
    result = result + encode_int_little_endian(round, 2)
    result = result + encode_int_little_endian(home_team, 4)
    result = result + encode_int_little_endian(away_team, 4)
    result = result + encode_int_little_endian(home_odds, 4)
    result = result + encode_int_little_endian(away_odds, 4)
    result = result + encode_int_little_endian(draw_odds, 4)
    return result

# Create a field event opcode
def make_field_event(event_id, timestamp, group, marketType, sport, tournament, round, mrg_in_percent, contenders_win_odds):
    result = make_common_header(OPCODE_BTX_FIELD_EVENT)
    result = result + encode_int_little_endian(event_id, 4)
    result = result + encode_int_little_endian(timestamp, 4)
    result = result + encode_int_little_endian(sport, 2)
    result = result + encode_int_little_endian(tournament, 2)
    result = result + encode_int_little_endian(round, 2)
    result = result + encode_int_little_endian(group, 1)
    result = result + encode_int_little_endian(marketType, 1)
    result = result + encode_int_little_endian(mrg_in_percent, 4)
    result = result + encode_int_little_endian(int(len(contenders_win_odds)), 1) # map size
    for contender_id, contender_odds in contenders_win_odds.items():
        result = result + encode_int_little_endian(int(contender_id), 4)
        result = result + encode_int_little_endian(int(contender_odds), 4)
    return result

# Create an update odds for field event opcode
def make_field_update_odds(event_id, contenders_win_odds):
    result = make_common_header(OPCODE_BTX_FIELD_UPDATE_ODDS)
    result = result + encode_int_little_endian(event_id, 4)
    result = result + encode_int_little_endian(int(len(contenders_win_odds)), 1) # map size
    for contender_id, contender_odds in contenders_win_odds.items():
        result = result + encode_int_little_endian(int(contender_id), 4)
        result = result + encode_int_little_endian(int(contender_odds), 4)
    return result

# Create an zeroing odds for field event opcode
def make_field_zeroing_odds(event_id):
    result = make_common_header(OPCODE_BTX_FIELD_ZEROING_ODDS)
    result = result + encode_int_little_endian(event_id, 4)
    return result

# Create a result for field event opcode
def make_field_result(event_id, result_type, contenders_results):
    result = make_common_header(OPCODE_BTX_FIELD_RESULT)
    result = result + encode_int_little_endian(event_id, 4)
    result = result + encode_int_little_endian(result_type, 1)
    result = result + encode_int_little_endian(int(len(contenders_results)), 1) # map size
    for contender_id, contender_result in contenders_results.items():
        result = result + encode_int_little_endian(int(contender_id), 4)
        result = result + encode_int_little_endian(int(contender_result), 1)
    return result

# Create a moneyline odds update opcode.
def make_update_ml_odds(event_id, home_odds, away_odds, draw_odds):
    result = make_common_header(OPCODE_BTX_UPDATE_ODDS)
    result = result + encode_int_little_endian(event_id, 4)
    result = result + encode_int_little_endian(home_odds, 4)
    result = result + encode_int_little_endian(away_odds, 4)
    result = result + encode_int_little_endian(draw_odds, 4)
    return result

def make_zeroing_odds(event_ids):
    result = make_common_header(OPCODE_BTX_ZERO_ODDS)
    result = result + encode_int_little_endian(int(len(event_ids)), 1) # vector size
    for event_id in event_ids:
        result = result + encode_int_little_endian(int(event_id), 4)
    return result

# Create a spread event
def make_spread_event(event_id, points, home_odds, away_odds):
    result = make_common_header(OPCODE_BTX_SPREAD_EVENT)
    result = result + encode_int_little_endian(event_id, 4)
    result = result + encode_signed_int_little_endian(points, 2)
    result = result + encode_int_little_endian(home_odds, 4)
    result = result + encode_int_little_endian(away_odds, 4)
    return result

# Create a total event
def make_total_event(event_id, points, over_odds, under_odds):
    result = make_common_header(OPCODE_BTX_TOTALS_EVENT)
    result = result + encode_int_little_endian(event_id, 4)
    result = result + encode_int_little_endian(points, 2)
    result = result + encode_int_little_endian(over_odds, 4)
    result = result + encode_int_little_endian(under_odds, 4)
    return result

def make_result(event_id, result_type, home_score, away_score):
    result = make_common_header(OPCODE_BTX_RESULT)
    result = result + encode_int_little_endian(event_id, 4)
    result = result + encode_int_little_endian(result_type, 1)
    result = result + encode_int_little_endian(home_score, 2)
    result = result + encode_int_little_endian(away_score, 2)
    return result

def make_chain_games_event(event_id, fee):
    result = make_common_header(OPCODE_BTX_CG_EVENT)
    result = result + encode_int_little_endian(event_id, 2)
    result = result + encode_int_little_endian(fee, 2)
    return result

def make_chain_games_result(event_id):
    result = make_common_header(OPCODE_BTX_CG_RESULT)
    result = result + encode_int_little_endian(event_id, 2)
    return result

def get_utxo_list(node, address, min_amount=WGR_TX_FEE):
    utxo_array = []
    total_amount = float(0.00)

    # Find enough UTXO to use in a spend transaction to cover the minimum amount.
    list_unspent = node.listunspent(1, 9999999, [address])
    assert(len(list_unspent) > 0)
    for utxo in list_unspent:
        utxo_array.append({'txid': utxo["txid"], 'vout': utxo["vout"]})
        total_amount += float(utxo['amount'])
        if total_amount > min_amount:
            break

    return utxo_array, total_amount

def post_opcode(node, opcode, address):
    # Get unspent outputs to use as inputs (spend).
    inputs, spend = get_utxo_list(node, address)
    # Calculate the change by subtracting the transaction fee from the UTXO's value.
    change = float(spend)
    # Create the output JSON
    outputs = {address: change, 'data': opcode}

    # Create the raw transaction.
    trx = node.createrawtransaction(inputs, outputs)
    # Sign the raw transaction.
    trx = node.signrawtransaction(trx)
    return node.sendrawtransaction(trx['hex'])

def post_raw_opcode(node, ctxout, address):
    # Get unspent outputs to use as inputs (spend).
    inputs, spend = get_utxo_list(node, address)
    # Calculate the change by subtracting the transaction fee from the UTXO's value.
    change = float(spend - ctxout.nValue / COIN)

    # Create the output JSON
    outputs = {address: change, 'ctxout': bytes_to_hex_str(ctxout.serialize())}

    # Create the raw transaction.
    trx = node.createrawtransaction(inputs, outputs)

    # Sign the raw transaction.
    trx = node.signrawtransaction(trx)
    return node.sendrawtransaction(trx['hex'])

# Create dice game bet.
def make_dice_bet(dice_type, number = 1):
    result = make_common_header(OPCODE_BTX_QG_BET)
    result = result + encode_int_little_endian(OPCODE_QG_DICE, 1)
    if dice_type != QG_DICE_EVEN and dice_type != QG_DICE_ODD:
        result = result + encode_int_little_endian(5, 1) # vector size
        result = result + encode_int_little_endian(dice_type, 1)
        result = result + encode_int_little_endian(number, 4)
    else:
        result = result + encode_int_little_endian(1, 1) # vector size
        result = result + encode_int_little_endian(dice_type, 1)
    return result

def calc_effective_odds(onchain_odds):
    return ((onchain_odds - ODDS_DIVISOR) * 9400) // ODDS_DIVISOR + ODDS_DIVISOR