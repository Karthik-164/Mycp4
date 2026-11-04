CC      = gcc
CXX     = g++
CFLAGS  = -std=gnu11 -Wall -Wextra -O2 -g -pthread -Iinclude -MMD -MP
LDFLAGS = -pthread
SRC     = $(wildcard src/*.c)
OBJ     = $(SRC:src/%.c=build/%.o)
TARGET  = mycp

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(OBJ) -o $@ $(LDFLAGS)

build/%.o: src/%.c | build
	$(CC) $(CFLAGS) -c $< -o $@

build:
	mkdir -p build

clean:
	rm -rf build $(TARGET) reports/*.html reports/*.css *.gcov

-include $(OBJ:.o=.d)

# ====================================================================
#  Quality targets:  make test | coverage | cppcheck | misra |
#                    valgrind | sanitize | check
#  Unit tests use GoogleTest.  The C sources are compiled together with
#  tests/fault_inject.h (-include) so that libc calls (malloc, read,
#  write, close ...) can be made to fail on demand.
# ====================================================================
TBUILD    ?= build/test
TEXTRA    ?= --coverage -fprofile-update=atomic
TLDEXTRA  ?= --coverage
TCFLAGS    = -std=gnu11 -O0 -g -pthread -Iinclude -Itests -MMD -MP \
             -include tests/fault_inject.h $(TEXTRA)
TCXXFLAGS  = -std=c++17 -O0 -g -pthread -Wall -Wextra -Iinclude -Itests -MMD -MP
TOBJ_C     = $(SRC:src/%.c=$(TBUILD)/%.o)
TOBJ_CXX   = $(patsubst tests/%.cpp,$(TBUILD)/%.o,$(wildcard tests/*.cpp))
TEST_BIN   = $(TBUILD)/unit_tests

$(TBUILD):
	mkdir -p $(TBUILD)

# main() is renamed so the test binary can call it
$(TBUILD)/main.o: TCFLAGS += -Dmain=mycp_main

$(TBUILD)/%.o: src/%.c | $(TBUILD)
	$(CC) $(TCFLAGS) -c $< -o $@

$(TBUILD)/fault.o: tests/fault.c | $(TBUILD)
	$(CC) -std=gnu11 -O0 -g -pthread -Itests -MMD -MP -c $< -o $@

$(TBUILD)/%.o: tests/%.cpp | $(TBUILD)
	$(CXX) $(TCXXFLAGS) -c $< -o $@

$(TEST_BIN): $(TOBJ_C) $(TBUILD)/fault.o $(TOBJ_CXX)
	$(CXX) $^ -o $@ $(TLDEXTRA) -lgtest_main -lgtest -pthread

-include $(wildcard $(TBUILD)/*.d)

test: $(TEST_BIN)
	find $(TBUILD) -name '*.gcda' -delete
	$(TEST_BIN)

coverage: test
	mkdir -p reports
	gcovr -r . --object-directory $(TBUILD) --filter 'src/' --txt-metric line   --txt reports/coverage_lines.txt
	gcovr -r . --object-directory $(TBUILD) --filter 'src/' --txt-metric branch --txt reports/coverage_branches.txt
	gcovr -r . --object-directory $(TBUILD) --filter 'src/' --html-details reports/coverage.html
	@cat reports/coverage_lines.txt reports/coverage_branches.txt

CPPCHECK ?= $(or $(wildcard /usr/bin/cppcheck),cppcheck)
CPPCHECK_FLAGS = --enable=all --inconclusive --std=c11 --check-level=exhaustive \
                 --library=posix --platform=unix64 -D_GNU_SOURCE \
                 --suppress=missingIncludeSystem --suppress=checkersReport \
                 --suppress=unusedFunction --suppress=unmatchedSuppression \
                 --suppress=readdirCalled --suppress=getpwuidCalled \
                 -Iinclude --error-exitcode=1
# readdirCalled : each DIR* is used by exactly one thread (phase 1 is single-threaded);
#                 readdir_r is deprecated in glibc.
# getpwuidCalled: only reached from print_file_info(), which main() disables when
#                 more than one thread is used.

cppcheck:
	mkdir -p reports
	$(CPPCHECK) $(CPPCHECK_FLAGS) --template='{file}:{line}: {severity}: {message} [{id}]' src 2> reports/cppcheck.txt; \
	  rc=$$?; grep -v '^Checking\|files checked' reports/cppcheck.txt; \
	  echo "cppcheck exit code: $$rc"; exit $$rc

misra:
	mkdir -p reports build/misra
	rm -f build/misra/*.dump
	$(CPPCHECK) --dump --library=posix --platform=unix64 --std=c11 -D_GNU_SOURCE \
	  -D__USE_UNIX98 -D__USE_XOPEN2K8 -I/usr/include/x86_64-linux-gnu -I/usr/include \
	  -Iinclude src > /dev/null 2>&1
	mv src/*.dump build/misra/
	for f in build/misra/*.dump; do python3 tools/misra_run.py $$f; done | sort -u > reports/misra_raw.txt
	python3 tools/misra_report.py reports/misra_raw.txt | tee reports/misra_summary.txt

valgrind: $(TARGET)
	sh tools/valgrind_run.sh

sanitize:
	$(MAKE) TBUILD=build/asan TEXTRA="-fsanitize=address,undefined -fno-omit-frame-pointer" \
	        TLDEXTRA="-fsanitize=address,undefined" build/asan/unit_tests
	ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 build/asan/unit_tests

check: test coverage cppcheck misra valgrind sanitize

.PHONY: all clean test coverage cppcheck misra valgrind sanitize check
