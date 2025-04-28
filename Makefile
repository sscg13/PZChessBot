EXE ?= pzchessbot
EVALFILE ?= nnue.bin

CXX := g++
CXXFLAGS := -std=c++17 -march=native -DNNUE_PATH=\"$(EVALFILE)\"
RELEASEFLAGS = -O3
DEBUGFLAGS = -g -fsanitize=address,undefined

SRCS := $(wildcard engine/*.cpp engine/nnue/*.cpp)
HDRS := $(wildcard engine/*.hpp engine/nnue/*.hpp)
OBJS := $(SRCS:.cpp=.o)

.PHONY: release debug clean download

all: release

download: $(EVALFILE)

$(EVALFILE):
	@echo "Downloading NNUE file..."
	@curl -o $(EVALFILE) https://data.stockfishchess.org/nn/nn-23507ff7848b.nnue

$(OBJS): $(EVALFILE)

release: download
release: CXXFLAGS += $(RELEASEFLAGS)
release: $(EXE)

debug: download
debug: CXXFLAGS += $(DEBUGFLAGS)
debug: $(EXE)

$(EXE): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^
	@echo "Build complete. Run with './$(EXE)'"

%.o: %.cpp $(HDRS)
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	@echo "Cleaning up..."
	rm -f $(EXE)
	rm -f $(OBJS)