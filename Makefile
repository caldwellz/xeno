APP_TITLE = xeno
APP_SDL = y

ifeq ($(TOOLCHAIN),)
  TOOLCHAIN = undefined
endif
XENO_DIR = $(CURDIR)
XENO_SRCS  = $(wildcard $(XENO_DIR)/src/*.c)
XENO_SRCS += $(wildcard $(XENO_DIR)/src/*.cpp)
XENO_SRCS += $(wildcard $(XENO_DIR)/engine/*.c)
XENO_SRCS += $(wildcard $(XENO_DIR)/engine/*.cpp)
SRCS += $(XENO_SRCS)
LIBS_DIR = $(XENO_DIR)/libs
ENGINE_DIR = $(XENO_DIR)/engine
include $(XENO_DIR)/Makefile.$(TOOLCHAIN)
include $(LIBS_DIR)/Makefile
