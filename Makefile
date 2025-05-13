VCPKG := $(HOME)/vcpkg
CXX := g++
CXX_FLAGS := -Wall -Wextra -g -std=c++20 -I$(VCPKG)/installed/x64-linux/include

SRCS := src/main.cpp \
		src/synclet.cpp

OBJS = $(SRCS:.cpp=.o)

OUT := synclet

%.o: %.cpp
	$(CXX) $(CXX_FLAGS) -c $< -o $@

$(OUT): $(OBJS)
		$(CXX) $(CXX_FLAGS) -o $@ $^ -lssl -lcrypto

all: 
	$(CXX) $(CXX_FLAGS) $(SRC) -o $(OUT) 

clean:	
	rm -rf $(OUT) $(OBJS)

# g++ a.cpp b.cpp -o x