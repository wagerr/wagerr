Wagerr Core version *1.6.0* is now available from:

  <https://github.com/wagerr/wagerr/releases/tag/v1.6.0>

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

zWGR Staking
------------

zWGR staking is here! With the release of zWGR staking, there are now
effectively two versions of zWGR, zWGR version 1 minted on a Wagerr Core version
before 1.6.0, and zWGR version 2 minted on Wagerr Core version 1.6.0 or higher.
To use the new zWGR features in this release (including staking) will require
the use of zWGR version 2. If you currently hold zWGR version 1 and wish to take
advantage of zWGR staking and deterministic zWGR, you will need to spend the
zWGR version 1 to yourself and remint zWGR version 2.

Note: To find your zWGR version, click the privacy tab, then the zWGR Control
button then expand the arrows next to the desired denomination.

Deterministic zWGR Seed Keys
----------------------------

zWGR is now associated with a deterministic seed key. With this seed key, users
are able to securely backup their zWGR outside of the wallet that the zWGR had
been minted on. zWGR can also be transferred from wallet to wallet without the
need of transferring the wallet data file.

Updated zWGR minting
--------------------

zWGR minting now only requires two mints (down from three) to mature. zWGR mints
still require 20 confirmations. Mints also require that the 'second' mint is at
least two checkpoints deep in the chain (this was already the case, but the
logic was not as precise).

zWGR Search
---------------

Users will now have the ability to search the blockchain for a specific serial #
to see if a zWGR denomination has been spent or not.

WGR, zWGR and Masternode Payment Schedule
-----------------------------------------

To encourage the use of zWGR and increase the Wagerr zerocoin anonymity set, the
Wagerr payment schedule has been changed to the following:

If a user staking zWGR wins the reward for their block, the following zWGR
reward will be:

- 3 zWGR (3 x 1 denominations) rewarded to the staker, 2 WGR rewarded to the
masternode owner and 1 WGR available for the budget. This is a total block
reward of 6 WGR, up from 5.

If a user staking WGR wins the reward, the following amounts will be rewarded:

- 2 WGR to the WGR staker, 3 WGR to the Masternode owner and 1 WGR available for
the budget. This is a total block reward of 6 WGR, up from 5.

Return change to sender when minting zWGR
-----------------------------------------

Previously, zWGR minting would send any change to a newly generated "change
address". This has caused confusion among some users, and in some cases
insufficient backups of the wallet. The wallet will now find the contributing
address which contained the most WGR and return the change from a zWGR mint to
that address.

Backup Wallet
-------------

The Wagerr Core wallet can now have user selected directories for automatic
backups of the wallet data file (wallet.dat). This can be set by adding the
following lines to the wagerr.conf file, found in the Wagerr data directory:

- backuppath = <directory / full path>
- zwgrbackuppath = <directory / full path>
- custombackupthreshold = <backup_limit>

Notes:

- Configured variables display in the *Wallet Repair* tab inside the
*Tools Window / Dropdown Menu*
- Allows for backing up wallet.dat to the user set path, simultaneous to other
backups
- Allows backing up to directories and files, with a limit (*threshold*) on how
many files can be saved in the directory before it begins overwriting the oldest
wallet file copy.
- If path is set to directory, the backup will be named `wallet.dat-<year>-<month>-<day>-<hour>-<minute>-<second>`
- If zWGR backup, auto generated name is `wallet-autozwgrbackup.dat-<year>-<month>-<day>-<hour>-<minute>-<second>`
- If path set to file, backup will be named `<filename>.dat`
- `custombackupthreshold` enables the user to select the maximum count of backup files to be written before overwriting existing backups.


### Example:

* -backuppath=~/WagerrBackup/
* -custombackupthreshold=2

Backing up 4 times will result as shown below:

* Backup #1 - 2018-04-20-00-04-00
* Backup #2 - 2018-04-21-04-20-00
* Backup #3 - 2018-04-22-00-20-04
* Backup #4 - 2018-04-23-20-04-00

```
1. ~/WagerrBackup/
                 /wallet.dat-2018-04-20-00-04-00

2. ~/WagerrBackup/
                 /wallet.dat-2018-04-20-00-04-00
                 /wallet.dat-2018-04-21-04-20-00

3. ~/WagerrBackup/
                 /wallet.dat-2018-04-22-00-20-04
                 /wallet.dat-2018-04-21-04-20-00

4. ~/WagerrBackup/
                 /wallet.dat-2018-04-22-00-20-04
                 /wallet.dat-2018-04-23-20-04-00
```

