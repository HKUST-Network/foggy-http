.PHONY: all clean nginx-module cp4 foggy-llama foggy-llama-check foggy-llama-test-build
all:
	$(MAKE) -C foggy-http
nginx-module:
	$(MAKE) -C foggy-http nginx-module
clean:
	$(MAKE) -C foggy-http clean
cp4: foggy-llama
foggy-llama foggy-llama-check foggy-llama-test-build:
	$(MAKE) -C foggy-http $@
