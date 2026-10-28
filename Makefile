CDEVFLAGS = -g -O0 -Wall -Wextra
CFLAGS = -g -O2 -Wall -Wextra
SAN = -fsanitize=address,undefined
LDLIBS = -luuid

default:
	@echo "Please provide a command:"
	@echo "> test-hash-table"
	@echo "> test-resp"
	@echo "> test"
	@echo "> test-listener"

build:
	gcc $(CFLAGS) -o "main" src/main.c src/buffer.c src/logger.c src/listener.c src/resp.c src/hash_table.c src/engine.c src/utils.c $(LDLIBS)
	./main

dev:
	gcc $(CDEVFLAGS) $(SAN) $(LDLIBS) -o "main.dev" src/main.c src/buffer.c src/logger.c src/listener.c src/resp.c src/hash_table.c src/engine.c src/utils.c
	./main.dev

test: test-hash-table test-resp

test-hash-table:
	gcc $(CDEVFLAGS) -o test_hash_table test/test_hash_table.c src/hash_table.c
	./test_hash_table
	gcc $(CDEVFLAGS) $(SAN) -o test_hash_table_asan test/test_hash_table.c src/hash_table.c
	./test_hash_table_asan
	rm ./test_hash_table ./test_hash_table_asan

test-resp:
	gcc $(CDEVFLAGS) -o test_resp test/test_resp.c src/resp.c
	./test_resp
	gcc $(CDEVFLAGS) $(SAN) -o test_resp_asan test/test_resp.c src/resp.c
	./test_resp_asan
	rm ./test_resp ./test_resp_asan

test-listener:
	gcc $(CDEVFLAGS) -o test_listener test/test_manual_listener.c src/listener.c src/logger.c src/buffer.c src/resp.c $(LDLIBS)


.PHONY: default test test-hash-table test-resp test-listener
