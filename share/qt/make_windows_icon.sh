#!/bin/bash
# create multiresolution windows icon
#mainnet
ICON_SRC=../../src/qt/res/icons/wagerr.png
ICON_DST=../../src/qt/res/icons/wagerr.ico
convert ${ICON_SRC} -resize 16x16 bitcoin-16.png
convert ${ICON_SRC} -resize 32x32 bitcoin-32.png
convert ${ICON_SRC} -resize 48x48 bitcoin-48.png
convert bitcoin-16.png bitcoin-32.png bitcoin-48.png ${ICON_DST}
#testnet
ICON_SRC=../../src/qt/res/icons/wagerr_testnet.png
ICON_DST=../../src/qt/res/icons/wagerr_testnet.ico
convert ${ICON_SRC} -resize 16x16 bitcoin-16.png
convert ${ICON_SRC} -resize 32x32 bitcoin-32.png
convert ${ICON_SRC} -resize 48x48 bitcoin-48.png
convert bitcoin-16.png bitcoin-32.png bitcoin-48.png ${ICON_DST}
rm bitcoin-16.png bitcoin-32.png bitcoin-48.png
#regtest
ICON_SRC=../../src/qt/res/icons/wagerr_regtest.png
ICON_DST=../../src/qt/res/icons/wagerr_regtest.ico
convert ${ICON_SRC} -resize 16x16 bitcoin-16.png
convert ${ICON_SRC} -resize 32x32 bitcoin-32.png
convert ${ICON_SRC} -resize 48x48 bitcoin-48.png
convert bitcoin-16.png bitcoin-32.png bitcoin-48.png ${ICON_DST}
rm bitcoin-16.png bitcoin-32.png bitcoin-48.png
