#!/bin/bash

TOPDIR=${TOPDIR:-$(git rev-parse --show-toplevel)}
SRCDIR=${SRCDIR:-$TOPDIR/src}
MANDIR=${MANDIR:-$TOPDIR/doc/man}

WAGERRD=${WAGERRD:-$SRCDIR/wagerrd}
WAGERRCLI=${WAGERRCLI:-$SRCDIR/wagerr-cli}
WAGERRTX=${WAGERRTX:-$SRCDIR/wagerr-tx}
WAGERRQT=${WAGERRQT:-$SRCDIR/qt/wagerr-qt}

[ ! -x $WAGERRD ] && echo "$WAGERRD not found or not executable." && exit 1

# The autodetected version git tag can screw up manpage output a little bit
BTCVER=($($WAGERRCLI --version | head -n1 | awk -F'[ -]' '{ print $6, $7 }'))

# Create a footer file with copyright content.
# This gets autodetected fine for wagerrd if --version-string is not set,
# but has different outcomes for wagerr-qt and wagerr-cli.
echo "[COPYRIGHT]" > footer.h2m
$WAGERRD --version | sed -n '1!p' >> footer.h2m

for cmd in $WAGERRD $WAGERRCLI $WAGERRTX $WAGERRQT; do
  cmdname="${cmd##*/}"
  help2man -N --version-string=${BTCVER[0]} --include=footer.h2m -o ${MANDIR}/${cmdname}.1 ${cmd}
  sed -i "s/\\\-${BTCVER[1]}//g" ${MANDIR}/${cmdname}.1
done

rm -f footer.h2m
