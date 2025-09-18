# Makefile para compilar o projeto Pix

CXX = g++
CXXFLAGS = -Wall -pthread -std=c++11
SRC_DIR = src
BUILD_DIR = build

all: server client

server: 
	$(CXX) $(CXXFLAGS) \
	$(SRC_DIR)/server/main.cpp \
	$(SRC_DIR)/server/discovery.cpp \
	$(SRC_DIR)/server/processing.cpp \
	$(SRC_DIR)/server/interface.cpp \
	$(SRC_DIR)/common/utils.cpp \
	-o $(BUILD_DIR)/server.exe

client:
	$(CXX) $(CXXFLAGS) \
	$(SRC_DIR)/client/main.cpp \
	$(SRC_DIR)/client/discovery.cpp \
	$(SRC_DIR)/client/request.cpp \
	$(SRC_DIR)/client/interface.cpp \
	$(SRC_DIR)/common/utils.cpp \
	-o $(BUILD_DIR)/client.exe

clean:
	rmdir /S /Q $(BUILD_DIR) 2>nul || true
	mkdir $(BUILD_DIR)

.PHONY: all server client clean
