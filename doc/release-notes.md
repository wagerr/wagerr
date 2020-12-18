
(note: this is a temporary file, to be added-to by anybody, and moved to release-notes at release time)

# WAGERR Core version 4.0.0

Release is now available from:

  <https://github.com/wagerr/wagerr/releases>

This is a new major version release, bringing major performance and stability improvements, new
features, various bugfixes and other improvements.

Please report bugs using the issue tracker at github:

  <https://github.com/wagerr/wagerr/issues>


## Upgrading and downgrading

### Mandatory upgrade

Version 4 introduces changes in the consensus protocol and as such this is a mandatory upgrade.

After the fork, clients with prior version (3.1 and lower) will not be able to participate to the Wagerr chain.

Clients need to fully resync or reindex the blockchain.

### How to Upgrade

If you are running an older version, shut it down. Wait until it has completely
shut down (which might take a few minutes for older versions), then run the
installer (on Windows) or just copy over /Applications/Wagerr-Qt (on Mac) or
wagerr/wagerr-qt (on Linux). If you were using version < 4.0.0 you will have to reindex
(start with -reindex) or resync (start with -resync) to make sure your wallet has all
the new data synced.

## Downgrade warning

### Downgrade to a version < 4.0.0

Downgrading to a version smaller than 3.0 is not supported anymore as the Wagerr Betting
Protocol V3 activated on mainnet and testnet.

### Compatibility

WAGERR Core is extensively tested on multiple operating systems using the Linux kernel, macOS 10.10+, and Windows 7 and later.

