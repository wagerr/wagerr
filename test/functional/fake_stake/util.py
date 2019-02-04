#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import subprocess

from test_framework.messages import CTransaction, CTxIn, CTxOut, COutPoint, COIN
from test_framework.messages import msg_getheaders, msg_headers, CBlockHeader
from test_framework.mininode import P2PInterface, mininode_lock
from test_framework.script import CScript
from test_framework.util import wait_until

''' -------------------------------------------------------------------------
TestNode CLASS --------------------------------------------------------------

A peer we use to send messsages to wagerrd and store responses
Extends P2PInterface.
'''

# TestNode: A peer we use to send messages to bitcoind, and store responses.
class TestNode(P2PInterface):
    def __init__(self):
        super().__init__()
        self.last_sendcmpct = []
        self.block_announced = False
        # Store the hashes of blocks we've seen announced.
        # This is for synchronizing the p2p message traffic,
        # so we can eg wait until a particular block is announced.
        self.announced_blockhashes = set()

    def on_sendcmpct(self, message):
        self.last_sendcmpct.append(message)

    def on_cmpctblock(self, message):
        self.block_announced = True
        self.last_message["cmpctblock"].header_and_shortids.header.calc_sha256()
        self.announced_blockhashes.add(self.last_message["cmpctblock"].header_and_shortids.header.sha256)

    def on_headers(self, message):
        self.block_announced = True
        for x in self.last_message["headers"].headers:
            x.calc_sha256()
            self.announced_blockhashes.add(x.sha256)

    def on_inv(self, message):
        for x in self.last_message["inv"].inv:
            if x.type == 2:
                self.block_announced = True
                self.announced_blockhashes.add(x.hash)

    # Requires caller to hold mininode_lock
    def received_block_announcement(self):
        return self.block_announced

    def clear_block_announcement(self):
        with mininode_lock:
            self.block_announced = False
            self.last_message.pop("inv", None)
            self.last_message.pop("headers", None)
            self.last_message.pop("cmpctblock", None)

    def get_headers(self, locator, hashstop):
        msg = msg_getheaders()
        msg.locator.vHave = locator
        msg.hashstop = hashstop
        self.connection.send_message(msg)

    def send_header_for_blocks(self, new_blocks):
        headers_message = msg_headers()
        headers_message.headers = [CBlockHeader(b) for b in new_blocks]
        self.send_message(headers_message)

    def request_headers_and_sync(self, locator, hashstop=0):
        self.clear_block_announcement()
        self.get_headers(locator, hashstop)
        wait_until(self.received_block_announcement, timeout=30, lock=mininode_lock)
        self.clear_block_announcement()

    # Block until a block announcement for a particular block hash is received.
    def wait_for_block_announcement(self, block_hash, timeout=30):
        def received_hash():
            return (block_hash in self.announced_blockhashes)
        wait_until(received_hash, timeout=timeout, lock=mininode_lock)

    def send_await_disconnect(self, message, timeout=30):
        """Sends a message to the node and wait for disconnect.

        This is used when we want to send a message into the node that we expect
        will get us disconnected, eg an invalid block."""
        self.send_message(message)
        wait_until(lambda: not self.connected, timeout=timeout, lock=mininode_lock)



''' -------------------------------------------------------------------------
MISC METHODS ----------------------------------------------------------------
'''

def dir_size(path):
    ''' returns the size in bytes of the directory at given path
    '''
    size = subprocess.check_output(['du','-shk', path]).split()[0].decode('utf-8')
    return int(size)


def create_transaction(outPoint, sig, value, nTime, scriptPubKey=CScript()):
    ''' creates a CTransaction object provided input-output data
    '''
    tx = CTransaction()
    tx.vin.append(CTxIn(outPoint, sig, 0xffffffff))
    tx.vout.append(CTxOut(value, scriptPubKey))
    tx.nTime = nTime
    tx.calc_sha256()
    return tx


def utxos_to_stakingPrevOuts(utxolist, blocktime):
    '''
    Creates a map of unspent outputs to (amount, blocktime) to be used as stake inputs
    :param   utxolist:          list of JSON object returned from listunspent
    :return  stakingPrevOuts:   ({COutPoint --> (int, int)} dictionary)
    '''
    stakingPrevOuts = {}
    COINBASE_MATURITY = 100
    for utxo in utxolist:
        if utxo['confirmations'] > COINBASE_MATURITY:
            outPoint = COutPoint(int(utxo['txid'], 16), utxo['vout'])
            stakingPrevOuts[outPoint] = (int(utxo['amount'])*COIN, blocktime)

    return stakingPrevOuts


def assert_not_equal(thing1, thing2):
    if thing1 == thing2:
        raise AssertionError("not(%s)" % " != ".join(str(arg) for arg in (thing1, thing2)))
