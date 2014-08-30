#!/bin/bash
echo "cp webee210config .confg"
cp -f ./webee210config ./.config
echo "update svn src"
./scripts/feeds update -a
./scripts/feeds install -a
echo "make V=99"
make V=99


