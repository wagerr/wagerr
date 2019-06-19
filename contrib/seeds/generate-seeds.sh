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
wget -O contrib/seeds/nodes_main.json https://chainz.cryptoid.info/wgr/api.dws?q=nodes #&& rm contrib/seeds/nodes_main.json

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
5.189.150.180:55002
5.189.158.23:55002
45.32.247.220:55002
45.77.188.47:55002
51.15.45.75:55002
51.15.81.234:55002
62.77.152.103:55002
69.16.212.83:55002
89.40.1.122:55002
95.183.50.120:55002
95.183.52.136:55002
118.89.175.236:55002
120.77.202.151:55002
136.144.177.105:55002
139.199.9.219:55002
172.104.53.156:55002
172.104.59.26:55002
185.52.1.189:55002
217.182.38.160:55002
217.182.54.240:55002
EOF

# append fixed ip's to nodes_testnet.txt
#tee ./contrib/seeds/nodes_test.txt <<EOF
cat <<'EOF' > ./contrib/seeds/nodes_test.txt
18.130.249.55:55004
35.176.79.71:55004
35.204.201.58:55004
38.122.91.218:55004
96.30.197.214:55004
144.202.87.185:55004
158.69.120.138:55004
178.62.237.25:55004
212.47.241.113:55004
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