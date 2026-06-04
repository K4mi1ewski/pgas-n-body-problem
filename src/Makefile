# UPC++ N-body (PGAS)
#   make
#   make run RUN_PROCS=8 INPUT=input.txt OUTPUT=result.txt
#
# Optional:
#   make opt          # optimized UPC++ library
#   make threaded     # OpenMP + UPCXX_THREADMODE=par

UPCXX = upcxx
UPCXXRUN = upcxx-run

UPCXX_GASNET_CONDUIT ?= udp
export UPCXX_GASNET_CONDUIT

UPCXX_THREADMODE ?= seq
export UPCXX_THREADMODE
UPCXX_CODEMODE ?= debug
export UPCXX_CODEMODE

CXX = $(UPCXX)
TARGET = nbody_upcxx
SRC = nbody_upcxx.cpp
CXXFLAGS ?= -std=c++14 -Wall -Wextra -O2
LDFLAGS ?= -lm

RUN_PROCS ?= 16
INPUT ?= input.txt
OUTPUT ?= output.txt
SHARED_HEAP ?= 256M
NODEFILE ?= nodes
NODES := $(shell upcxx-nodes $(NODEFILE))

.PHONY: all clean run opt threaded

all: $(TARGET)

$(TARGET): $(SRC) nbody_common.hpp
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SRC) $(LDFLAGS)

opt:
	$(MAKE) UPCXX_CODEMODE=opt

threaded:
	$(MAKE) UPCXX_THREADMODE=par CXXFLAGS+='-fopenmp'

run: $(TARGET)
	OUTPUT_FILE="$(OUTPUT)" $(UPCXXRUN) -shared-heap $(SHARED_HEAP) -n $(RUN_PROCS) $(NODES) ./$(TARGET) $(INPUT)

clean:
	rm -f $(TARGET) $(TARGET).exe
