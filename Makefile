# 
# Copyright (c) 2021-2023 Noah Orensa.
# Licensed under the MIT license. See LICENSE file in the project root for details.
# 

# module name
MODULE = libspl

################################################################################

BUILD_DIR = build/$(shell uname -s)-$(shell uname -m)
LIB_DIR = lib/$(shell uname -s)-$(shell uname -m)

INCLUDES = -Iinclude

CXX = g++
CPPFLAGS = -Werror -Wall -Winline -Wpedantic
CXXFLAGS = -std=c++11 -march=native -fopenmp -pthread -fPIC

AR = ar
ARFLAGS = rc

LDFLAGS = -Wl,-E -Wl,-export-dynamic
DEPFLAGS = -MM

SOURCES = $(wildcard src/*.cpp)
OBJ_FILES = $(SOURCES:src/%.cpp=$(BUILD_DIR)/%.o)

.PHONY : all libspl test clean clean-dep

all : libspl

test : libspl
	@$(MAKE) -C test --no-print-directory EXTRACXXFLAGS="$(EXTRACXXFLAGS)" nodep="$(nodep)"
	@./test/dtest/dtest-cxx11

test-build-only : libspl
	@$(MAKE) -C test --no-print-directory EXTRACXXFLAGS="$(EXTRACXXFLAGS)" nodep="$(nodep)"

libspl : $(LIB_DIR)/libspl.so $(LIB_DIR)/libspl.a

ifndef nodep
include $(SOURCES:src/%.cpp=.dep/%.d)
else
ifneq ($(nodep), true)
include $(SOURCES:src/%.cpp=.dep/%.d)
endif
endif

# cleanup

clean :
	@rm -rf build lib
	@echo "Cleaned $(MODULE)/build/"
	@echo "Cleaned $(MODULE)/lib/"
	@$(MAKE) -C test --no-print-directory clean nodep="$(nodep)"

clean-dep :
	@rm -rf .dep
	@echo "Cleaned $(MODULE)/.dep/"
	@$(MAKE) -C test --no-print-directory clean-dep nodep="$(nodep)"

# dirs

.dep $(BUILD_DIR) $(LIB_DIR):
	@echo "MKDIR     $(MODULE)/$@/"
	@mkdir -p $@

# core

$(LIB_DIR)/libspl.so: $(OBJ_FILES) | $(LIB_DIR)
	@echo "LD        $(MODULE)/$@"
	@$(CXX) -shared $(CXXFLAGS) $(EXTRACXXFLAGS) $(OBJ_FILES) -o $@

$(LIB_DIR)/libspl.a: $(OBJ_FILES) | $(LIB_DIR)
	@echo "AR        $(MODULE)/$@"
	@ar rcs $@ $(OBJ_FILES)

.dep/%.d : src/%.cpp | .dep
	@echo "DEP       $(MODULE)/$@"
	@set -e; rm -f $@; \
	$(CXX) $(DEPFLAGS) $(INCLUDES) $< > $@.$$$$; \
	sed 's,\($*\)\.o[ :]*,$(BUILD_DIR)/\1.o $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$

$(BUILD_DIR)/%.o : src/%.cpp | $(BUILD_DIR)
	@echo "CXX       $(MODULE)/$@"
	@$(CXX) -c $(CPPFLAGS) $(CXXFLAGS) $(EXTRACXXFLAGS) $(INCLUDES) $< -o $@
