CFLAGS = -g -O0 -Wall -Wextra
SAN = -fsanitize=address,undefined

default:
	@echo "Please provide a command:"
	@echo "> test-hash-table"
	@echo "> test-resp"
	@echo "> test"

test: test-hash-table test-resp

test-hash-table:
	gcc $(CFLAGS) -o test_hash_table test/test_hash_table.c src/hash_table.c
	./test_hash_table
	gcc $(CFLAGS) $(SAN) -o test_hash_table_asan test/test_hash_table.c src/hash_table.c
	./test_hash_table_asan
	rm ./test_hash_table ./test_hash_table_asan

test-resp:
	gcc $(CFLAGS) -o test_resp test/test_resp.c src/resp.c
	./test_resp
	gcc $(CFLAGS) $(SAN) -o test_resp_asan test/test_resp.c src/resp.c
	./test_resp_asan
	rm ./test_resp ./test_resp_asan

.PHONY: default test test-hash-table test-resp
