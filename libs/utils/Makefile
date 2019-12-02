UTILS_DIR = $(LIBS_DIR)/utils
UTILS_SRCS += $(wildcard $(UTILS_DIR)/*.c)
UTILS_OBJS = $(addsuffix .obj, $(basename $(UTILS_SRCS)))

ifneq ($(UTILS_OBJS),)
$(LIBS_DIR)/libutils.lib: $(UTILS_OBJS)
main.exe: $(LIBS_DIR)/libutils.lib
endif

ifeq ($(TOOLCHAIN),nxdk)
UTILS_FLAGS = -I$(UTILS_DIR)
CFLAGS   += $(UTILS_FLAGS)
CXXFLAGS += $(UTILS_FLAGS)
endif

CLEANRULES += clean-utils

.PHONY: clean-utils
clean-utils:
	$(VE)rm -f $(UTILS_OBJS) \
	           $(LIBS_DIR)/libutils.lib
