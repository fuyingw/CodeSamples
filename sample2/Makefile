CC=g++
CFLAGS=-std=c++17
DEPS = TradeStat.hpp

tstat: main.cpp $(DEPS)
	$(CC) -o $@ main.cpp $(CFLAGS)

.PHONY: clean test

clean:
	rm -f tstat *.o test/test_TradeStat

test:
	$(CC) -o test/test_TradeStat test/test_TradeStat.cpp -I./  $(CFLAGS) &&  test/test_TradeStat

gtest:
	$(CC) -o test/gtest_TradeStat test/gtest_TradeStat.cpp -I./  $(CFLAGS) -l gtest &&  test/gtest_TradeStat
