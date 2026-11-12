.PHONY: all clean nginx-module cp4 foggy-llama-test-build
all:
	$(MAKE) -C foggy-http
nginx-module:
	$(MAKE) -C foggy-http nginx-module
clean:
	$(MAKE) -C foggy-http clean
cp4:
	@test -f thirdparty/foggytcp/foggytcp/inc/foggy_tcp.h || git submodule update --init --depth 1 -- thirdparty/foggytcp
	@test -f thirdparty/llama.cpp/include/llama.h || git submodule update --init --depth 1 -- thirdparty/llama.cpp
	$(MAKE) -C foggy-http cp4
foggy-llama-test-build:
	$(MAKE) -C foggy-http $@
