Wagerr Core version 2.0.1 is now available from:

  <https://github.com/wagerr/wagerr/releases/tag/v2.0.1>

This is a new minor version release, with various bugfixes.

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

**Currently there are known issues with the Gitian release for macOS 10.13
and you must download the version labelled for High Sierra.**

Notable changes
===============

GUI Fixes
---------

Many GUI cleanups and fixes since the release of Wagerr “On-Chain” betting.

- Rather than just warning the user to not bet on 0 or N/A odds, disable the
  buttons in the GUI instead.
- Fix the counting of locked Masternode coins in the overview screen
- When placing a bet show the correct notification text

zWGRv1 Spending
---------------

You can now spend your zWGRv1! This mean you can convert it back to WGR and then
into zWGRv2.

Build Signing Process
---------------------

Fixes to improve the build signing process. Previously builds were being signed
outside of the Gitian building process. We now bring this functionality into our
Gitian build process.

2.0.1 change log
----------------

- `3241709` Make default issue text all comments to make issues more readable (Cryptarchist)
- `affd039` Fix GetLockedCredit() to count masternode coins once for the locked coins balance WGRCORE-105 #done (Kokary)
- `032e6a9` [WGRCORE-102] Add extended balance RPC call #done (Kokary)
- `07461f5` Disable buttons to accidentally burn coins (Rocco Stanzione)
- `14665bf` Grey out disabled bet buttons (Rocco Stanzione)
- `71670e0` Remove unnecessary namespace declarations (Fuzzbawls)
- `268f83f` Remove boost dependency (Fuzzbawls)
- `0c5dc5d` Clean up header includes (Fuzzbawls)
- `7fa2c10` Add proper translation functions for user facing strings (Fuzzbawls)
- `2b3c1c6` Remove useless returns (Fuzzbawls)
- `071e54c` Comment/Whitespace/nullptr cleanup (Fuzzbawls)
- `5364bb9` release: add win detached sig creator and our cert chain (Cryptarchist)
- `2eee0c1` release: create a bundle for the new signing script (Cryptarchist)
- `058943a` [WGRCORE-106] Add RPC call for spending zWGR with coin control (Kokary)
- `699e4b9` Build: Fix macOS signing (Cryptarchist)
- `879ba10` Contrib: Replace developer keys with list of pgp fingerprints (Cryptarchist)
- `58be889` Updated checkpoint handling to enable zWGRv1 spending (Kokary)

Credits
=======

Thanks to everyone who directly contributed to this release:

- Cryptarchist
- Fuzzbawls
- Kokary
- Rocco Stanzione

As well as everyone that helped translating on [Transifex](https://www.transifex.com/projects/p/wagerr/).
