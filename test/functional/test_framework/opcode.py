#!/usr/bin/env python
# -*- coding: utf-8 -*-

import os
import struct
import subprocess

DEBUG_MODE = False
OPCODE_PREFIX = 42
OPCODE_VERSION = 1
OPCODE_BTX_MAPPING = 1
OPCODE_BTX_MONEYLINE_EVENT = 2
OPCODE_BTX_MONEYLINE_PATCH = 11
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
    mapping_id_size = 2 if namespace_id > 0 and namespace_id < 4 else 4
    result = make_common_header(OPCODE_BTX_MAPPING)
    result = result + encode_int_little_endian(namespace_id, 1)
    result = result + encode_int_little_endian(mapping_id, mapping_id_size)
    for sym in mapping_name:
        result = result + encode_int_little_endian(ord(sym), 1)
    result = result + encode_int_little_endian(0, 1)
    return result

# Create a moneyline patch opcode.
def make_mlpatch(event_id, timestamp):
    result = make_common_header(OPCODE_BTX_MONEYLINE_PATCH)
    result = result + encode_int_little_endian(event_id, 4)
    result = result + encode_int_little_endian(timestamp, 4)
    return result

# Create a moneyline event opcode.
def make_mlevent(event_id, timestamp, sport, tournament, round, home_team, away_team, home_odds, away_odds, draw_odds):
    result = make_common_header(OPCODE_BTX_MONEYLINE_EVENT)
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

def get_utxo_list(node, address):
    utxo_array = []
    total_amount = float(0.00)
    min_amount = WGR_TX_FEE

    # Find enough UTXO to use in a spend transaction to cover the minimum amount.
    print(node.listunspent())
    for utxo in node.listunspent(1, 9999999, [address]):
        utxo_array.append({'txid': utxo["txid"], 'vout': utxo["vout"]})
        total_amount += float(utxo['amount'])
        if total_amount > min_amount:
            break

    return utxo_array, total_amount

def post_opcode(node, opcode, address):
    # Get unspent outputs to use as inputs (spend).
    inputs, spend = get_utxo_list(node, address)
    # Calculate the change by subtracting the transaction fee from the UTXO's value.
    change = float(spend) - float(WGR_TX_FEE)
    # Create the output JSON
    outputs = {address: change, 'data': opcode}

    print('inputs: ', inputs)
    print('outputs: ', outputs)

    # Create the raw transaction.
    trx = node.createrawtransaction(inputs, outputs)
    # Sign the raw transaction.
    trx = node.signrawtransaction(trx)
    return node.sendrawtransaction(trx['hex'])
