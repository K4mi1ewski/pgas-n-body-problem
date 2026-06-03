# UPC++ N-body (PGAS)
#   export UPCXX_INSTALL=/path/to/upcxx
#   make
#   make run RUN_PROCS=8 INPUT=input.txt OUTPUT=result.txt
#
# Optional:
#   make opt          # optimized UPC++ library
#   make threaded     # OpenMP + UPCXX_THREADMODE=par

ifeq ($(UPCXX_INSTALL),)
$(warning UPCXX_INSTALL is not set; assuming upcxx and upcxx-run are in PATH)
UPCXX = upcxx
UPCXXRUN = upcxx-run
else
ifeq ($(wildcard $(UPCXX_INSTALL)/bin/upcxx),)
$(error Set UPCXX_INSTALL to your UPC++ install prefix)
else
UPCXX = $(UPCXX_INSTALL)/bin/upcxx
UPCXXRUN = $(UPCXX_INSTALL)/bin/upcxx-run
endif
endif

UPCXX_THREADMODE ?= seq
export UPCXX_THREADMODE
UPCXX_CODEMODE ?= debug
export UPCXX_CODEMODE

CXX = $(UPCXX)
TARGET = nbody_upcxx
SRC = nbody_upcxx.cpp
CXXFLAGS ?= -std=c++14 -Wall -Wextra
LDFLAGS ?= -lm

RUN_PROCS ?= 4
INPUT ?= input.txt
OUTPUT ?= output.txt
SHARED_HEAP ?= 512M

.PHONY: all clean run opt threaded

all: $(TARGET)

$(TARGET): $(SRC) nbody_common.hpp
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SRC) $(LDFLAGS)

opt:
	$(MAKE) UPCXX_CODEMODE=opt

threaded:
	$(MAKE) UPCXX_THREADMODE=par CXXFLAGS+='-fopenmp'

run: $(TARGET)
	OUTPUT_FILE="$(OUTPUT)" $(UPCXXRUN) -shared-heap $(SHARED_HEAP) -n $(RUN_PROCS) ./$(TARGET) $(INPUT)

clean:
	rm -f $(TARGET) $(TARGET).exe
