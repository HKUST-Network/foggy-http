CC ?= cc
CXX ?= g++
CFLAGS = -std=c11 -D_POSIX_C_SOURCE=200809L -O1 -g -pthread -Wall -Wextra -Wpedantic
CXXFLAGS = -std=c++17 -O1 -g -pthread -Wall -Wextra
CPPFLAGS = -Iinc -I../thirdparty/foggytcp/foggytcp/inc
BUILD = ../build
UPSTREAM = ../thirdparty/foggytcp/foggytcp
TRANSPORT = $(addprefix $(BUILD)/,foggy_tcp.o foggy_backend.o foggy_function.o foggy_packet.o transport.o)
CLIENT = $(BUILD)/foggy-http-client
SERVER = $(BUILD)/foggy-http-server
all: $(CLIENT) $(SERVER) $(BUILD)/SHA256SUMS
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
$(BUILD)/client.o: src/client.cc inc/foggy_http.h inc/transport.h
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) -x c -c $< -o $@
$(BUILD)/server.o: src/server.cc inc/foggy_http.h inc/transport.h
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) -x c -c $< -o $@
$(CLIENT): $(BUILD)/client.o $(BUILD)/http.o $(TRANSPORT)
	$(CXX) $(CXXFLAGS) $^ -o $@
$(SERVER): $(BUILD)/server.o $(BUILD)/http.o $(TRANSPORT)
	$(CXX) $(CXXFLAGS) $^ -o $@
$(BUILD)/SHA256SUMS: $(CLIENT) $(SERVER) conf/nginx-stream.conf ../submit.py
	cp conf/nginx-stream.conf $(BUILD)/nginx-stream.conf
	python3 ../submit.py --record-build
clean:
	rm -rf $(BUILD)
.PHONY: all clean
