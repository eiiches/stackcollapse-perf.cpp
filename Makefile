SOURCES := stackcollapse-perf.cpp
CXXFLAGS := -std=c++11 -g -O3 -Wall -pedantic

OUTPUT_EXE := stackcollapse-perf
OUTPUT_STATIC_EXE := stackcollapse-perf.static
OUTPUT_DEBUG_EXE := stackcollapse-perf.debug

PREFIX := /usr/local

.PHONY: all
all: shared

.PHONY: shared static debug
shared: $(OUTPUT_EXE)
static: $(OUTPUT_STATIC_EXE)
debug: $(OUTPUT_DEBUG_EXE)

$(OUTPUT_EXE): $(SOURCES) Makefile
	$(CXX) $(CXXFLAGS) -o $@ $(SOURCES)

$(OUTPUT_DEBUG_EXE): $(SOURCES) Makefile
	$(CXX) $(CXXFLAGS) -o $@ -g -fno-inline -fno-omit-frame-pointer $(SOURCES)

$(OUTPUT_STATIC_EXE): $(SOURCES) Makefile
	$(CXX) $(CXXFLAGS) -o $@ -g -static $(SOURCES)

.PHONY: clean
clean:
	$(RM) $(OUTPUT_EXE) $(OUTPUT_DEBUG_EXE) $(OUTPUT_STATIC_EXE)

.PHONY: install
install: $(OUTPUT_EXE)
	mkdir -p $(PREFIX)/bin
	install -m 0755 -o root -g root $(OUTPUT_EXE) $(PREFIX)/bin/$(OUTPUT_EXE)

# -----------------------------------------------------------------------------
# for developers

BROWSER := chromium
FLAMEGRAPH := flamegraph.pl
SAMPLE_PERF := sample.perf
PROFILE_PERF := profile.perf

.PHONY: require-root
require-root:
	@if [ `id -u` -ne 0 ]; then echo -e "\nError: Permission denied. You must be root to collect system-wide stats.\n" >&2; exit 1; fi

$(SAMPLE_PERF):
	@$(MAKE) require-root
	perf record -ag -o $@ -- sleep 10

$(SAMPLE_PERF).txt: $(SAMPLE_PERF)
	@$(MAKE) require-root
	perf script -f -i $< > $@

.PHONY: prof
prof: $(OUTPUT_EXE) $(OUTPUT_DEBUG_EXE) $(SAMPLE_PERF).txt
	for i in `seq 10`; do cat $(SAMPLE_PERF).txt | ./$(OUTPUT_EXE) > /dev/null; done
	cat $(SAMPLE_PERF).txt | perf record -o $(PROFILE_PERF) -g ./$(OUTPUT_DEBUG_EXE) > /dev/null
	perf script -f -i $(PROFILE_PERF) | sed 's/+0x[0-9a-f]\+//g' | ./$(OUTPUT_EXE) | $(FLAMEGRAPH) > $(PROFILE_PERF).svg
	$(BROWSER) $(PROFILE_PERF).svg

.PHONY: sample
sample:
	$(RM) $(SAMPLE_PERF) $(SAMPLE_PERF).txt
	$(MAKE) $(SAMPLE_PERF).txt

.PHONY: graph
graph: $(SAMPLE_PERF).txt $(OUTPUT_EXE)
	cat $(SAMPLE_PERF).txt | ./$(OUTPUT_EXE) | $(FLAMEGRAPH) > $(SAMPLE_PERF).svg
	$(BROWSER) $(SAMPLE_PERF).svg

.PHONY: cleanall
cleanall: clean
	$(RM) $(SAMPLE_PERF).txt $(SAMPLE_PERF).svg $(SAMPLE_PERF) $(PROFILE_PERF) $(PROFILE_PERF).svg $(PROFILE_PERF).old
