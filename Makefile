# ScanAll — convenience wrapper around ufbt.
#
# Requires ufbt:  pip install --upgrade ufbt
#
#   make           # build the .fap
#   make launch    # build, upload and run on a connected Flipper
#   make clean     # remove build output
#   make update    # update the ufbt SDK

.PHONY: all build launch clean update

all: build

build:
	ufbt

launch:
	ufbt launch

clean:
	ufbt -c || true
	rm -rf dist build .ufbt

update:
	ufbt update
