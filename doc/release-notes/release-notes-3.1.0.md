Wagerr Core version 3.1.0 is now available from:

  <https://github.com/wagerr/wagerr/releases/tag/v3.1.0>

This is a new major version release, including new features, various bug
fixes and performance improvements, as well as updated translations.

Please report bugs using the issue tracker at GitHub:

  <https://github.com/wagerr/wagerr/issues>

How to Upgrade
==============

If you are running an older version, shut it down. Wait until it has
completely shut down (which might take a few minutes for older
versions), then run the installer (on Windows) or just copy over
`/Applications/Wagerr-Qt` (on Mac) or `wagerrd`/`wagerr-qt` (on
Linux).

Compatibility
=============

Wagerr Core is supported and extensively tested on operating systems
using the Linux kernel, macOS 10.10+, and Windows 7 and newer. It is not
recommended to use Wagerr Core on unsupported systems.

Wagerr Core should also work on most other Unix-like systems but is not
as frequently tested on them.

From 3.0.0 onwards, macOS <10.10 is no longer supported. 3.0.0 is
built using Qt 5.9.x, which doesn't support versions of macOS older than
10.10. Additionally, Wagerr Core does not yet change appearance when
macOS "dark mode" is activated.

Notable changes
===============

zWGR Public Spends
------------------

Allow zWGR spends to occur using this new public spend method for version 2 zWGR.

It is advised that users spend/convert their existing zWGR to WGR, which can be
done via the GUI or RPC as it was prior to the disabling of zWGR.

Version 2 Stake Modifier
------------------------

A new 256-bit modifier for the proof of stake protocol has been defined,
CBlockIndex::nStakeModifierV2. It is computed at every block, by taking the hash
of the modifier of previous block along with the coinstake input. To meet the
protocol, the PoS kernel must comprise the modifier of the previous block.

Changeover enforcement of this new modifier is set to occur at block XXX for
testnet and block 891276 for mainnet.

New getbet RPC method
---------------------

Introduce a new getbet RPC method that returns the details of a bet with the
matching transaction hash.

3.1.0 change log
================

- New pivx patches from commit 2eacdb7f46673de758dd273a53effe2 - Regtest now stakes past zeroCoin start - kernel log updates (ckti)
- build snap for qt only (ckti)
- Set correct amounts of fake serial coins and other static coin supply data (Kokary)
- [FIX] Correct the functions that count WGR supply (Kokary)
- Update testnet params for new testnet chain (Kokary)
- Upstream updates, including new stake modifier (Kokary)
- Implemented getbet RPC API for light clients (Cryptarchist)
- [FIX] Update handling of spreads and totals (Kokary)

Credits
=======

Thanks to everyone who directly contributed to this release:

- 3reioDev
- briandiolun
- CaveSpectre11
- cevap
- Chun Kuan Lee
- ckti
- Cryptarchist
- CryptoKnight
- David Mah
- Emil
- furszy
- Fuzzbawls
- Kokary
- Martin Bullman
- Nitya Sattva
- Patch 3.0
- Patrick Strateman
- presstab
- random-zebra
- rdelune
- Rocco Stanzione
- TechSquad
- Wagerr Developer
- wagerrdeveloper
- wagerrtor
- warrows

As well as everyone that helped translating on [Transifex](https://www.transifex.com/projects/p/wagerr/).
