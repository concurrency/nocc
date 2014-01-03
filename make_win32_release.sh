#! /bin/bash

# script to gather the various files needed for stand-alone (ish) Windows execution.

NOCCEXE="nocc.exe"

if [ $# -lt 1 ]; then
	printf 'Usage: %s <path>\n' "$0" 1>&2
	exit 1
fi

# make sure there is no trailing slash
TPATH=$(printf '%s' "$1" | sed -e 's/\/$//g')

if [ ! -x "$NOCCEXE" ]; then
	printf '%s: expected to find nocc.exe here, but did not\n' "$0" 1>&2
	exit 1
fi

if [ ! -d "$TPATH" ]; then
	printf '%s: %s is not a directory.\n' "$0" "$TPATH" 1>&2
	exit 1
fi

for lib in $(ldd "$NOCCEXE" | grep '/usr/bin' | sed -e 's/.*=> \([^ ]*\).*$/\1/'); do
	cp -v "$lib" "$TPATH/"
done

cp -v "$NOCCEXE" "$TPATH/"

if [ -r "$TPATH/nocc.specs.xml" ]; then
	printf '%s: (notice) not overwriting %s/nocc.specs.xml\n' "$0" "$TPATH"
else
	cp -v nocc.specs.xml "$TPATH/"
fi

# and do the addons
mkdir -p "$TPATH/addons" || exit 1
mkdir -p "$TPATH/addons/headers" || exit 1

cp -v addons/*.ldef "$TPATH/addons/"
cp -v addons/headers/* "$TPATH/addons/headers/"

