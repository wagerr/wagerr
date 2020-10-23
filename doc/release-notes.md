
(note: this is a temporary file, to be added-to by anybody, and moved to release-notes at release time)

# WAGERR Core version 4.0.0

Release is now available from:

  <https://github.com/wagerr/wagerr/releases>

This is a new major version release, bringing major performance and stability improvements, new
features, various bugfixes and other improvements.

Please report bugs using the issue tracker at github:

  <https://github.com/wagerr/wagerr/issues>


## Upgrading and downgrading

### Recommended upgrade

### How to Upgrade

If you are running an older version, shut it down. Wait until it has completely
shut down (which might take a few minutes for older versions), then run the
installer (on Windows) or just copy over /Applications/Wagerr-Qt (on Mac) or
wagerr/wagerr-qt (on Linux). If you were using version < 4.0.0 you will have to reindex
(start with -reindex) or resync (start with -resync) to make sure your wallet has all
the new data synced.

## Downgrade warning

### Downgrade to a version < 3.0.0

Downgrading to a version smaller than 3.0 is not supported anymore as the Wagerr Betting
Protocol V3 activated on mainnet and testnet.

### Downgrade to versions 3.0.0 - 3.1.0

Downgrading to 3.0 and 3.1 releases is fully supported but is highly discouraged unless
you have some serious issues with version 4.0.

### Compatibility

WAGERR Core is extensively tested on multiple operating systems using the Linux kernel, macOS 10.10+, and Windows 7 and later.

Microsoft ended support for Windows XP on [April 8th, 2014](https://www.microsoft.com/en-us/WindowsForBusiness/end-of-xp-support), No attempt is made to prevent installing or running the software on Windows XP, you can still do so at your own risk but be aware that there are known instabilities and issues. Please do not report issues about Windows XP to the issue tracker.

Apple released it's last Mountain Lion update August 13, 2015, and officially ended support on [December 14, 2015](http://news.fnal.gov/2015/10/mac-os-x-mountain-lion-10-8-end-of-life-december-14/). WAGERR Core software starting with v3.2.0 will no longer run on MacOS versions prior to Yosemite (10.10). Please do not report issues about MacOS versions prior to Yosemite to the issue tracker.

WAGERR Core should also work on most other Unix-like systems but is not frequently tested on them.

## Notable changes

4.0 introduces a fully rewritten database layer for the processing of betting activities. As a result:
- All betting activities are handled by the core client's (cached) internal database.
- Transaction and block processing is significantly faster. Block synchronization now averages at 7 hours for a full sync.
- Improved stability due to better handlign of chain reorganizations, fixing instability issues in the 3.x clients.
- All betting data from the Wagerr blockchain are made available through the core client now, including placed bets,
  bet result statuses, odds, spread amounts, total amounts, and score outcome of matches.
- New commands include: getmybets, getbetbytxid "txid", getpayoutinfo "txid".

### Core Features

### Build System

### P2P Protocol and Network Code

### GUI

### Wallet

### Miscellaneous

### RPC/REST
-----------
There are a few changes in existing RPC interfaces in this release:
- `...` ...

There are also new RPC commands:
- `...` ...

Few RPC commands are no longer supported: `...`

See `help command` in rpc for more info.

### Command-line options

Changes in existing cmd-line options:
- `-...` ...

New cmd-line options:
- `-...`

Few cmd-line options are no longer supported: `-...`, `-...`

See `Help -> Command-line options` in Qt wallet or `wagerrd --help` for more info.

## Miscellaneous

A lot of refactoring, backports, code cleanups and other small fixes were done in this release.

## 4.0.0 Change log

See detailed [set of changes](https://github.com/wagerr/wagerr/compare/v4.0.0...wagerr:v3.1.0).

CkTi (1):
* [`fb327b9b`](https://github.com/wagerr/wagerr/commit/fb327b9b) Change chainparams to 152000

Cryptarchist (4):
* [`839d6376`](https://github.com/wagerr/wagerr/commit/839d6376) [Build] Bump version to 4.0.0
* [`bba10597`](https://github.com/wagerr/wagerr/commit/bba10597) [Contrib] Update gitian descriptors for 4.0
* [`5ea5d413`](https://github.com/wagerr/wagerr/commit/5ea5d413) Update client name to Reno for v4 release
* [`814f6777`](https://github.com/wagerr/wagerr/commit/814f6777) [Doc] Update manpages for 4.0.0

David Mah (3):
* [`d5a6ab57`](https://github.com/wagerr/wagerr/commit/d5a6ab57) Update build-unix.md
* [`318fa717`](https://github.com/wagerr/wagerr/commit/318fa717) update parlay start height and new testnet checkpoint
* [`9a31ffc7`](https://github.com/wagerr/wagerr/commit/9a31ffc7) Updated Testnet Checkpoints and Protocol starting height

Kokary (25):
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
* [`d473bae6`](https://github.com/wagerr/wagerr/commit/d473bae6) [FIX][RPC] Update logic to filter watchonly addresses in getmybets

ckti (4):
* [`0e7dc0ee`](https://github.com/wagerr/wagerr/commit/0e7dc0ee) Update snapcraft.yaml
* [`ea02a36d`](https://github.com/wagerr/wagerr/commit/ea02a36d) Update to build s390x and powerpc64le in gitian build This will be necessary to update snapcraft.yaml to use the release binaries to build snap packages on snapcraft store
* [`2017043a`](https://github.com/wagerr/wagerr/commit/2017043a) Change to devel and 3.1.99
* [`ead7d1ae`](https://github.com/wagerr/wagerr/commit/ead7d1ae) Build snap from master

wagerrdeveloper (83):
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

## Credits

Thanks to everyone who directly contributed to this release:

- CkTi
- Cryptarchist
- David Mah
- Kokary
- wagerrdeveloper
- WagerrTor

As well as everyone that helped translating on [Transifex](https://www.transifex.com/projects/p/wagerr-translations/),
everyone that submitted issues and reviewed pull requests, the QA team during Testing and the Node hosts supporting our Testnet.
