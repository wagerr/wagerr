#!/bin/bash
# Copyright (c) 2013-2014 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

BUILDDIR="/media/dev/M2SSD/go/github.com/wagerr/wagerr"
EXEEXT=""

# These will turn into comments if they were disabled when configuring.
ENABLE_WALLET=1
ENABLE_UTILS=1
ENABLE_BITCOIND=1

REAL_BITCOIND="$BUILDDIR/src/wagerrd${EXEEXT}"
REAL_BITCOINCLI="$BUILDDIR/src/wagerr-cli${EXEEXT}"

