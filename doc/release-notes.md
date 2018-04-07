(note: this is a temporary file, to be added-to by anybody, and moved to release-notes at release time)

Wagerr Core version *version* is now available from:

  <https://github.com/wagerr/wagerr/releases>

This is a new major version release, including various bug fixes and
performance improvements, as well as updated translations.

Please report bugs using the issue tracker at github:

  <https://github.com/wagerr/wagerr/issues>

Autocombine changes
---------------------------------
The autocombine feature was carrying a bug leading to a significant CPU overhead
when being used. The function is now called only once initial blockchain
download is finished. It's also now avoiding to combine several times while
under the threshold in order to avoid additional transaction fees. Last but not
least, the fee computation as been changed and the dust from fee provisioning
is returned in the main output.


SOCKS5 Proxy bug
---------------------------------
When inputting wrong data into the GUI for a SOCKS5 proxy, the wallet would
crash and be unable to restart without accessing hidden configuration.
This crash has been fixed.

High Sierra build
-----------------
Removed warning

Other notable changes:
----------------------
- Version
  - 1.5.0 and new checkpoint https://github.com/wagerr/wagerr/commit/4b7e26fc3f4342342ab3fdc7a2c07beee862aa18
- QT
   - connect automint icon https://github.com/wagerr/wagerr/commit/12c6aced2da5609e718390d08dc9b6c5c411933d
  - Update privacy tab info about zeromint on config change https://github.com/wagerr/wagerr/commit/d3c45d0104d760c33b3d2707d39a60dd6c0f152d
  - Refresh xWGR balance after resetting mints or spends https://github.com/wagerr/wagerr/commit/514750e9c05a39a303c80e1d69e6e88e62637ff7
- Wallet 
  - Add a check on zWGR spend to avoid a segfault https://github.com/wagerr/wagerr/commit/903d3b80227bc398e3099f4b0247e99a2bf23b2d
  - Increase valid range for automint percentage https://github.com/wagerr/wagerr/commit/df3bdbc7612651b2fb8e6090aadfa27b0663a877
  - Add argument to mint zerocoin from specific UTXO https://github.com/wagerr/wagerr/commit/766d425e5c9f02897a51a647d1f38f0ac4a16d2d
- Consensus
  - Fix compilation with OpenSSL 1.1 https://github.com/wagerr/wagerr/commit/e9bcf324cdd18b4ee887338e2f328b5c0f79bdb8
  - Require standard transactions for testnet https://github.com/wagerr/wagerr/commit/a86d106954e0e7836ba6581eb2267a81a0f9e9a2
- Trivial
  - Add debug info for CWallet::ReconsiderZerocoins() https://github.com/wagerr/wagerr/commit/867d8735f505b70086ac39dcb095de7e165d84c8
  - Fix errant LogPrint in UpdateZWGRSupply https://github.com/wagerr/wagerr/commit/158b0b53c8f6f1e81d404bd64fdf0d9be69d877a
- GUI
  - Make "For anonymization and staking only" checked by default https://github.com/wagerr/wagerr/commit/49450f06d9f23a84479bae8962faab8cc67471eb
  - Zeromint status bar icon https://github.com/wagerr/wagerr/commit/13e412015f4ce0468336d891821e65864f42dd59
- Documentation
  - Improve help for mintzerocoin rpc command https://github.com/wagerr/wagerr/commit/1f6802fc9da8760dcc530be5b1777d35baf9ab52
- Core
  - Remove Gitan-OSX warning for High Sierra builds https://github.com/wagerr/wagerr/commit/fe4c2598143c7157e0b179cf7e4a55b12660ea83
- Bugs fixed
  - Segfault with -enableswifttx=0 / -enableswifttx=false https://github.com/wagerr/wagerr/commit/6e982101923c9d38fa7f45eb808e7718118ad3a5
  - Listtransactions bug https://github.com/wagerr/wagerr/commit/8f5c430f4535c8cad6f10128329385947c4785b1
- Other changes
  - Instructions on how to make the Homebrew OpenSSL headers visible https://github.com/wagerr/wagerr/commit/359e5c862f9502d1a5d1738365a9bd094837f360
  - Correct zerocoin lavel https://github.com/wagerr/wagerr/commit/706629ed593b1241bcd4fbd916121397dcc59fe4
  - Change git info in genbuild.sh https://github.com/wagerr/wagerr/commit/92dcfdb07e3d22fa3a8da72d53e892df95e7a426


How to Upgrade
==============

If you are running an older version, shut it down. Wait until it has completely shut down (which might take a few minutes for older versions), then run the installer (on Windows) or just copy over /Applications/Wagerr-Qt (on Mac) or wagerrd/wagerr-qt (on Linux).

Compatibility
==============

Wagerr Core is extensively tested on multiple operating systems using
the Linux kernel, macOS 10.8+, and Windows Vista and later.

Microsoft ended support for Windows XP on [April 8th, 2014](https://www.microsoft.com/en-us/WindowsForBusiness/end-of-xp-support),
No attempt is made to prevent installing or running the software on Windows XP, you
can still do so at your own risk but be aware that there are known instabilities and issues.
Please do not report issues about Windows XP to the issue tracker.

Wagerr Core should also work on most other Unix-like systems but is not
frequently tested on them.

Notable Changes
===============

Random-cookie RPC authentication
---------------------------------

When no `-rpcpassword` is specified, the daemon now uses a special 'cookie'
file for authentication. This file is generated with random content when the
daemon starts, and deleted when it exits. Its contents are used as
authentication token. Read access to this file controls who can access through
RPC. By default it is stored in the data directory but its location can be
overridden with the option `-rpccookiefile`.

This is similar to Tor's CookieAuthentication: see
https://www.torproject.org/docs/tor-manual.html.en

This allows running wagerrd without having to do any manual configuration.


*version* Change log
=================

Detailed release notes follow. This overview includes changes that affect
behavior, not code moves, refactors and string updates. For convenience in locating
the code changes and accompanying discussion, both the pull request and
git merge commit are mentioned.

### Broad Features
### P2P Protocol and Network Code
### GUI
### Miscellaneous

Credits
=======

Thanks to everyone who directly contributed to this release:


As well as everyone that helped translating on [Transifex](https://www.transifex.com/projects/p/wagerr-translations/).
