#!/usr/bin/env python3
# Copyright (c) 2018 The WAGERR developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test RPC commands for BIP38 encrypting and decrypting addresses."""

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import assert_equal

class Bip38Test(BitcoinTestFramework):
    def set_test_params(self):
        self.setup_clean_chain = True
        self.num_nodes = 2

    def run_test(self):
        password = 'test'
        address = self.nodes[0].getnewaddress()
        privkey = self.nodes[0].dumpprivkey(address)

        self.log.info('encrypt address %s' % (address))
        bip38key = self.nodes[0].bip38encrypt(address, password)['Encrypted Key']

        self.log.info('decrypt bip38 key %s' % (bip38key))
        assert_equal(self.nodes[1].bip38decrypt(bip38key, password)['Address'], address)

if __name__ == '__main__':
    Bip38Test().main()
