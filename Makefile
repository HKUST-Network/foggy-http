.PHONY: all clean nginx-module
all:
	$(MAKE) -C foggy-http
nginx-module:
	$(MAKE) -C foggy-http nginx-module
clean:
	$(MAKE) -C foggy-http clean
