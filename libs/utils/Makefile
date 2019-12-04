UTILS_DIR = $(LIBS_DIR)/utils
UTILS_SRCS = $(wildcard $(UTILS_DIR)/*.c)
UTILS_SRCS += $(wildcard $(UTILS_DIR)/*.cpp)
UTILS_OBJS = $(addsuffix .obj, $(basename $(UTILS_SRCS)))
UTILS_LIB = $(LIBS_DIR)/libutils$(LIB_EXT)

ifeq ($(TOOLCHAIN),nxdk)
ifneq ($(UTILS_OBJS),)
$(UTILS_LIB): $(UTILS_OBJS)

$(BINTARGET): $(UTILS_LIB)
endif

UTILS_FLAGS = -I$(UTILS_DIR)
CFLAGS   += $(UTILS_FLAGS)
CXXFLAGS += $(UTILS_FLAGS)
endif

CLEANRULES += clean-utils

.PHONY: clean-utils
clean-utils:
	$(VE)rm -f $(UTILS_OBJS) \
	           $(UTILS_LIB)
