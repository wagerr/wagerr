Multi masternode config
=======================

The multi masternode config allows you to control multiple masternodes from a single wallet. The wallet needs to have a valid collateral output of 10000 coins for each masternode. To use this, place a file named masternode.conf in the data directory of your install:
 * Windows: %APPDATA%\wagerr\
 * Mac OS: ~/Library/Application Support/wagerr/
 * Unix/Linux: ~/.wagerr/

The new masternode.conf format consists of a space seperated text file. Each line consisting of an alias, IP address followed by port, masternode private key, collateral output transaction id, collateral output index, donation address and donation percentage (the latter two are optional and should be in format "address:percentage").

Example:
```
mn1 127.0.0.2:55004 7gb6HNz8gRwVwKZLMGQ6XEaLjzPoxUNK4ui3Pig6mXA6RZ8xhsn 49012766543cac37369cf3813d6216bdddc1b9a8ed03ac690221be10aa5edd6c 0
mn2 127.0.0.3:55004 7gHrUV5JFdKF8cYLxAhfzDsj5RkRzebkHuHTG7pErCgaYGxT2vn 49012766543cac37369cf3813d6216bdddc1b9a8ed03ac690221be10aa5edd6c 1 TKa5kuygkJcMDmwsEP1aRawyXPUjbNKxqJ:33
mn3 127.0.0.4:55004 7hTm4uYFYicJb5WhyV7GDkHXJvmn3JT1EvzYTkf9Tnd7pvXa2NQ fdcf8c644452e0c1faff174e5a08948cb550c42a839d10c7ca2e0992bf77a65a 1 TKa5kuygkJcMDmwsEP1aRawyXPUjbNKxqJ
```

In the example above:
* the collateral for mn1 consists of transaction 49012766543cac37369cf3813d6216bdddc1b9a8ed03ac690221be10aa5edd6c, output index 0 has amount 25000
* masternode 2 will donate 33% of its income
* masternode 3 will donate 100% of its income


The following new RPC commands are supported:
* list-conf: shows the parsed masternode.conf
* start-alias \<alias\>
* stop-alias \<alias\>
* start-many
* stop-many
* outputs: list available collateral output transaction ids and corresponding collateral output indexes

When using the multi masternode setup, it is advised to run the wallet with 'masternode=0' as it is not needed anymore.
