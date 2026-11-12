# CPU-only build of the pinned llama.cpp sources for the course ARM64 VMs.
LLAMA_CXX = $(patsubst $(LLAMA_SOURCE)/%,%,$(wildcard $(LLAMA_SOURCE)/src/*.cpp $(LLAMA_SOURCE)/src/models/*.cpp))
GGML_C = ggml.c ggml-alloc.c ggml-quants.c ggml-cpu/ggml-cpu.c ggml-cpu/quants.c ggml-cpu/arch/arm/quants.c
GGML_CXX = ggml.cpp ggml-backend.cpp ggml-backend-meta.cpp ggml-backend-dl.cpp ggml-backend-reg.cpp \
    ggml-opt.cpp ggml-threading.cpp gguf.cpp ggml-cpu/ggml-cpu.cpp ggml-cpu/repack.cpp \
    ggml-cpu/iqp.cpp ggml-cpu/hbm.cpp ggml-cpu/traits.cpp ggml-cpu/amx/amx.cpp \
    ggml-cpu/amx/mmq.cpp ggml-cpu/binary-ops.cpp ggml-cpu/unary-ops.cpp \
    ggml-cpu/vec.cpp ggml-cpu/ops.cpp ggml-cpu/arch/arm/repack.cpp
SOURCES = $(LLAMA_CXX) $(addprefix ggml/src/,$(GGML_C) $(GGML_CXX))
OBJECTS = $(addprefix $(BUILD)/,$(addsuffix .o,$(SOURCES)))
VERSIONS = $(BUILD)/llama-version.h $(BUILD)/ggml-version.h
CPU_FLAGS = -D_GNU_SOURCE -DGGML_USE_CPU -DNDEBUG -pthread -I$(BUILD) \
    -I$(LLAMA_SOURCE)/include -I$(LLAMA_SOURCE)/src -I$(LLAMA_SOURCE)/ggml/include \
    -I$(LLAMA_SOURCE)/ggml/src -I$(LLAMA_SOURCE)/ggml/src/ggml-cpu
CPU_MAKEFILE = $(lastword $(MAKEFILE_LIST))

$(BUILD)/libllama.a: $(OBJECTS) $(CPU_MAKEFILE)
	@rm -f "$@.tmp"
	$(AR) rcs "$@.tmp" $(OBJECTS)
	@mv "$@.tmp" "$@"

$(BUILD)/llama-version.h: $(LLAMA_SOURCE)/src/llama-version.h.in $(CPU_MAKEFILE)
	@mkdir -p $(@D)
	@sed -e 's/@LLAMA_VERSION@/0.5.0/g' -e 's/@LLAMA_BUILD_COMMIT@/1ab7e5a/g' "$<" > "$@"

$(BUILD)/ggml-version.h: $(LLAMA_SOURCE)/ggml/src/ggml-version.h.in $(CPU_MAKEFILE)
	@mkdir -p $(@D)
	@sed -e 's/@GGML_VERSION@/0.25.3/g' -e 's/@GGML_BUILD_COMMIT@/1ab7e5a/g' "$<" > "$@"

$(BUILD)/%.c.o: $(LLAMA_SOURCE)/%.c $(VERSIONS) $(CPU_MAKEFILE)
	@mkdir -p $(@D)
	$(CC) -std=gnu11 -O2 $(CPU_FLAGS) -MMD -MP -c $< -o $@

$(BUILD)/%.cpp.o: $(LLAMA_SOURCE)/%.cpp $(VERSIONS) $(CPU_MAKEFILE)
	@mkdir -p $(@D)
	$(CXX) -std=gnu++17 -O1 $(CPU_FLAGS) -MMD -MP -c $< -o $@

-include $(OBJECTS:.o=.d)
.DELETE_ON_ERROR:
