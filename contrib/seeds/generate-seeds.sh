# Copyright (c) 2018 The Wagerr developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

# replace Wagerr chainparamsseeds.h
# Requirements:
#  - 1. RUN THIS SCRIPT FROM WAGERR (Coins) ROOT FOLDER
#       contrib/seeds/generate-seeds.sh
#  - 2. File contrib/seeds/generate-seeds.py MUST BE PRESENT
#       Check requirements for usage of generate-seeds.py
#       for your operating system.
# Get nodes from chainz
wget -O contrib/seeds/nodes_main.json https://chainz.cryptoid.info/wgr/api.dws?q=nodes && rm contrib/seeds/nodes_main.json

# parse seed ip's into nodes_main.txt and remove node_main.json
awk -F ':[ \t]*' '/^.*"nodes"/ {print $5 $9 $13 $17 $21 $25 $29}' contrib/seeds/nodes_main.json > contrib/seeds/nodes_main.txt
rm contrib/seeds/nodes_main.json;

# cleanup to get ip's only **TODO** [there are better/faster ways to do that]
sed -i 's/,{\"subver\"/\\\n/g' contrib/seeds/nodes_main.txt;
sed -i 's/\[\"//g' contrib/seeds/nodes_main.txt;
sed -i 's/\",\"/\\\n/g' contrib/seeds/nodes_main.txt;
sed -i 's/\"]}]//g' contrib/seeds/nodes_main.txt;
sed -i 's/\"]}//g' contrib/seeds/nodes_main.txt;
sed -i 's/\\//g' contrib/seeds/nodes_main.txt;

# append fixed ip's to nodes_main.txt
#tee -a ./contrib/seeds/nodes_main.txt <<EOF
cat <<'EOF' >> ./contrib/seeds/nodes_main.txt
46.166.148.3:55002
92.222.79.69:55002
94.23.169.175:55002
188.165.142.180:55002
51.15.136.185:55002
159.65.128.250:55002
159.65.7.222:55002
88.202.231.139:55002
198.62.109.223:55002
94.237.27.150:55002
78.132.7.170:55002
35.231.63.53:55002
35.231.13.18:55002
35.227.74.136:55002
35.189.104.248:55002
EOF

# append fixed ip's to nodes_testnet.txt
#tee ./contrib/seeds/nodes_test.txt <<EOF
cat <<'EOF' > ./contrib/seeds/nodes_test.txt
46.166.148.3:55004
92.222.79.69:55004
94.23.169.175:55004
188.165.142.180:55004
51.15.136.185:55004
159.65.128.250:55004
159.65.7.222:55004
88.202.231.139:55004
198.62.109.223:55004
94.237.27.150:55004
78.132.7.170:55004
35.231.63.53:55004
35.231.13.18:55004
35.227.74.136:55004
35.189.104.248:55004
EOF

# remove duplicate entries
echo "removing duplicates"
awk ‘!x[$0]++’ ./contrib/seeds/nodes_main.txt
awk ‘!x[$0]++’ ./contrib/seeds/nodes_test.txt

# run generate-seeds.py and update chainparamsseeds.h
cd contrib/seeds;
python3 generate-seeds.py . > ../../src/chainparamsseeds.h;
rm ./nodes_main.txt;
rm ./nodes_test.txt;
cd ../..;

# Optional: print new harcoded entries
cat < ./src/chainparamsseeds.h;
