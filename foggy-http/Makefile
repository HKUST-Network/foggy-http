CXX ?= g++
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
$(BUILD)/transport.o: src/transport.cpp inc/transport.h $(UPSTREAM)/inc/foggy_tcp.h
	@mkdir -p $(@D)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@
$(BUILD)/http.o: src/foggy_http.cpp inc/foggy_http.h inc/transport.h
	@mkdir -p $(@D)
	cp src/foggy_http.cpp $(BUILD)/foggy_http.cpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $(BUILD)/foggy_http.cpp -o $@
$(CLIENT): src/client.cpp $(BUILD)/http.o $(TRANSPORT)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $^ -o $@
$(SERVER): src/server.cpp $(BUILD)/http.o $(TRANSPORT)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $^ -o $@
$(BUILD)/SHA256SUMS: $(CLIENT) $(SERVER) conf/nginx-stream.conf ../submit.py
	cp conf/nginx-stream.conf $(BUILD)/nginx-stream.conf
	python3 ../submit.py --record-build
clean:
	rm -rf $(BUILD)
.PHONY: all clean
