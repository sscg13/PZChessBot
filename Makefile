EXE ?= pzchessbot
EVALFILE ?= nn-a660a82f6a81.nnue

CXX := g++
CXXFLAGS := -std=c++17 -march=native -static -DNNUE_PATH=\"$(EVALFILE)\"
RELEASEFLAGS = -O3
DEBUGFLAGS = -g

SRCS := $(wildcard engine/*.cpp engine/nnue/*.cpp)
HDRS := $(wildcard engine/*.hpp engine/nnue/*.hpp)
OBJS := $(SRCS:.cpp=.o)

.PHONY: release debug clean

release: CXXFLAGS += $(RELEASEFLAGS)
release: $(EXE)

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
