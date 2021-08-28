#!/bin/sh

set -x
lua test/trig.lua &
sleep 1
s6-svscan test/svc &
sleep 20
s6-svscanctl -t test/svc
