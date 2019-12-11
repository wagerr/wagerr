#!/usr/bin/env python
# -*- coding: utf-8 -*-

import os
import struct
import subprocess

OPCODE_PREFIX = 42
OPCODE_VERSION = 0x01

OPCODE_BTX_MAPPING = 0x01
OPCODE_BTX_EVENT = 0x02
OPCODE_BTX_BET = 0x03
OPCODE_BTX_RESULT = 0x04
OPCODE_BTX_UPDATE_ODDS = 0x05
OPCODE_BTX_SPREAD_EVENT = 0x09
OPCODE_BTX_TOTALS_EVENT = 0x0a
OPCODE_BTX_EVENT_PATCH = 0x0b
OPCODE_BTX_PARLAY_BET = 0x0c

SPORT_MAPPING      = 0x01
ROUND_MAPPING      = 0x02
TEAM_MAPPING       = 0x03
TOURNAMENT_MAPPING = 0x04

STANDARD_RESULT = 0x01
EVENT_REFUND    = 0x02
ML_REFUND       = 0x03
SPREADS_REFUND  = 0x04
TOTALS_REFUND   = 0x05

WGR_TX_FEE = 0.001

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


# Encode a string in hexadecimal notation. Byte order does not matter for UTF-8 encoded strings, however, I have kept
# used similar encoding methods as I would to encode in little-endian byte order to keep things consistent with int
# handling. The end result for a string is the same for little/big endian.
#
# There is no limit to the size of the encoded string as the function will use the length of the string.
def encode_str_hex(value: str):
    value = bytes(value.encode('utf8'))
    return struct.pack("<%ds" % (len(value)), value).hex()

# Create a common opcode.
def make_common_header(btx_type):
    prefix = str(OPCODE_PREFIX)
    version_hex = encode_int_little_endian(OPCODE_VERSION, 1)
    btx_type_hex = encode_int_little_endian(btx_type, 1)
    return prefix + version_hex + btx_type_hex

# Create a mapping opcode.
def make_mapping(namespace_id, mapping_id, mapping_name):
    mapping_id_size = 2 if namespace_id != 3 else 4
    result = make_common_header(OPCODE_BTX_MAPPING)
    result = result + encode_int_little_endian(namespace_id, 1)
    result = result + encode_int_little_endian(mapping_id, mapping_id_size)
    for sym in mapping_name:
        result = result + encode_int_little_endian(ord(sym), 1)
    result = result + encode_int_little_endian(0, 1)
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

# Create a moneyline odds update opcode.
def make_update_ml_odds(event_id, home_odds, away_odds, draw_odds):
    result = make_common_header(OPCODE_BTX_UPDATE_ODDS)
    result = result + encode_int_little_endian(event_id, 4)
    result = result + encode_int_little_endian(home_odds, 4)
    result = result + encode_int_little_endian(away_odds, 4)
    result = result + encode_int_little_endian(draw_odds, 4)
    return result

# Create a spread event
def make_spread_event(event_id, points, home_odds, away_odds):
    result = make_common_header(OPCODE_BTX_SPREAD_EVENT)
    result = result + encode_int_little_endian(event_id, 4)
    result = result + encode_int_little_endian(points, 2)
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