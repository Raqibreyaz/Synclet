VCPKG := $(HOME/vcpkg)
CXX := g++
CXX_FLAGS :=  -std=c++20 -I$(VCPKG)/installed/x64-linux/include

SRC := main.cpp
OUT := synclet

all: 
	$(CXX) $(CXX_FLAGS) $(SRC) -o $(OUT) 

clean:	
	rm -rf $(OUT)