Microsoft ended support for Windows XP on [April 8th, 2014](https://www.microsoft.com/en-us/WindowsForBusiness/end-of-xp-support), No attempt is made to prevent installing or running the software on Windows XP, you can still do so at your own risk but be aware that there are known instabilities and issues. Please do not report issues about Windows XP to the issue tracker.

Apple released it's last Mountain Lion update August 13, 2015, and officially ended support on [December 14, 2015](http://news.fnal.gov/2015/10/mac-os-x-mountain-lion-10-8-end-of-life-december-14/). WAGERR Core software starting with v3.2.0 will no longer run on MacOS versions prior to Yosemite (10.10). Please do not report issues about MacOS versions prior to Yosemite to the issue tracker.

WAGERR Core should also work on most other Unix-like systems but is not frequently tested on them.

## Notable changes

Wagerr introduces version 3 of its betting protocol. The key changes are:
- Introduction of parlay betting.
- Increase of maximum event duration from 2 weeks to 2 months.
- Better handling of favorites identification.

4.0 introduces a fully rewritten database layer for the processing of betting activities. As a result:
- All betting activities are handled by the core client's (cached) internal database.
- Transaction and block processing is significantly faster. Block synchronization now averages at 7 hours for a full sync.
- Improved stability due to better handlign of chain reorganizations, fixing instability issues in the 3.x clients.
- All betting data from the Wagerr blockchain are made available through the core client now, including placed bets,
  bet result statuses, odds, spread amounts, total amounts, and score outcome of matches.
- New commands include: getmybets, getbetbytxid "txid", getpayoutinfo "txid".

### Core Features

- A new block time algorithm has been ported from upstream. Most noticable to the end user is that the time window for
  block creation has been made smaller, resulting in a lower variation in block times. Note that staking clients now
  need to have a somewhat accurate system clock.
- Minimum maturity for spending bet winnings has decreased from 100 blocks to 60 blocks.
- Support for public spending of zWGR.
- Rotation of spork keys and Oracle keys.

### RPC/REST
-----------
There are a few changes in existing RPC interfaces in this release:
- `geteventsliability` is renamed to `geteventliability`

There are also new RPC commands:
- `abandontransaction`
- `createrawzerocoinspend`
- `getallbets`
- `getalleventliabilities`
- `getallqgbets`
- `getbetbytxid`
- `getmybets`
- `getmyqgbets`
- `getpayoutinfo`
- `getpayoutinfosince`
- `listbetsdb`
- `listeventsdebug`
- `placeparlaybet`

Few RPC commands are no longer supported: `clearspendcache`, `placechaingamesbet`, `createrawzerocoinpublicspend`

See `help command` in rpc for more info.

## Miscellaneous

A lot of refactoring, backports, code cleanups and other small fixes were done in this release.

## 4.0.0 Change log

See detailed [set of changes](https://github.com/wagerr/wagerr/compare/v4.0.0...wagerr:v3.1.0).

Akshay (1):
* [`2f6c3674`](https://github.com/wagerr/wagerr/commit/2f6c3674) Fix AA_EnableHighDpiScaling warning

Alex Morcos (5):
* [`81faea01`](https://github.com/wagerr/wagerr/commit/81faea01) Make sure conflicted wallet tx's update balances
* [`de3a7a2a`](https://github.com/wagerr/wagerr/commit/de3a7a2a) Make wallet descendant searching more efficient
* [`e6e058b9`](https://github.com/wagerr/wagerr/commit/e6e058b9) Add new rpc call: abandontransaction
* [`997e93d6`](https://github.com/wagerr/wagerr/commit/997e93d6) Flush wallet after abandontransaction
* [`bd40b936`](https://github.com/wagerr/wagerr/commit/bd40b936) Fix calculation of balances and available coins.

Andrew Chow (3):
* [`233c6dbe`](https://github.com/wagerr/wagerr/commit/233c6dbe) build: if VERSION_BUILD is non-zero, include it in the package version
* [`dfd6c5ee`](https://github.com/wagerr/wagerr/commit/dfd6c5ee) build: include rc number in version number
* [`c7b86b46`](https://github.com/wagerr/wagerr/commit/c7b86b46) Update release-process.md to include RC version bumping

Ben Woosley (1):
* [`a712a131`](https://github.com/wagerr/wagerr/commit/a712a131) Fix that CWallet::AbandonTransaction would only traverse one level

CkTi (18):
* [`fb327b9b`](https://github.com/wagerr/wagerr/commit/fb327b9b) Change chainparams to 152000
* [`ac37a9b1`](https://github.com/wagerr/wagerr/commit/ac37a9b1) Update help for getmappingname and getmappingid
* [`1afd9508`](https://github.com/wagerr/wagerr/commit/1afd9508) Update travis to not run binary tests
* [`974bd622`](https://github.com/wagerr/wagerr/commit/974bd622) Enable CircleCI, disable Travis
* [`d4b8075e`](https://github.com/wagerr/wagerr/commit/d4b8075e) Enable CircleCI, disable Travis
* [`f421c145`](https://github.com/wagerr/wagerr/commit/f421c145) Add in make check and test running
* [`7bb0fbc1`](https://github.com/wagerr/wagerr/commit/7bb0fbc1) REmove bench tests to run under src/test/test_wagerr
* [`8c4ddfda`](https://github.com/wagerr/wagerr/commit/8c4ddfda) Clean config
* [`372d34b9`](https://github.com/wagerr/wagerr/commit/372d34b9) Add in tests to i686, xenial and focal
* [`f0af3dd7`](https://github.com/wagerr/wagerr/commit/f0af3dd7) Uncomment snapcraft
* [`54f121cb`](https://github.com/wagerr/wagerr/commit/54f121cb) Re-comment snapcraft
* [`38cb3717`](https://github.com/wagerr/wagerr/commit/38cb3717) Revert "Fix AA_EnableHighDpiScaling warning"
* [`0d0032ab`](https://github.com/wagerr/wagerr/commit/0d0032ab) Update snapcraft version to 4.0.99
* [`b0886fa8`](https://github.com/wagerr/wagerr/commit/b0886fa8) Update snapcraft to build ckti-master-wagerr
* [`c034bf0a`](https://github.com/wagerr/wagerr/commit/c034bf0a) Update snapcraft to build snap-version
* [`bc398347`](https://github.com/wagerr/wagerr/commit/bc398347) Revert "Update snapcraft to build ckti-master-wagerr"
* [`3888c89a`](https://github.com/wagerr/wagerr/commit/3888c89a) Revert "Update snapcraft to build snap-version"
* [`74afef97`](https://github.com/wagerr/wagerr/commit/74afef97) Update timestamps in tests to 1902871341

Cory Fields (2):
* [`1d7f7c03`](https://github.com/wagerr/wagerr/commit/1d7f7c03) random: only use getentropy on openbsd
* [`7aaab55c`](https://github.com/wagerr/wagerr/commit/7aaab55c) random: fix crash on some 64bit platforms

Cryptarchist (2):
* [`e09ed246`](https://github.com/wagerr/wagerr/commit/e09ed246) Fix filename in docs/ (replace spaces) to fix build
* [`701ccc2e`](https://github.com/wagerr/wagerr/commit/701ccc2e) Update client name to Reno for v4 release

Dag Robole (1):
* [`f33e3d62`](https://github.com/wagerr/wagerr/commit/f33e3d62) Fix resource leak

David Mah (3):
* [`d5a6ab57`](https://github.com/wagerr/wagerr/commit/d5a6ab57) Update build-unix.md
* [`318fa717`](https://github.com/wagerr/wagerr/commit/318fa717) update parlay start height and new testnet checkpoint
* [`9a31ffc7`](https://github.com/wagerr/wagerr/commit/9a31ffc7) Updated Testnet Checkpoints and Protocol starting height

Fuzzbawls (3):
* [`7af86e50`](https://github.com/wagerr/wagerr/commit/7af86e50) Update `crypto/common.h` functions to use compat endian header
* [`da708009`](https://github.com/wagerr/wagerr/commit/da708009) validate proposal amount earlier
* [`6ec0de2d`](https://github.com/wagerr/wagerr/commit/6ec0de2d) Remove precomputing

GitStashCore (1):
* [`1b5f8cd4`](https://github.com/wagerr/wagerr/commit/1b5f8cd4) Invalid scripts

Hennadii Stepanov (1):
* [`48598862`](https://github.com/wagerr/wagerr/commit/48598862) build: Suppress -Wdeprecated-copy warnings

James Hilliard (1):
* [`e7ed2296`](https://github.com/wagerr/wagerr/commit/e7ed2296) Check if sys/random.h is required for getentropy on OSX.

Jonas Schnelli (2):
* [`1aafdf9f`](https://github.com/wagerr/wagerr/commit/1aafdf9f) Remove openssl info from init/log and from Qt debug window
* [`e206f097`](https://github.com/wagerr/wagerr/commit/e206f097) Call notification signal when a transaction is abandoned

Kokary (68):
* [`8f5363e7`](https://github.com/wagerr/wagerr/commit/8f5363e7) Spreads event update
* [`51f4ef4a`](https://github.com/wagerr/wagerr/commit/51f4ef4a) Allow databased code to run on the existing chain
* [`473faac5`](https://github.com/wagerr/wagerr/commit/473faac5) Account for blocks without MN payment when extracting bet payments
* [`49b04e74`](https://github.com/wagerr/wagerr/commit/49b04e74) Improved parameter handling for MN payment location
* [`b7ee44d0`](https://github.com/wagerr/wagerr/commit/b7ee44d0) Improved parameter handling for MN payment location
* [`218df4ba`](https://github.com/wagerr/wagerr/commit/218df4ba) Use uint256 to initialize CBigNum instead of uint64_t
* [`fb4a5736`](https://github.com/wagerr/wagerr/commit/fb4a5736) Fix issue for OSX: map needs its key const
* [`6ab0f655`](https://github.com/wagerr/wagerr/commit/6ab0f655) Do not include total bet rewards in MN payout
* [`e94d1cae`](https://github.com/wagerr/wagerr/commit/e94d1cae) Disambiguate between heights of the last block and the current block
* [`9ccd1951`](https://github.com/wagerr/wagerr/commit/9ccd1951) Add legacy home favorite data to parsed tx
* [`3e474efe`](https://github.com/wagerr/wagerr/commit/3e474efe) Correct off-by-one database error for bet payout heights
* [`fd07085f`](https://github.com/wagerr/wagerr/commit/fd07085f) Enable mainnet icon and give regtest its own icon
* [`04c9c635`](https://github.com/wagerr/wagerr/commit/04c9c635) Adapt -resync to new betting data folder structure
* [`353d56cd`](https://github.com/wagerr/wagerr/commit/353d56cd) Add parameter interaction: default to -zapwallettxes=1 when -resync=1 is specified
* [`4e0b5841`](https://github.com/wagerr/wagerr/commit/4e0b5841) Add last block time to information console panel
* [`d20421e4`](https://github.com/wagerr/wagerr/commit/d20421e4) Allow inconsistencies in betting DB to trigger reindexing.
* [`083b57b3`](https://github.com/wagerr/wagerr/commit/083b57b3) Address compiler warnings
* [`eb59f3a4`](https://github.com/wagerr/wagerr/commit/eb59f3a4) Ensure oracle updates are processed after bets
* [`d72ddd4a`](https://github.com/wagerr/wagerr/commit/d72ddd4a) Ensure oracle updates are undone before bets
* [`780c2bc9`](https://github.com/wagerr/wagerr/commit/780c2bc9) Update betting payouts ordering when mining legacy blocks
* [`466c2a4a`](https://github.com/wagerr/wagerr/commit/466c2a4a) Switch to 60 confirmations of coinstake maturity after v3 starts
* [`39569186`](https://github.com/wagerr/wagerr/commit/39569186) Make comparator in legacy payout class const
* [`9342d300`](https://github.com/wagerr/wagerr/commit/9342d300) Set separate start block for reduced coinstake maturity
* [`930f5615`](https://github.com/wagerr/wagerr/commit/930f5615) [FIX] Filter unconfirmed txs before calculating stake depth
* [`3df9e857`](https://github.com/wagerr/wagerr/commit/3df9e857) [FIX][RPC] Update logic to filter watchonly addresses in getmybets
* [`60d076da`](https://github.com/wagerr/wagerr/commit/60d076da) Add wagerr logging category
* [`e5fa9c6e`](https://github.com/wagerr/wagerr/commit/e5fa9c6e) Remove generated files from repository
* [`03c6e936`](https://github.com/wagerr/wagerr/commit/03c6e936) [RPC] Remove wallet lock from getmybets and getmyqgbets
* [`32430660`](https://github.com/wagerr/wagerr/commit/32430660) Bump protocol version number
* [`a479cdee`](https://github.com/wagerr/wagerr/commit/a479cdee) [FIX] Push back V3 block height
* [`06451954`](https://github.com/wagerr/wagerr/commit/06451954) [RPC]Harmonize getmyqgbets - add parameters account and includewatchonly
* [`a1a10119`](https://github.com/wagerr/wagerr/commit/a1a10119) Getbets: Add wildcard to filtering and reverse order of results
* [`11abef9f`](https://github.com/wagerr/wagerr/commit/11abef9f) Fix missing std:: on vector declaration in walletmodel
* [`d2773aac`](https://github.com/wagerr/wagerr/commit/d2773aac) Replace boost::lexical_cast with std::string
* [`16d43be8`](https://github.com/wagerr/wagerr/commit/16d43be8) 0664-0 Renamce fCLTVHasMajority to fCLTVHasMajority
* [`9364d7c7`](https://github.com/wagerr/wagerr/commit/9364d7c7) Update functional test for double spends
* [`368b83c3`](https://github.com/wagerr/wagerr/commit/368b83c3) Add BIP34 fork height
* [`3fd990ca`](https://github.com/wagerr/wagerr/commit/3fd990ca) Add serialization of hashProofOfStake
* [`22fb3e00`](https://github.com/wagerr/wagerr/commit/22fb3e00) Update regtest miner
* [`20e1e2de`](https://github.com/wagerr/wagerr/commit/20e1e2de) Increase betting DB flushing frequency
* [`b3909494`](https://github.com/wagerr/wagerr/commit/b3909494) Clean up height based block reward parameters
* [`17f6fa8a`](https://github.com/wagerr/wagerr/commit/17f6fa8a) Set last accumulator checkpoint param for mainnet
* [`1588da05`](https://github.com/wagerr/wagerr/commit/1588da05) Remove unused code for Qt bet page
* [`13c5a09f`](https://github.com/wagerr/wagerr/commit/13c5a09f) Reduce log spam
* [`97882908`](https://github.com/wagerr/wagerr/commit/97882908) Make oracle settings more flexible
* [`fd8bf4d3`](https://github.com/wagerr/wagerr/commit/fd8bf4d3) Introduce sporks for betting and quick games maintenance
* [`5d9c2d56`](https://github.com/wagerr/wagerr/commit/5d9c2d56) Update keys and rotation
* [`da88cf36`](https://github.com/wagerr/wagerr/commit/da88cf36) Update mainnet checkpoints
* [`f04d958d`](https://github.com/wagerr/wagerr/commit/f04d958d) Release notes - add initial 4.0.0 notes and update process
* [`313fe4a9`](https://github.com/wagerr/wagerr/commit/313fe4a9) Add call 'getalleventliabilities'
* [`a91f338f`](https://github.com/wagerr/wagerr/commit/a91f338f) Update client version nr from 3 to 4
* [`bf9ce443`](https://github.com/wagerr/wagerr/commit/bf9ce443) Update spork keys and tesnet oracle keys
* [`b9adb761`](https://github.com/wagerr/wagerr/commit/b9adb761) Set V3 max event length to 2 months
* [`603174ce`](https://github.com/wagerr/wagerr/commit/603174ce) Lock betting db when reading or flushing
* [`0a278a10`](https://github.com/wagerr/wagerr/commit/0a278a10) [FIX] Add std namespace to test_json
* [`b61984f8`](https://github.com/wagerr/wagerr/commit/b61984f8) [FIX] Update -devnet switch
* [`4c9a70d1`](https://github.com/wagerr/wagerr/commit/4c9a70d1) Disable QuickGames and ChainGames
* [`166ebc4a`](https://github.com/wagerr/wagerr/commit/166ebc4a) Remove trailing whitespace
* [`f5f4ee66`](https://github.com/wagerr/wagerr/commit/f5f4ee66) Reduce memory footprint from zerocoin supply map
* [`95424b5c`](https://github.com/wagerr/wagerr/commit/95424b5c) [RPC] Remove commands to place chaingame and quickgame bets
* [`df7b573a`](https://github.com/wagerr/wagerr/commit/df7b573a) Update release notes
* [`5964e414`](https://github.com/wagerr/wagerr/commit/5964e414) [FIX] Flush betting cache after each block during block verification
* [`a6235ed5`](https://github.com/wagerr/wagerr/commit/a6235ed5) Proper parametrization of pruning betting data
* [`93f0e921`](https://github.com/wagerr/wagerr/commit/93f0e921) Add parlay to listtransactionrecords
* [`e5548d07`](https://github.com/wagerr/wagerr/commit/e5548d07) Update testnet fork params
* [`a271b778`](https://github.com/wagerr/wagerr/commit/a271b778) Update mainnet spork key
* [`07bd817f`](https://github.com/wagerr/wagerr/commit/07bd817f) Update mainnet fork params
* [`de01dacd`](https://github.com/wagerr/wagerr/commit/de01dacd) Add rest endpoints

MarcoFalke (1):
* [`02ade790`](https://github.com/wagerr/wagerr/commit/02ade790) build: use full version string in setup.exe

Matt Corallo (2):
* [`b470d8a6`](https://github.com/wagerr/wagerr/commit/b470d8a6) Add internal method to add new random data to our internal RNG state
* [`fde2fec1`](https://github.com/wagerr/wagerr/commit/fde2fec1) Add perf counter data to GetStrongRandBytes state in scheduler

Mrs-X (1):
* [`fc37ca94`](https://github.com/wagerr/wagerr/commit/fc37ca94) Fix wrong argument when verifying MacOS singatures

Pieter Wuille (20):
* [`b2181a77`](https://github.com/wagerr/wagerr/commit/b2181a77) Always require OS randomness when generating secret keys
* [`bea8f2aa`](https://github.com/wagerr/wagerr/commit/bea8f2aa) Don't use assert for catching randomness failures
* [`5a812be0`](https://github.com/wagerr/wagerr/commit/5a812be0) Maintain state across GetStrongRandBytes calls
* [`f530bcce`](https://github.com/wagerr/wagerr/commit/f530bcce) Add ChaCha20
* [`543244a8`](https://github.com/wagerr/wagerr/commit/543244a8) Introduce FastRandomContext::randbool()
* [`19a542e0`](https://github.com/wagerr/wagerr/commit/19a542e0) Switch FastRandomContext to ChaCha20
* [`96de425e`](https://github.com/wagerr/wagerr/commit/96de425e) Add a FastRandomContext::randrange and use it
* [`cdf64b85`](https://github.com/wagerr/wagerr/commit/cdf64b85) Use hardware timestamps in RNG seeding
* [`dc37f1fe`](https://github.com/wagerr/wagerr/commit/dc37f1fe) Test that GetPerformanceCounter() increments
* [`0b2bf510`](https://github.com/wagerr/wagerr/commit/0b2bf510) Use sanity check timestamps as entropy
* [`832bdcf1`](https://github.com/wagerr/wagerr/commit/832bdcf1) Add FastRandomContext::rand256() and ::randbytes()
* [`b6b5fcef`](https://github.com/wagerr/wagerr/commit/b6b5fcef) Add various insecure_rand wrappers for tests
* [`d3a56390`](https://github.com/wagerr/wagerr/commit/d3a56390) Merge test_random.h into test_bitcoin.h
* [`e3effa50`](https://github.com/wagerr/wagerr/commit/e3effa50) Replace more rand() % NUM by randranges
* [`622d13ae`](https://github.com/wagerr/wagerr/commit/622d13ae) Replace rand() & ((1 << N) - 1) with randbits(N)
* [`f27c3353`](https://github.com/wagerr/wagerr/commit/f27c3353) Use rdrand as entropy source on supported platforms
* [`2d250086`](https://github.com/wagerr/wagerr/commit/2d250086) Use cpuid intrinsics instead of asm code
* [`4bb654bc`](https://github.com/wagerr/wagerr/commit/4bb654bc) Clarify entropy source
* [`47abc1b1`](https://github.com/wagerr/wagerr/commit/47abc1b1) Bugfix: randbytes should seed when needed (non reachable issue)
* [`cd607ab8`](https://github.com/wagerr/wagerr/commit/cd607ab8) Do not permit copying FastRandomContexts

Wladimir J. van der Laan (5):
* [`b1915282`](https://github.com/wagerr/wagerr/commit/b1915282) util: Specific GetOSRandom for Linux/FreeBSD/OpenBSD
* [`eced12e3`](https://github.com/wagerr/wagerr/commit/eced12e3) squashme: comment that NUM_OS_RANDOM_BYTES should not be changed lightly
* [`d024c3ad`](https://github.com/wagerr/wagerr/commit/d024c3ad) sanity: Move OS random to sanity check function
* [`9822f304`](https://github.com/wagerr/wagerr/commit/9822f304) random: Add fallback if getrandom syscall not available
* [`ea2825e0`](https://github.com/wagerr/wagerr/commit/ea2825e0) Kill insecure_random and associated global state

ckti (7):
* [`0e7dc0ee`](https://github.com/wagerr/wagerr/commit/0e7dc0ee) Update snapcraft.yaml
* [`ea02a36d`](https://github.com/wagerr/wagerr/commit/ea02a36d) Update to build s390x and powerpc64le in gitian build This will be necessary to update snapcraft.yaml to use the release binaries to build snap packages on snapcraft store
* [`2017043a`](https://github.com/wagerr/wagerr/commit/2017043a) Change to devel and 3.1.99
* [`ead7d1ae`](https://github.com/wagerr/wagerr/commit/ead7d1ae) Build snap from master
* [`6dff3a94`](https://github.com/wagerr/wagerr/commit/6dff3a94) Updated feature_betting to start at block 300, remove quickgames from test_runner (#21)
* [`24a32437`](https://github.com/wagerr/wagerr/commit/24a32437) Revert "Enable CircleCI, disable Travis"
* [`c5df51cf`](https://github.com/wagerr/wagerr/commit/c5df51cf) Config.yml add in wagerr/wagerr (#221)

dexX7 (1):
* [`be6337b0`](https://github.com/wagerr/wagerr/commit/be6337b0) sort pending wallet transactions before reaccepting

fanquake (1):
* [`b86683ca`](https://github.com/wagerr/wagerr/commit/b86683ca) build: Add CLIENT_VERSION_BUILD to CFBundleGetInfoString

furszy (57):
* [`4a65de59`](https://github.com/wagerr/wagerr/commit/4a65de59) empty views + qr color
* [`534dec06`](https://github.com/wagerr/wagerr/commit/534dec06) lock/unlock/encrypt wallet + contacts widget
* [`154183e8`](https://github.com/wagerr/wagerr/commit/154183e8) convert back zWGR method created + connected to privacy view.
* [`0d0373ed`](https://github.com/wagerr/wagerr/commit/0d0373ed) zerocoin spend flow improved, multi outputs accepted
* [`9feae8cb`](https://github.com/wagerr/wagerr/commit/9feae8cb) send widget refresh view, zwgr tx error flow - wallet model create multi output zwgr tx
* [`a0f4f90d`](https://github.com/wagerr/wagerr/commit/a0f4f90d) send widget, contacts dropdown and change address features connected
* [`a8d25733`](https://github.com/wagerr/wagerr/commit/a8d25733) rebase issues solved.
* [`fc8d4a28`](https://github.com/wagerr/wagerr/commit/fc8d4a28) zWGR name added
* [`468bda9c`](https://github.com/wagerr/wagerr/commit/468bda9c) getWalletTx for tx detail dialog usage.
* [`08093062`](https://github.com/wagerr/wagerr/commit/08093062) payment request connected + getNewAddress method on module
* [`4bd952a0`](https://github.com/wagerr/wagerr/commit/4bd952a0) get address/key creation time method.
* [`c35731c3`](https://github.com/wagerr/wagerr/commit/c35731c3) privacy widget reset mint and reset spends connected.
* [`ba14031c`](https://github.com/wagerr/wagerr/commit/ba14031c) zwgr spend, change address added.
* [`0cf11235`](https://github.com/wagerr/wagerr/commit/0cf11235) tx holder, row index invalid data fix
* [`0ec09cfc`](https://github.com/wagerr/wagerr/commit/0ec09cfc) isWalletUnlocked method created.
* [`d760ba4f`](https://github.com/wagerr/wagerr/commit/d760ba4f) Address table model update amount of send/receive address.
* [`0ffb80a3`](https://github.com/wagerr/wagerr/commit/0ffb80a3) openuridialog buttons style, code cleanup + encryptionStatus event connected.
* [`73ee4ea3`](https://github.com/wagerr/wagerr/commit/73ee4ea3) block zwgr mints from the ui.
* [`249f322a`](https://github.com/wagerr/wagerr/commit/249f322a) wallet first key creation time method implemented.
* [`cd0ab7c6`](https://github.com/wagerr/wagerr/commit/cd0ab7c6) isCoinStake method added.
* [`8a4a6a5c`](https://github.com/wagerr/wagerr/commit/8a4a6a5c) Delete single master node implemented.
* [`e2213348`](https://github.com/wagerr/wagerr/commit/e2213348) no swiftTx by default.
* [`6c9a8b9a`](https://github.com/wagerr/wagerr/commit/6c9a8b9a) dashboard, update chart only with own stakes + cleanup.
* [`65457ff6`](https://github.com/wagerr/wagerr/commit/65457ff6) set wallet default fee method created.
* [`dfa9eb89`](https://github.com/wagerr/wagerr/commit/dfa9eb89) GetKeyCreationTime moved from walletModel into the wallet object + addressTableModel new field for the address creation time.
* [`4b9ec0f7`](https://github.com/wagerr/wagerr/commit/4b9ec0f7) isTestnet method created, abstracting the UI from backend dependencies.
* [`f4ca87f3`](https://github.com/wagerr/wagerr/commit/f4ca87f3) Badly nStakeSplitThreshold set in optionsModel fixed + value moved to constant STAKE_SPLIT_THRESHOLD.
* [`95e15f9d`](https://github.com/wagerr/wagerr/commit/95e15f9d) Master node MISSING status added, only happens when the Master Node has been created on the controller side, added to the masternode.conf and not started. It's not added to the vMasternodes list or any other network list, only lives locally on the masternodeConfig.
* [`56fbcbc5`](https://github.com/wagerr/wagerr/commit/56fbcbc5) isCoinStakeMine method validating against the tx input instead of the output.
* [`deb81f02`](https://github.com/wagerr/wagerr/commit/deb81f02) Send screen, onResetCustomOptions() --> coinControl is cleaned, refresh bottom send amounts.
* [`45120010`](https://github.com/wagerr/wagerr/commit/45120010) Don't continue loading the wallet if the shutdown was requested.
* [`e20960ef`](https://github.com/wagerr/wagerr/commit/e20960ef) * OS memory allocation fail handler. * OS signal handler registration method created to remove code duplication. * AppInitBasicSetup() method created, organizing better the setup step of the wallet initialization.
* [`7367a50a`](https://github.com/wagerr/wagerr/commit/7367a50a) * Stop loading block indexes on wallet startup if shutdown was requested. * Wallet loading, wallet rescan and block index load time logged in a more understandable way.
* [`94220173`](https://github.com/wagerr/wagerr/commit/94220173) #10952 BTC back port. Named "Remove vchDefaultKey and have better first run detection".
* [`4b9c4bda`](https://github.com/wagerr/wagerr/commit/4b9c4bda) Don't continue rescanning the wallet if shutdown was requested.
* [`e7dc3740`](https://github.com/wagerr/wagerr/commit/e7dc3740) Init.cpp, instead of use LogPrint, use error.
* [`14e9635d`](https://github.com/wagerr/wagerr/commit/14e9635d) Use error() instead of LogPrintf + return false.
* [`b76bc581`](https://github.com/wagerr/wagerr/commit/b76bc581) Transaction IsEquivalentTo method backported + Duplicated mempool check code cleanup in IsTrusted method. Comes from bitcoin b2b361926215eadd6bf43ed1d7110b925fc7cae5
* [`55eca61f`](https://github.com/wagerr/wagerr/commit/55eca61f) Fix contextCheckBlock for the first block that it's a v1 block.
* [`3e7e41a4`](https://github.com/wagerr/wagerr/commit/3e7e41a4) Main.cpp code cleanup: * Not used GetInputAgeIX method removed. * AcceptToMemoryPool HasZerocoinSpendInputs code duplication. * Several for loops copying the variable cleaned.
* [`745631dc`](https://github.com/wagerr/wagerr/commit/745631dc) Compiler warnings over the overrided methods fix.
* [`8c9a1e7a`](https://github.com/wagerr/wagerr/commit/8c9a1e7a) Segfault when spork value exceeds the bounds of ctime conversion fixed.
* [`700e8893`](https://github.com/wagerr/wagerr/commit/700e8893) Base58, do not try to parse an empty string on SetString initialization.
* [`4da08b9e`](https://github.com/wagerr/wagerr/commit/4da08b9e) Do not look for address label if the input address is empty
* [`733d51a4`](https://github.com/wagerr/wagerr/commit/733d51a4) WalletModel isMine address, input QString instead of CBitcoinAddress created.
* [`b7485211`](https://github.com/wagerr/wagerr/commit/b7485211) IsOutputAvailable method created in coinsViewCache.
* [`b1a5f3b8`](https://github.com/wagerr/wagerr/commit/b1a5f3b8) IsOutputAvailable badly checking if the output is available fix (was checking if the output is not available).
* [`1c02ae4d`](https://github.com/wagerr/wagerr/commit/1c02ae4d) Masternodes sync, try locking cs_main when it looks for the tip, preventing possible multi-threading shared resource problem.
* [`1277b8d5`](https://github.com/wagerr/wagerr/commit/1277b8d5) Guard chainActive.Tip() and chainActive.Height() methods call.
* [`ec0eb3be`](https://github.com/wagerr/wagerr/commit/ec0eb3be) Reset mn sync process if it's sleep.
* [`9df6dc3e`](https://github.com/wagerr/wagerr/commit/9df6dc3e) Add SPORK 18 to the fMissingSporks flag + move the validation below the old protocol and the connected to ourself check. No need to send the message if we are going to end up disconnecting from the peer.
* [`2193a84d`](https://github.com/wagerr/wagerr/commit/2193a84d) Graceful shutdown in the unlock corrupted wallet, showing the proper error message in screen.
* [`6443c145`](https://github.com/wagerr/wagerr/commit/6443c145) TxRecord updateStatus not accepted stake status fix + performance improvements.
* [`a3a60b94`](https://github.com/wagerr/wagerr/commit/a3a60b94) ZLNP code removed entirely.
* [`f8dafbea`](https://github.com/wagerr/wagerr/commit/f8dafbea) Double index call locking cs_wallet and cs_main twice for every single record fix.
* [`20de6a30`](https://github.com/wagerr/wagerr/commit/20de6a30) Shut down if trying to connect a corrupted block
* [`aa97a2c9`](https://github.com/wagerr/wagerr/commit/aa97a2c9) Solving a dead end inconsistency over the masternode activation process.

practicalswift (2):
* [`8338e2a5`](https://github.com/wagerr/wagerr/commit/8338e2a5) Add attribute [[noreturn]] (C++11) to functions that will not return
* [`e6986151`](https://github.com/wagerr/wagerr/commit/e6986151) Net: Fix resource leak in ReadBinaryFile(...)

presstab (1):
* [`6c3dddcc`](https://github.com/wagerr/wagerr/commit/6c3dddcc) Remove stale wallet transactions on initial load.

random-zebra (111):
* [`3447366f`](https://github.com/wagerr/wagerr/commit/3447366f) Remove Old message format in CMasternodeBroadcast
* [`b4092d08`](https://github.com/wagerr/wagerr/commit/b4092d08) rename SporkKey to SporkPubKey
* [`7d6f1c5d`](https://github.com/wagerr/wagerr/commit/7d6f1c5d) Spork code overhaul classes
* [`2b85a0f2`](https://github.com/wagerr/wagerr/commit/2b85a0f2) fix CreateZerocoinSpendTransaction with empty addressesTo
* [`4657e578`](https://github.com/wagerr/wagerr/commit/4657e578) Introduce constant for maximum CScript length
* [`cf878c41`](https://github.com/wagerr/wagerr/commit/cf878c41) Treat overly long scriptPubKeys as unspendable
* [`82ab4f10`](https://github.com/wagerr/wagerr/commit/82ab4f10) Fix OOM when deserializing UTXO entries with invalid length
* [`459b7895`](https://github.com/wagerr/wagerr/commit/459b7895) CDataStream::ignore Throw exception instead of assert on negative nSize
* [`7c5db029`](https://github.com/wagerr/wagerr/commit/7c5db029) Add tests for CCoins deserialization
* [`9f5294d6`](https://github.com/wagerr/wagerr/commit/9f5294d6) Fix inconsistencies with GetDepthInMainChain
* [`7577bc23`](https://github.com/wagerr/wagerr/commit/7577bc23) Fix bug with coinstake inputs wrongly marked as spent
* [`57e91f02`](https://github.com/wagerr/wagerr/commit/57e91f02) Placeholder block height for activation of new signatures
* [`5379e080`](https://github.com/wagerr/wagerr/commit/5379e080) Define messageSigner and hashSigner classes
* [`0539123a`](https://github.com/wagerr/wagerr/commit/0539123a) New signature for CMasternodePaymentWinner
* [`80e79ce7`](https://github.com/wagerr/wagerr/commit/80e79ce7) MPW: rename SignatureValid() to CheckSignature()
* [`21dcb79b`](https://github.com/wagerr/wagerr/commit/21dcb79b) New signature for CMasternodePing
* [`4fa7c981`](https://github.com/wagerr/wagerr/commit/4fa7c981) Include blockhash in CMasternodePing hash
* [`6243a7ef`](https://github.com/wagerr/wagerr/commit/6243a7ef) New signature for CBudgetVote
* [`16c4c04a`](https://github.com/wagerr/wagerr/commit/16c4c04a) CBudgetVote: rename SignatureValid() to CheckSignature()
* [`d93ab965`](https://github.com/wagerr/wagerr/commit/d93ab965) New signature for CFinalizedBudgetVote
* [`73fb16fb`](https://github.com/wagerr/wagerr/commit/73fb16fb) FBV: rename SignatureValid() to CheckSignature()
* [`ce11900f`](https://github.com/wagerr/wagerr/commit/ce11900f) constructors with arg list for BV and FBV
* [`5880c79a`](https://github.com/wagerr/wagerr/commit/5880c79a) New signature for CConsensusVote (SwiftX)
* [`51388df2`](https://github.com/wagerr/wagerr/commit/51388df2) CConsensusVote: rename SignatureValid() to CheckSignature()
* [`413f2761`](https://github.com/wagerr/wagerr/commit/413f2761) Align the format of the log messages
* [`bcd4905b`](https://github.com/wagerr/wagerr/commit/bcd4905b) New signature for CMasternodeBroadcast
* [`02683aa0`](https://github.com/wagerr/wagerr/commit/02683aa0) rename mnb: sig --> vchSig, VerifySignature --> CheckSignature
* [`24d8f951`](https://github.com/wagerr/wagerr/commit/24d8f951) New signature for CObfuScationRelay
* [`f279e683`](https://github.com/wagerr/wagerr/commit/f279e683) New signature for CSporkMessage
* [`914601a7`](https://github.com/wagerr/wagerr/commit/914601a7) make signatures private variables in their classes
* [`8d7429ad`](https://github.com/wagerr/wagerr/commit/8d7429ad) CFinalizedBudget/Broadcast cleanup constructors
* [`39c6edbc`](https://github.com/wagerr/wagerr/commit/39c6edbc) Add a field 'nMessVersion' to signed messages
* [`b81f7dc7`](https://github.com/wagerr/wagerr/commit/b81f7dc7) add nMessVersion to signature hash
* [`c919b718`](https://github.com/wagerr/wagerr/commit/c919b718) Reject old message versions for CMasternodePaymentWinner
* [`6b11161c`](https://github.com/wagerr/wagerr/commit/6b11161c) Reject old message versions for CSporkMessage
* [`9bc7ae32`](https://github.com/wagerr/wagerr/commit/9bc7ae32) define CSignedMessage parent class
* [`0d4fa2d7`](https://github.com/wagerr/wagerr/commit/0d4fa2d7) CSignedMessage parent of CMasternode and CMasternodePing
* [`7db01f8d`](https://github.com/wagerr/wagerr/commit/7db01f8d) CSignedMessage parent of CMasternodePaymentWinner
* [`7dfaa250`](https://github.com/wagerr/wagerr/commit/7dfaa250) CSignedMessage parent of CBudgetVote/CFinalizedBudgetVote
* [`28d5653d`](https://github.com/wagerr/wagerr/commit/28d5653d) Abstract GetPublicKey in CSignedMessage
* [`06c19419`](https://github.com/wagerr/wagerr/commit/06c19419) CSignedMessage parent of CConsensusVote
* [`91e12c21`](https://github.com/wagerr/wagerr/commit/91e12c21) CSignedMessage parent of CObfuscationRelay
* [`5a41acf5`](https://github.com/wagerr/wagerr/commit/5a41acf5) CSignedMessage parent of CSporkMessage
* [`b15a3c4e`](https://github.com/wagerr/wagerr/commit/b15a3c4e) fNewSigs always defaults to false
* [`956a5a2f`](https://github.com/wagerr/wagerr/commit/956a5a2f) Double signatures swap in CMasternode
* [`9e561efa`](https://github.com/wagerr/wagerr/commit/9e561efa) remove CMasternode explicit constructor from mnb
* [`0f0cbe51`](https://github.com/wagerr/wagerr/commit/0f0cbe51) default public key for mnb
* [`3321f570`](https://github.com/wagerr/wagerr/commit/3321f570) better log for CHashSigner::VerifyHash failures
* [`7689874f`](https://github.com/wagerr/wagerr/commit/7689874f) mnp sigtime and signature check for mnb in CheckAndUpdate
* [`818b494c`](https://github.com/wagerr/wagerr/commit/818b494c) fix startmasternode "alias/missing" relay
* [`2073ae8b`](https://github.com/wagerr/wagerr/commit/2073ae8b) Refactor masternode start/broadcast
* [`f87ebbcd`](https://github.com/wagerr/wagerr/commit/f87ebbcd) mnb: sign the hex representation of the double hash of the data
* [`0bff62f4`](https://github.com/wagerr/wagerr/commit/0bff62f4) Fix messages serialization for version MESS_VER_STRMESS
* [`d76c3c21`](https://github.com/wagerr/wagerr/commit/d76c3c21) decodemasternodebroadcast: add nMessVersion field for MNB and MNP
* [`dc87b105`](https://github.com/wagerr/wagerr/commit/dc87b105) fix a few log lines
* [`e7efb1b8`](https://github.com/wagerr/wagerr/commit/e7efb1b8) Fix error messages: Setkey -> GetKeysFromSecret
* [`64a55496`](https://github.com/wagerr/wagerr/commit/64a55496) Prevent coinstakes from overpaying masternodes
* [`8397c281`](https://github.com/wagerr/wagerr/commit/8397c281) publiccoinspend
* [`ccfc7358`](https://github.com/wagerr/wagerr/commit/ccfc7358) Set zc PublicSpend version v3/v4 via SPORK
* [`8a77a56b`](https://github.com/wagerr/wagerr/commit/8a77a56b) fix parameters check for spendzerocoin (missing ispublicspend)
* [`61955d28`](https://github.com/wagerr/wagerr/commit/61955d28) Lock/UnlockCoin const argument + checks in lockunspent
* [`9ae0dd27`](https://github.com/wagerr/wagerr/commit/9ae0dd27) Unlock spent outputs
* [`7e6987a6`](https://github.com/wagerr/wagerr/commit/7e6987a6) Enable miner with mnsync incomplete
* [`460af8a9`](https://github.com/wagerr/wagerr/commit/460af8a9) Add placeholder block height for Time Protocol V2
* [`c5bfa609`](https://github.com/wagerr/wagerr/commit/c5bfa609) reduce possible nTimeOffset to 15 seconds
* [`505be82e`](https://github.com/wagerr/wagerr/commit/505be82e) Define p2p_time_offset functional test
* [`d67268c9`](https://github.com/wagerr/wagerr/commit/d67268c9) Set placeholders for nBlockV7StartHeight hardfork
* [`63276914`](https://github.com/wagerr/wagerr/commit/63276914) define Block version 7 removing the accumulator checkpoints
* [`db931648`](https://github.com/wagerr/wagerr/commit/db931648) set nBlockLastAccumulatorCheckpoint in chainparams
* [`d78e1381`](https://github.com/wagerr/wagerr/commit/d78e1381) Don't look for checkpoints after nBlockLastAccumulatorCheckpoint
* [`4110b7c2`](https://github.com/wagerr/wagerr/commit/4110b7c2) Check for pindex != nullptr during loops
* [`be1f7035`](https://github.com/wagerr/wagerr/commit/be1f7035) Add p2p_time_offset functional test to test_runner
* [`b63cc3d2`](https://github.com/wagerr/wagerr/commit/b63cc3d2) Fix timedata.cpp includes
* [`f6953207`](https://github.com/wagerr/wagerr/commit/f6953207) Define TestNet changeover block for Wagerr v4.0
* [`c0a83b79`](https://github.com/wagerr/wagerr/commit/c0a83b79) Use abs value of nTimeOffset to check whether to call AddTimeData
* [`22be7a16`](https://github.com/wagerr/wagerr/commit/22be7a16) Define CheckOffsetDisconnectedPeers
* [`1a693ca0`](https://github.com/wagerr/wagerr/commit/1a693ca0) Clean time offset warning when it gets back within range
* [`ee59842e`](https://github.com/wagerr/wagerr/commit/ee59842e) Fix CheckBlockHeader version after block 300 for regtest
* [`c082ee6f`](https://github.com/wagerr/wagerr/commit/c082ee6f) Detailed debug log for received spork messages
* [`172c59bc`](https://github.com/wagerr/wagerr/commit/172c59bc) Add mint txid to spendrawzerocoin (and look for it if empty)
* [`70141b1b`](https://github.com/wagerr/wagerr/commit/70141b1b) Refactor spendzerocoin/spendzcoinmints, fix createrawzerocoinstake
* [`a3be6713`](https://github.com/wagerr/wagerr/commit/a3be6713) Lower zerocoin confirmations on regtest
* [`f2aec4af`](https://github.com/wagerr/wagerr/commit/f2aec4af) use json object as output of mintzerocoin
* [`15e3efd8`](https://github.com/wagerr/wagerr/commit/15e3efd8) createrawzerocoinpublicspend --> createrawzerocoinspend
* [`5014d8b4`](https://github.com/wagerr/wagerr/commit/5014d8b4) Fix fake-stake spent serials detection on forked chains
* [`a7647e6b`](https://github.com/wagerr/wagerr/commit/a7647e6b) Refactor POS reorg test
* [`166419b9`](https://github.com/wagerr/wagerr/commit/166419b9) Enable v2 spending on regtest with spendrawzerocoin
* [`22e4d56b`](https://github.com/wagerr/wagerr/commit/22e4d56b) Remove extra lock in spendrawzerocoin
* [`e145bb5c`](https://github.com/wagerr/wagerr/commit/e145bb5c) Remove spammy log in in StakeV1
* [`3609d9d8`](https://github.com/wagerr/wagerr/commit/3609d9d8) Init: remove "precompute" debug category (not used anywhere)
* [`a5a4fdea`](https://github.com/wagerr/wagerr/commit/a5a4fdea) Init: remove precompute-related helps in strUsage
* [`6df0f7fb`](https://github.com/wagerr/wagerr/commit/6df0f7fb) remove COLUMN_PRECOMPUTE from zwgrcontroldialog
* [`50adf967`](https://github.com/wagerr/wagerr/commit/50adf967) Remove zWGR precomputing global variables.
* [`2d4d82bd`](https://github.com/wagerr/wagerr/commit/2d4d82bd) Remove DB functions for zerocoin precomputing
* [`6eefbe2b`](https://github.com/wagerr/wagerr/commit/6eefbe2b) Remove zwgr spend cache from zwgr tracker
* [`d5a37253`](https://github.com/wagerr/wagerr/commit/d5a37253) Remove crazy useful unittest created by "Tom"
* [`b09fb54e`](https://github.com/wagerr/wagerr/commit/b09fb54e) remove fPrecompute option in SelectStakeCoins
* [`70709abc`](https://github.com/wagerr/wagerr/commit/70709abc) Remove precompute option in default framework node conf
* [`852937bc`](https://github.com/wagerr/wagerr/commit/852937bc) fix signature check (against old format) in mnbudgetrawvote
* [`9af3c76f`](https://github.com/wagerr/wagerr/commit/9af3c76f) Don't log missing MNs during CBudgetProposal::CleanAndRemove
* [`1b0bc572`](https://github.com/wagerr/wagerr/commit/1b0bc572) Rework staking status
* [`725ee413`](https://github.com/wagerr/wagerr/commit/725ee413) Don't do extra PoW round for pos blocks in 'generate' RPC
* [`29f0ed87`](https://github.com/wagerr/wagerr/commit/29f0ed87) Don't count zWGR in MintableCoins()
* [`5d88fc82`](https://github.com/wagerr/wagerr/commit/5d88fc82) Remove GetStakingBalance calls to get staking status
* [`f1c6da83`](https://github.com/wagerr/wagerr/commit/f1c6da83) Check min depth requirement for stake inputs in AvailableCoins
* [`9d530047`](https://github.com/wagerr/wagerr/commit/9d530047) Faster lookup for MintableCoins
* [`70e1b185`](https://github.com/wagerr/wagerr/commit/70e1b185) zWGR Don't validate Accumulator Checkpoints anymore
* [`302157b9`](https://github.com/wagerr/wagerr/commit/302157b9) Remove extra checks before GetBlocksToMaturity
* [`b9a77e3f`](https://github.com/wagerr/wagerr/commit/b9a77e3f) Add function CMerkleTx::IsInMainChainImmature
* [`99d65310`](https://github.com/wagerr/wagerr/commit/99d65310) Set '-staking' disabled by default on RegTest
* [`ce1502f8`](https://github.com/wagerr/wagerr/commit/ce1502f8) Align GetBlockValue calls to CreateCoinStake: current height + 1

wagerrdeveloper (87):
* [`0eb153e9`](https://github.com/wagerr/wagerr/commit/0eb153e9) added flushable storage model
* [`78f68d43`](https://github.com/wagerr/wagerr/commit/78f68d43) Betting DB with flushable storage was implemented. Also added undo.
* [`8a2f245d`](https://github.com/wagerr/wagerr/commit/8a2f245d) added unit-tests for Betting DB
* [`2b992ce2`](https://github.com/wagerr/wagerr/commit/2b992ce2) added VS Code debug configs
* [`90cca67a`](https://github.com/wagerr/wagerr/commit/90cca67a) added betting funcional tests
* [`bda08308`](https://github.com/wagerr/wagerr/commit/bda08308) added comments
* [`58048501`](https://github.com/wagerr/wagerr/commit/58048501) fix Travis building
* [`d7323cef`](https://github.com/wagerr/wagerr/commit/d7323cef) fix Travis errors [wip]
* [`8ea542b7`](https://github.com/wagerr/wagerr/commit/8ea542b7) fix old gcc compiler bug (https://gcc.gnu.org/bugzilla/show_bug.cgi?id=50025)
* [`cdddde44`](https://github.com/wagerr/wagerr/commit/cdddde44) Fix of handling of spreads and totals was reverted back
* [`ab28fcae`](https://github.com/wagerr/wagerr/commit/ab28fcae) fix flushable db key compatarion also added debug log msgs for betting
* [`f402c434`](https://github.com/wagerr/wagerr/commit/f402c434) Created ParlayBetTxType. Created class for serialization Single Bet or Parlay Bet on DB. Created ParlayToOpCode/ParlayFromOpCode functions.
* [`1127a5cc`](https://github.com/wagerr/wagerr/commit/1127a5cc) Handle ParlayTx
* [`2550ebaf`](https://github.com/wagerr/wagerr/commit/2550ebaf) add new payout system add bets undo
* [`aad9a92d`](https://github.com/wagerr/wagerr/commit/aad9a92d) temporary fix PoS for regtest
* [`76356957`](https://github.com/wagerr/wagerr/commit/76356957) fix key byte order fix undo for peerless bets add undo for parlay bets
* [`9bcf9d76`](https://github.com/wagerr/wagerr/commit/9bcf9d76) add rpc for place parlay bets add rpc for list bets from db fix some rpc commands
* [`9858b879`](https://github.com/wagerr/wagerr/commit/9858b879) add functional tests for betting when PoS won't work in regtest it should not work
* [`b1878fde`](https://github.com/wagerr/wagerr/commit/b1878fde) fix rpc unit test fix some compile errors
* [`7f09dd3a`](https://github.com/wagerr/wagerr/commit/7f09dd3a) fixed regtest betting fixed functional test fixed some mistakes
* [`9095adda`](https://github.com/wagerr/wagerr/commit/9095adda) getting locked events for bets from previous block fixed test
* [`50bd9ce4`](https://github.com/wagerr/wagerr/commit/50bd9ce4) added betting tx checker when accepting to mempool also added feature test
* [`4c3e0bdc`](https://github.com/wagerr/wagerr/commit/4c3e0bdc) fix bet time serialization
* [`fbf282f4`](https://github.com/wagerr/wagerr/commit/fbf282f4) added refund for zerro odds added refund for timecuted bets fix bets undo
* [`ab841a4e`](https://github.com/wagerr/wagerr/commit/ab841a4e) added tests for timecut refund
* [`13e2208d`](https://github.com/wagerr/wagerr/commit/13e2208d) fixed seeking of kv flushable iterator
* [`6a33ff70`](https://github.com/wagerr/wagerr/commit/6a33ff70) implement payouts info entry structures init db storage
* [`79099b36`](https://github.com/wagerr/wagerr/commit/79099b36) add generating of payout info [WIP]
* [`e25bc2e9`](https://github.com/wagerr/wagerr/commit/e25bc2e9) fixup! add logic of negative nSpreadPoints to new GetBetOdds func
* [`2b8e2108`](https://github.com/wagerr/wagerr/commit/2b8e2108) add tests for spread events v2
* [`8901d42d`](https://github.com/wagerr/wagerr/commit/8901d42d) fix PruneOlderUndos
* [`b1592f79`](https://github.com/wagerr/wagerr/commit/b1592f79) added PayoutType added collecting of payouts info for betting and lotto
* [`addfbde5`](https://github.com/wagerr/wagerr/commit/addfbde5) added write payouts info to db added revert payouts info when block disconnecting fix some compile errors
* [`297a7b5e`](https://github.com/wagerr/wagerr/commit/297a7b5e) added RPC methods for getting payouts info
* [`197ecd58`](https://github.com/wagerr/wagerr/commit/197ecd58) added tests for payouts info
* [`4c0e7cab`](https://github.com/wagerr/wagerr/commit/4c0e7cab) fix trevis errors
* [`ba502b53`](https://github.com/wagerr/wagerr/commit/ba502b53) fixed parlay bets undo
* [`f8ef0b69`](https://github.com/wagerr/wagerr/commit/f8ef0b69) fix payout info undo
* [`54e3413a`](https://github.com/wagerr/wagerr/commit/54e3413a) fix some RPC commands
* [`9bff8e7c`](https://github.com/wagerr/wagerr/commit/9bff8e7c) enchancement of RPC listbetsdb func
* [`adf8e614`](https://github.com/wagerr/wagerr/commit/adf8e614) fix changing in DB when iterating
* [`30afc08b`](https://github.com/wagerr/wagerr/commit/30afc08b) Update listevents for v2 Spread Change Formatting
* [`1e8c987d`](https://github.com/wagerr/wagerr/commit/1e8c987d) fixed spreads v2
* [`26b067ac`](https://github.com/wagerr/wagerr/commit/26b067ac) add getmybets getallbets rpc commands add payouts to getallbets/getmybets rpc calls add bets result type and payout fix RPC getpayoutinfosince for getting info from last blocks added game scores to RPC response rename outcome types for better understandin
* [`1353e1a2`](https://github.com/wagerr/wagerr/commit/1353e1a2) added bet amount checking when add to mempool
* [`b3b353c1`](https://github.com/wagerr/wagerr/commit/b3b353c1) added periodic flushing for betting DB
* [`812b938b`](https://github.com/wagerr/wagerr/commit/812b938b) fix betting undo when opcode with nonexistent event is come
* [`e6612ef8`](https://github.com/wagerr/wagerr/commit/e6612ef8) quick games: DB, structurs and opcode implementation
* [`f5c28ed6`](https://github.com/wagerr/wagerr/commit/f5c28ed6) quick games: framework and dice game implementation
* [`ade1f975`](https://github.com/wagerr/wagerr/commit/ade1f975) quick games: RPC methods placeqgdicebet, getallqgbets and getmyqgbets implementation
* [`cac64965`](https://github.com/wagerr/wagerr/commit/cac64965) quick games: python tests implementation
* [`19684662`](https://github.com/wagerr/wagerr/commit/19684662) fix test for spreads v2
* [`9f0c906a`](https://github.com/wagerr/wagerr/commit/9f0c906a) added get odds method for asian handicap added checking prototypes for Oracle TXS added tests for half and quarter spreads v2
* [`e71c323c`](https://github.com/wagerr/wagerr/commit/e71c323c) fix bet's resultType value
* [`e36773ba`](https://github.com/wagerr/wagerr/commit/e36773ba) add new RPC function getbetbytxid
* [`34223872`](https://github.com/wagerr/wagerr/commit/34223872) added payoutTxHash and payoutTxOut to response of RPC functions getallbets, getmybets, getbetbytxid
* [`dbf7e423`](https://github.com/wagerr/wagerr/commit/dbf7e423) Big betting refactoring: - reworked opcode parse system - reworked and separated DB, TX and common structs, enums and algorythms - optimized betting module for external resources - added parlay checking to discard bet with same event in legs
* [`9ec39684`](https://github.com/wagerr/wagerr/commit/9ec39684) refactoring small fixes
* [`5fa96ff6`](https://github.com/wagerr/wagerr/commit/5fa96ff6) Revert "[FIX] Ensure oracle updates are undone before bets"
* [`e29190fb`](https://github.com/wagerr/wagerr/commit/e29190fb) Revert "[FIX] Ensure oracle updates are processed after bets"
* [`03e157bc`](https://github.com/wagerr/wagerr/commit/03e157bc) Revert "Allow inconsistencies in betting DB to trigger reindexing."
* [`18569883`](https://github.com/wagerr/wagerr/commit/18569883) Change parlay odds calculation to effective
* [`a02afdeb`](https://github.com/wagerr/wagerr/commit/a02afdeb) Probably raise exception on block height 1092935 on linux debug build
* [`084a9dd5`](https://github.com/wagerr/wagerr/commit/084a9dd5) Added new bet's statuses: half-lose, half-win, partial-lose, partial-win
* [`929223ae`](https://github.com/wagerr/wagerr/commit/929223ae) Fix incorrect effective odds calculation for onchain odds = 0
* [`dc1dc0a1`](https://github.com/wagerr/wagerr/commit/dc1dc0a1) Added quickgame bets collecting in RPC getbetbytxid
* [`a9a304ca`](https://github.com/wagerr/wagerr/commit/a9a304ca) Added some log traces, fix in bet handler potential winning and payout calculation
* [`1e37e1ad`](https://github.com/wagerr/wagerr/commit/1e37e1ad) Fix payout calculation with incorrect effective odds in block 857206 Also fix erorr message for Inconsistent block database detection
* [`a109d0d6`](https://github.com/wagerr/wagerr/commit/a109d0d6) Fix placeqgdicebet params json parsing
* [`885911c7`](https://github.com/wagerr/wagerr/commit/885911c7) Fix getbetbytxid, payout tx info for qg bets
* [`30f435ca`](https://github.com/wagerr/wagerr/commit/30f435ca) Fix UNDO bugs with duplicated or not affected to chain oracle txs
* [`862a8a5b`](https://github.com/wagerr/wagerr/commit/862a8a5b) Fix omno/dev reward calculation to effective odds format
* [`a40b74ff`](https://github.com/wagerr/wagerr/commit/a40b74ff) Fixed dice bet info parser Fixed placeqgdicebet Fixed quick games rewards Fixed opcode parse processing due test failure Reworked RPC geteventliability
* [`fb599278`](https://github.com/wagerr/wagerr/commit/fb599278) Fixed dice bet info parser Fixed placeqgdicebet Fixed quick games rewards Reworked RPC geteventliability
* [`1d9cb61c`](https://github.com/wagerr/wagerr/commit/1d9cb61c) add reverse iterator to db
* [`223ea18d`](https://github.com/wagerr/wagerr/commit/223ea18d) add includeWatchonly param to getmybets
* [`81c3e67a`](https://github.com/wagerr/wagerr/commit/81c3e67a) add account param to getmybets
* [`2ef4a0cb`](https://github.com/wagerr/wagerr/commit/2ef4a0cb) add db for chain games
* [`55f3c97c`](https://github.com/wagerr/wagerr/commit/55f3c97c) fix reverse iterator
* [`0245d1dc`](https://github.com/wagerr/wagerr/commit/0245d1dc) Added mempool filter for oracle txs
* [`14fbd3a9`](https://github.com/wagerr/wagerr/commit/14fbd3a9) Added falied txs db, for skiping those undo
* [`06e37906`](https://github.com/wagerr/wagerr/commit/06e37906) Revert "Fixed dice bet info parser"
* [`3bd44c50`](https://github.com/wagerr/wagerr/commit/3bd44c50) Fixed failedTxs DB cache creating
* [`274d8128`](https://github.com/wagerr/wagerr/commit/274d8128) Update Splash Screen
* [`e3148333`](https://github.com/wagerr/wagerr/commit/e3148333) Update About Logo
* [`cc0234e1`](https://github.com/wagerr/wagerr/commit/cc0234e1) Update Horizontal Logo
* [`7dd4ccdd`](https://github.com/wagerr/wagerr/commit/7dd4ccdd) Update QT Sidebar Colors

warrows (23):
* [`c82c9af9`](https://github.com/wagerr/wagerr/commit/c82c9af9) Fix compilation
* [`3cd09dba`](https://github.com/wagerr/wagerr/commit/3cd09dba) scripted-diff: use insecure_rand256/randrange more
* [`b5efced1`](https://github.com/wagerr/wagerr/commit/b5efced1) scripted-diff: Use randbits/bool instead of randrange
* [`6245da59`](https://github.com/wagerr/wagerr/commit/6245da59) scripted-diff: Use new naming style for insecure_rand* functions
* [`e971485b`](https://github.com/wagerr/wagerr/commit/e971485b) Fix compilation
* [`47b8b3e2`](https://github.com/wagerr/wagerr/commit/47b8b3e2) Add a missing include
* [`5c729734`](https://github.com/wagerr/wagerr/commit/5c729734) Use arrays instead of unic vars in Chacha20
* [`9cbbe817`](https://github.com/wagerr/wagerr/commit/9cbbe817) Replace a string by the function name in a log
* [`7871db56`](https://github.com/wagerr/wagerr/commit/7871db56) Remove unused OpenSSL includes
* [`1b87b581`](https://github.com/wagerr/wagerr/commit/1b87b581) Remove OpenSSL version check
* [`4e2fa569`](https://github.com/wagerr/wagerr/commit/4e2fa569) Pass caught exceptions by reference
* [`8eba271c`](https://github.com/wagerr/wagerr/commit/8eba271c) Add const qualifier to exception catching
* [`e23fd2f6`](https://github.com/wagerr/wagerr/commit/e23fd2f6) Replace tabs with spaces
* [`38af6c0b`](https://github.com/wagerr/wagerr/commit/38af6c0b) Remove all "using namespace" statements
* [`f911b183`](https://github.com/wagerr/wagerr/commit/f911b183) Do not store Merkle branches in the wallet
* [`01b21fc1`](https://github.com/wagerr/wagerr/commit/01b21fc1) Switch to a constant-space Merkle root/branch algorithm
* [`88f01697`](https://github.com/wagerr/wagerr/commit/88f01697) Move wallet functions out of header
* [`cdafe64d`](https://github.com/wagerr/wagerr/commit/cdafe64d) Do not flush the wallet in AddToWalletIfInvolvingMe(..)
* [`acb8424b`](https://github.com/wagerr/wagerr/commit/acb8424b) Keep track of explicit wallet conflicts instead of using mempool
* [`519c8b4a`](https://github.com/wagerr/wagerr/commit/519c8b4a) Fix an error in tx depth computation
* [`2068d98e`](https://github.com/wagerr/wagerr/commit/2068d98e) Ignore coinbase and zc tx "conflicts"
* [`6f653479`](https://github.com/wagerr/wagerr/commit/6f653479) Remove a call to IsSuperMajority
* [`a4759152`](https://github.com/wagerr/wagerr/commit/a4759152) Remove a duplicate variable definition

## Credits

Thanks to everyone who directly contributed to this release and to everyone whose upstream updates were merged.

- Akshay
- Alex Morcos
- Andrew Chow
- Ben Woosley
- CkTi
- Cory Fields
- Cryptarchist
- Dag Robole
- David Mah
- dexX7
- fanquake
- furszy
- Fuzzbawls
- GitStashCore
- Hennadii Stepanov
- James Hilliard
- Jonas Schnelli
- Kokary
- MarcoFalke
- Matt Corallo
- Mrs-X
- Pieter Wuille
- practicalswift
- presstab
- random-zebra
- wagerrdeveloper
- WagerrTor
- warrows
- Wladimir J. van der Laan

As well as everyone that helped translating on [Transifex](https://www.transifex.com/projects/p/wagerr-translations/),
everyone that submitted issues and reviewed pull requests, the QA team during Testing and the Node hosts supporting our Testnet.