Masternode Broadcast RPC commands
---------------------------------

- `createmasternodebroadcast`
- `decodemasternodebroadcast`
- `relaymasternodebroadcast`

A new set of RPC commands `masternodebroadcast` to create Masternode broadcast
messages offline and relay them from an online node later (messages expire in
~1 hour).

Migration to libevent based http server
---------------------------------------

The RPC and REST interfaces are now initialized and controlled using standard
libevent instead of the ad-hoc pseudo httpd interface that was used previously.
This change introduces a more resource friendly and effective interface.

New Notification Path
---------------------

`blocksizenotify`

A new notification path has been added to allow a script to be executed when
receiving blocks larger than the 1MB legacy size. This functions similar to the
other notification listeners (`blocknotify`, `walletnotify`, etc).


Further Reading: Version 2 Zerocoins
====================================

Several critical security flaws in the zerocoin protocol and Wagerr's zerocoin
implementation have been patched. Enough has changed that new zerocoins are
distinct from old zerocoins, and have been labelled as *version 2*. When using
the zWGR Control dialog in the Qt wallet, a user is able to see zWGR marked as
version 1 or 2.

zPoS (zWGR staking)
-------------------

Once a zWGR has over 200 confirmations it becomes available to stake. Staking
zWGR will consume the exact zerocoin that is staked and replace it with a
freshly minted zerocoin of the same denomination as well as a reward of three 1
denomination zWGR. So for example if a 1,000 zWGR denomination is staked, the
protocol replaces that with a fresh 1,000 denomination and 3 x 1 denomination
zWGRs.

Secure Spending
---------------

Version 1 zerocoins, as implemented by [Miers et. al](http://zerocoin.org/media/pdf/ZerocoinOakland.pdf),
allow for something we describe as *serial trolling*. Spending zerocoins
requires that the spender reveal their serial number associated with the
zerocoin, and in turn that serial number is used to check for double spending.
There is a fringe situation (which is very unlikely to happen within Wagerr's
zerocoin implementation due to delayed coin accumulation) where the spender
sends the spending transaction, but the transaction does not immediately make it
into the blockchain and remains in the mempool for a long enough duration that a
*troll* has enough time to see the spender's serial number, mint a new zerocoin
with the same serial number, and spend the new zerocoin before the original
spender's transaction becomes confirmed. If the timing of this fringe situation
worked, then the original spender's coin would be seen as invalid because the
troll was able to have the serial recorded into the blockchain first, thus
making the original spender's serial appear as a double spend.

The serial troll situation is mitigated in version 2 by requiring that the
serial number be a hash of a public key. The spend requires an additional
signature signed by the private key associated with the public key hash matching
the serial number. This work around was conceived by Tim Ruffing, a
cryptographer that has studied the zerocoin protocol and done consulting work
for the ZCoin project.

Deterministic Zerocoin Generation
---------------------------------

Zerocoins, or zWGR, are now deterministically generated using a unique 256 bit
seed. Each wallet will generate a new seed on its first run. The deterministic
seed is used to generate a string of zWGR that can be recalculated at any time
using the seed. Deterministic zWGR allows for users to backup all of their
future zWGR by simply recording their seed and keeping it in a safe place
(similar to backing up a private key for WGR). The zWGR seed needs to remain in
the wallet in order to spend the zWGR after it is generated, if the seed is
changed then the coins will not be spendable because the wallet will not have
the ability to regenerate all of the private zWGR data from the seed. It is
important that users record & backup their seed after their first run of the
wallet. If the wallet is locked during the first run, then the seed will be
generated the first time the wallet is unlocked.

Zerocoin Modulus
----------------

Wagerr's zerocoin implementation used the same code from the ZCoin project to
import the modulus use for the zerocoin protocol. The chosen modulus is the 2048
bit RSA number created for the RSA factoring challenge. The ZCoin project's
implementation (which Wagerr used) improperly imported the modulus into the
code. This flaw was discovered by user GOAT from the [Civitas Project](https://github.com/eastcoastcrypto/Civitas/),
and reported to PIVX and the patch ported to Wagerr. The modulus is now
correctly imported and Wagerr's accumulators have been changed to use the new
proper modulus.

1.6.0 Change log
================



Credits
=======

Thanks to everyone who directly contributed to this release:

- 3reioDev
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
