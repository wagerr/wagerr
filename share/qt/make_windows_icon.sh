#!/bin/bash
# create multiresolution windows icon
#mainnet
ICON_SRC=../../src/qt/res/icons/wagerr.png
ICON_DST=../../src/qt/res/icons/wagerr.ico
convert ${ICON_SRC} -resize 16x16 wagerr-16.png
convert ${ICON_SRC} -resize 32x32 wagerr-32.png
convert ${ICON_SRC} -resize 48x48 wagerr-48.png
convert ${ICON_SRC} -resize 256x256 wagerr-256.png
convert wagerr-16.png wagerr-32.png wagerr-48.png wagerr-256.png ${ICON_DST}
#testnet
ICON_SRC=../../src/qt/res/icons/wagerr_testnet.png
ICON_DST=../../src/qt/res/icons/wagerr_testnet.ico
convert ${ICON_SRC} -resize 16x16 wagerr-16.png
convert ${ICON_SRC} -resize 32x32 wagerr-32.png
convert ${ICON_SRC} -resize 48x48 wagerr-48.png
convert ${ICON_SRC} -resize 256x256 wagerr-256.png
convert wagerr-16.png wagerr-32.png wagerr-48.png wagerr-256.png ${ICON_DST}
rm wagerr-16.png wagerr-32.png wagerr-48.png wagerr-256.png
