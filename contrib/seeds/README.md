# Seeds (python)

**Python script is currently depreceated due to unavailability of a service, plaease use shell script for using chainz seeds
Utility to generate the seeds.txt list that is compiled into the client
(see [src/chainparamsseeds.h](/src/chainparamsseeds.h) and other utilities in [contrib/seeds](/contrib/seeds)).

Be sure to update `PATTERN_AGENT` in `makeseeds.py` to include the current version,
and remove old versions as necessary.

The seeds compiled into the release are created from fuzzbawls' DNS seed data, like this:

    curl -s http://seeder.wagerr.com/wagerr-mainnet.txt > seeds_main.txt
    python3 makeseeds.py < seeds_main.txt > nodes_main.txt
    python3 generate-seeds.py . > ../../src/chainparamsseeds.h

## Dependencies

Ubuntu:

    sudo apt-get install python3-dnspython

Replaces Wagerrs src/chainparamsseeds.h

Requirements:


# Seeds from chainz
1. RUN THIS SCRIPT FROM WAGERR (Coins) ROOT FOLDER ./contrib/seeds/generate-seeds.sh)

    contrib/seeds/generate-seeds.sh

2. File contrib/seeds/generate-seeds.py MUST BE PRESENT

    Check requirements for usage of generate-seeds.py for your operating system.

