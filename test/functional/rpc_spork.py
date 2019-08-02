#!/usr/bin/env python3
# Copyright (c) 2019 The PIVX developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
# -*- coding: utf-8 -*-

from test_framework.test_framework import BitcoinTestFramework

class WGR_RPCSporkTest(BitcoinTestFramework):
    def set_test_params(self):
        self.setup_clean_chain = True
        self.num_nodes = 1
        self.extra_args = [['-staking=1', '-sporkkey=6xLZdACFRA53uyxz8gKDLcgVrm5kUUEu2B3BUzWUxHqa2W7irbH']]

    def printDict(self, d):
        self.log.info("{")
        for k in d:
            self.log.info("  %s = %d" % (k, d[k]))
        self.log.info("}")


    def run_test(self):
        self.description = "Performs tests on the Spork RPC"
        # check spork values:
        sporks = self.nodes[0].spork("show")
        self.printDict(sporks)
        active = self.nodes[0].spork("active")
        assert(not active["SPORK_8_MASTERNODE_PAYMENT_ENFORCEMENT"])
        # activate SPORK 8
        new_value = 1563253447
        res = self.nodes[0].spork("SPORK_8_MASTERNODE_PAYMENT_ENFORCEMENT", new_value)
        assert(res == "success")
        sporks = self.nodes[0].spork("show")
        self.printDict(sporks)
        assert(sporks["SPORK_8_MASTERNODE_PAYMENT_ENFORCEMENT"] == new_value)
        active = self.nodes[0].spork("active")
        assert (active["SPORK_8_MASTERNODE_PAYMENT_ENFORCEMENT"])



if __name__ == '__main__':
    WGR_RPCSporkTest().main()
