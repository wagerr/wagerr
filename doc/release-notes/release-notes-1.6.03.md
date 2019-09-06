Wagerr Core version *1.6.03* is now available from:

  <https://github.com/wagerr/wagerr/releases/tag/v1.6.03>

This is a new major version release, including various bugfixes and
performance improvements, as well as updated translations.

Please report bugs using the issue tracker at GitHub:

  <https://github.com/wagerr/wagerr/issues>

How to Upgrade
==============

If you are running an older version, shut it down. Wait until it has completely
shut down (which might take a few minutes for older versions), then run the
installer (on Windows) or just copy over `/Applications/Wagerr-Qt` (on Mac)
or `wagerrd`/`wagerr-qt` (on Linux).

Downgrading warning
-------------------

This release contains new consensus rules and improvements that are not
backwards compatible with older versions. Users will have a grace period of one
week to update their clients before enforcement of this update is enabled.

Users updating from a previous version after (INSERT DATE HERE)
will require a full resync of their local blockchain from either the P2P network
or by way of the bootstrap.

Compatibility
==============

Wagerr Core is extensively tested on multiple operating systems using
the Linux kernel, macOS 10.8+, and Windows Vista and later. Windows XP is not supported.

Wagerr Core should also work on most other Unix-like systems but is not
frequently tested on them.

macOS 10.13 warning
-------------------

Currently there are known issues with the 1.6.0 Gitian release for macOS 10.13
and you must download the version labelled for High Sierra.

Notable changes
===============

Betting
------------

- Main parameters for betting moved to chain parameters
- Cleaned up betting part by exporting variables into chain parameters

Bugs fixed
------------

there was a bug in validating bets:

 - [x] `reward pays too much and correct last pow`
 - [x] Sync testnet and main net without protocol restriction, allowing 70918

1.6.03 Change log
================

Credits
=======

Thanks to everyone who directly contributed to this release:

- Anthony
- Anthony Posselli
- blondfrogs
- Cryptarchist
- David Mah
- Fuzzbawls
- gpdionisio
- Jeremy
- Kokary
- Mrs-X
- Nitya Sattva
- Patrick Strateman
- presstab
- rejectedpromise
- Rocco Stanzione
- SHTDJ
- WagerrTor
- Warrows

As well as everyone that helped translating on [Transifex](https://www.transifex.com/projects/p/wagerr/).
