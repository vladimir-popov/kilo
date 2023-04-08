.DEFAULT_GOAL = build

CC := gcc
SRC := src
TEST := test
BUILD := build
NAME := kilo

compile:
	$(CC) -o $(BUILD)/$(NAME) $(SRC)/$(NAME).c

run: compile
	$(BUILD)/$(NAME)

test-compile:
	@$(CC) -I$(SRC) -o $(BUILD)/utest $(TEST)/utest.c

test: test-compile
	@$(BUILD)/utest

build: compile test
