Wagerr Core version 3.0.1 is now available from:

  <https://github.com/wagerr/wagerr/releases/tag/v3.0.1>

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

3.0.1 change log
================

- Add in port for regtest at 55007 (ckti)
- Update Wagerr graphics for Atlantic City release (wagerrdeveloper)
- Add in Oracle addresses (ckti)
- Replace CLTV (BIP65) soft fork check with its corresponding activation heights (Kokary)

Credits
=======

Thanks to everyone who directly contributed to this release:

- ckti
- Cryptarchist
- Kokary
- wagerrdeveloper

As well as everyone that helped translating on [Transifex](https://www.transifex.com/projects/p/wagerr/).
