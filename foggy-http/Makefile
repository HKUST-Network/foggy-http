CC ?= cc
CXX ?= g++
CFLAGS = -std=c11 -D_POSIX_C_SOURCE=200809L -O1 -g -pthread -Wall -Wextra -Wpedantic
CXXFLAGS = -std=c++17 -O1 -g -pthread -Wall -Wextra
CPPFLAGS = -Iinc -I../thirdparty/foggytcp/foggytcp/inc
BUILD = ../build
UPSTREAM = ../thirdparty/foggytcp/foggytcp
TRANSPORT = $(addprefix $(BUILD)/,foggy_tcp.o foggy_backend.o foggy_function.o foggy_packet.o transport.o)
PROGRAMS = date-client date-server echo-client echo-server

CP4 = ../foggy-llama
CP4_BUILD = $(BUILD)/foggy-llama
CP4_CPPFLAGS = $(CPPFLAGS) -I$(CP4)/inc -I$(CP4)/vendor
CP4_CXXFLAGS = $(filter-out -std=%,$(CXXFLAGS)) -std=c++11 -Wpedantic
CP4_HEADERS = $(wildcard $(CP4)/inc/*.h) $(CP4)/vendor/json.hpp inc/foggy_http.h inc/transport.h
CP4_COMMON = $(addprefix $(CP4_BUILD)/,api.o mock_engine.o client_flow.o llama_routes.o http.o) $(TRANSPORT)
CP4_MOCK_SERVER = $(BUILD)/test-mock/foggy-server
CP4_MODEL = $(CP4)/models/SmolLM2-135M-Instruct-Q4_K_M.gguf
CP4_TEST_DIR ?= ../cp4/integration/tests
FOGGY_HTTP_READ_MS ?= 120000
LLAMA_SOURCE = ../thirdparty/llama.cpp
LLAMA_PLATFORM = $(shell uname -s)-$(shell uname -m)
ifneq ($(filter Linux-aarch64 Linux-arm64,$(LLAMA_PLATFORM)),)
LLAMA_RUNTIME = $(BUILD)/llama-cpu/libllama.a
LLAMA_LDLIBS = "$(LLAMA_RUNTIME)" -ldl -lm
LLAMA_JOBS = $(if $(filter -j% --jobserver%,$(MAKEFLAGS)),,-j2)
else
LLAMA_RUNTIME = $(BUILD)/.llama-runtime
LLAMA_LDLIBS = -L"$(BUILD)" -lllama -lggml -lggml-base $(LLAMA_RPATH)
endif

ifeq ($(shell uname -s),Darwin)
LLAMA_RPATH = -Wl,-rpath,@loader_path
else
LLAMA_RPATH = -Wl,-rpath,'$$ORIGIN'
endif

all: $(addprefix $(BUILD)/,$(PROGRAMS)) $(BUILD)/SHA256SUMS nginx-module
nginx-module:
	+bash support/nginx/install.sh "$(abspath $(BUILD))/ngx_stream_foggy_http_module.so"
$(BUILD)/foggy_%.o: $(UPSTREAM)/src/foggy_%.cc $(wildcard $(UPSTREAM)/inc/*.h) inc/transport_log.h
	@mkdir -p $(@D)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -include inc/transport_log.h -c $< -o $@
# FoggyTCP uses C++ internally. This adapter exports a C ABI to the HTTP layer.
$(BUILD)/transport.o: src/transport.cc inc/transport.h $(UPSTREAM)/inc/foggy_tcp.h
	@mkdir -p $(@D)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@
# .cc matches FoggyTCP naming; -x c enforces actual C11 compilation.
$(BUILD)/http.o: src/foggy_http.cc inc/foggy_http.h inc/transport.h
	@mkdir -p $(@D)
	cp src/foggy_http.cc $(BUILD)/foggy_http.cc
	$(CC) $(CPPFLAGS) $(CFLAGS) -x c -c $(BUILD)/foggy_http.cc -o $@
$(BUILD)/apps/%.o: src/%.cc src/client_common.h src/server_common.h inc/foggy_http.h inc/transport.h
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) -x c -c $< -o $@
$(addprefix $(BUILD)/,$(PROGRAMS)): $(BUILD)/%: $(BUILD)/apps/%.o $(BUILD)/http.o $(TRANSPORT)
	$(CXX) $(CXXFLAGS) $^ -o $@
$(BUILD)/SHA256SUMS: $(addprefix $(BUILD)/,$(PROGRAMS)) conf/nginx-stream.conf ../submit.py
	cp conf/nginx-stream.conf $(BUILD)/nginx-stream.conf
	python3 ../submit.py --record-build

cp4: $(BUILD)/foggy-server $(BUILD)/foggy-client $(BUILD)/foggy-http-probe $(CP4_MODEL)
foggy-llama-test-build: $(CP4_MOCK_SERVER) $(BUILD)/foggy-client $(BUILD)/test-core

$(CP4_MODEL): $(CP4)/vendor/fetch_model.py $(CP4)/vendor/model.json
	python3 $(CP4)/vendor/fetch_model.py
	@touch "$@"
ifneq ($(filter Linux-aarch64 Linux-arm64,$(LLAMA_PLATFORM)),)
$(LLAMA_RUNTIME): FORCE
	+$(MAKE) $(LLAMA_JOBS) -f $(CP4)/vendor/llama-cpu.mk "LLAMA_SOURCE=$(LLAMA_SOURCE)" "BUILD=$(BUILD)/llama-cpu"
else
$(LLAMA_RUNTIME): FORCE $(CP4)/vendor/fetch_llama.py $(CP4)/vendor/fetch_model.py $(CP4)/vendor/llama-release.json
	@python3 $(CP4)/vendor/fetch_llama.py "$(BUILD)"
endif

# Compile the same HTTP source with CP4 flags in a separate object.
$(CP4_BUILD)/http.o: src/foggy_http.cc inc/foggy_http.h inc/transport.h Makefile
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) -DFOGGY_CP4=1 -DFOGGY_HTTP_READ_MS=$(FOGGY_HTTP_READ_MS) -x c -c $< -o $@
# The student route registration uses the same C11 conventions as the HTTP source.
$(CP4_BUILD)/llama_routes.o: $(CP4)/src/llama_routes.cc $(CP4)/inc/foggy_llama.h inc/foggy_http.h inc/transport.h Makefile
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) -I$(CP4)/inc $(CFLAGS) -x c -c $< -o $@
$(CP4_BUILD)/%.o: $(CP4)/src/%.cc $(CP4_HEADERS) Makefile
	@mkdir -p $(@D)
	$(CXX) $(CP4_CPPFLAGS) $(CP4_CXXFLAGS) -c $< -o $@
$(CP4_MOCK_SERVER): $(CP4_BUILD)/server.o $(CP4_COMMON)
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) $^ -o $@
$(BUILD)/foggy-client: $(CP4_BUILD)/client.o $(CP4_COMMON)
	$(CXX) $(CXXFLAGS) $^ -o $@
$(CP4_BUILD)/server-llama.o: $(CP4)/src/server.cc $(CP4_HEADERS) Makefile
	@mkdir -p $(@D)
	$(CXX) $(CP4_CPPFLAGS) $(CP4_CXXFLAGS) -DFOGGY_WITH_LLAMA -c $< -o $@
$(CP4_BUILD)/llama_engine.o: $(CP4)/src/llama_engine.cc $(CP4_HEADERS) $(LLAMA_SOURCE)/include/llama.h $(wildcard $(LLAMA_SOURCE)/ggml/include/*.h) Makefile
	@mkdir -p $(@D)
	$(CXX) $(CP4_CPPFLAGS) -I$(LLAMA_SOURCE)/include -I$(LLAMA_SOURCE)/ggml/include $(CP4_CXXFLAGS) -c $< -o $@
$(BUILD)/foggy-server: $(CP4_BUILD)/server-llama.o $(CP4_BUILD)/llama_engine.o $(CP4_COMMON) $(LLAMA_RUNTIME)
	$(CXX) $(CXXFLAGS) $(filter %.o,$^) $(LLAMA_LDLIBS) -o $@
$(CP4_BUILD)/http-probe.o: $(CP4)/tests/http_probe.cc inc/transport.h Makefile
	@mkdir -p $(@D)
	$(CXX) $(CPPFLAGS) $(CP4_CXXFLAGS) -c $< -o $@
$(BUILD)/foggy-http-probe: $(CP4_BUILD)/http-probe.o $(TRANSPORT)
	$(CXX) $(CXXFLAGS) $^ -o $@
$(CP4_BUILD)/test-core.o: $(CP4_TEST_DIR)/test_core.cc $(CP4_HEADERS) Makefile
	@mkdir -p $(@D)
	$(CXX) $(CP4_CPPFLAGS) $(CP4_CXXFLAGS) -c $< -o $@
$(BUILD)/test-core: $(CP4_BUILD)/test-core.o $(CP4_COMMON)
	$(CXX) $(CXXFLAGS) $^ -o $@

clean:
	rm -rf $(BUILD)
.PHONY: all clean nginx-module cp4 foggy-llama-test-build FORCE
