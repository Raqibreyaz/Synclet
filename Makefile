VCPKG := $(HOME)/vcpkg
CXX := g++
CXX_FLAGS := -Wall -Wextra -g -std=c++20 -I$(VCPKG)/installed/x64-linux/include

CLIENT_SRCS := client/client.cpp \
	src/message.cpp \
	src/tcp-socket.cpp \
	src/utils.cpp

SERVER_SRCS := server/server.cpp \
	src/message.cpp \
	src/tcp-socket.cpp \
	src/utils.cpp

CLIENT_OBJS = $(CLIENT_SRCS:.cpp=.o)
SERVER_OBJS = $(SERVER_SRCS:.cpp=.o)

CLIENT_OUT = client/client
SERVER_OUT = server/server

%.o: %.cpp
	$(CXX) $(CXX_FLAGS) -c $< -o $@

$(CLIENT_OUT): $(CLIENT_OBJS)
		$(CXX) $(CXX_FLAGS) -o $@ $^ -lssl -lcrypto

$(SERVER_OUT): $(SERVER_OBJS)
		$(CXX) $(CXX_FLAGS) -o $@ $^ -lssl -lcrypto

all: $(CLIENT_OUT) $(SERVER_OUT) 

clean:	
	rm -rf $(CLIENT_OUT) $(SERVER_OUT) $(SERVER_OBJS) $(CLIENT_OBJS)



# g++ a.cpp b.cpp -o x