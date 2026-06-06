SHELL := /bin/bash

ROOT := ../..
CC ?= cc
C_DEMO_BIN := $(ROOT)/target/debug/zoneout_c_api_demo

.PHONY: all build run clean

all: run

build:
	@cd $(ROOT) && cargo build
	@$(CC) -I$(ROOT)/include demo.c -L$(ROOT)/target/debug -lzoneout \
		-Wl,-rpath,$$(cd $(ROOT) && pwd)/target/debug -o $(C_DEMO_BIN)

run: build
	@$(C_DEMO_BIN)

clean:
	@rm -f $(C_DEMO_BIN)
