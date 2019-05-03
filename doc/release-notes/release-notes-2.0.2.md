Wagerr Core version 2.0.2 is now available from:

  <https://github.com/wagerr/wagerr/releases/tag/v2.0.2>

This is a new patch version release, with various bugfixes.

Please report bugs using the issue tracker at GitHub:

  <https://github.com/wagerr/wagerr/issues>

How to Upgrade
==============

If you are running an older version, shut it down. Wait until it has completely
shut down (which might take a few minutes for older versions), then run the
installer (on Windows) or just copy over `/Applications/Wagerr-Qt` (on Mac)
or `wagerrd`/`wagerr-qt` (on Linux).

Upgrade Warning
---------------

Before any upgrade we recommend that you securely backup your wallet. Please see
the [Wagerr help desk article](https://wagerr.zendesk.com/hc/en-us/articles/360001309872-How-to-backup-and-restore-Wagerr-dat-and-private-keys)
for more information.

Compatibility
=============

Wagerr Core is extensively tested on multiple operating systems using
the Linux kernel, macOS 10.8+, and Windows Vista and later. Windows XP is not supported.

Wagerr Core should also work on most other Unix-like systems but is not
frequently tested on them.

**Currently there are known issues with the Gitian release for macOS 10.13 and
above and you must download the version labelled for your macOS.**

Notable changes
===============

RPC Fixes
---------

When listing events only look back the past 16 days worth of blocks.

This should significantly improve the `listevents` command as it no longer
scans from BetStartHeight for events. This is the same logic used to payout
bets.

2.0.2 change log
----------------

- `177ec6e` When listing events only look back the past 16 days worth of blocks (Cryptarchist)

Credits
=======

Thanks to everyone who directly contributed to this release:

- Cryptarchist

As well as everyone that helped translating on [Transifex](https://www.transifex.com/projects/p/wagerr/).
