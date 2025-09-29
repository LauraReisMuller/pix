# Makefile para compilar o projeto Pix

CXX = g++
CXXFLAGS = -Wall -pthread -std=c++17 -Iinclude
SRC_DIR = src
BUILD_DIR = build

all: $(BUILD_DIR) server client
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

server: $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) \
	$(SRC_DIR)/server/main.cpp \
	$(SRC_DIR)/server/discovery.cpp \
	$(SRC_DIR)/server/processing.cpp \
	$(SRC_DIR)/server/interface.cpp \
	$(SRC_DIR)/common/utils.cpp \
	-o $(BUILD_DIR)/server.exe

client: $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) \
	$(SRC_DIR)/client/main.cpp \
	$(SRC_DIR)/client/discovery.cpp \
	$(SRC_DIR)/client/request.cpp \
	$(SRC_DIR)/client/interface.cpp \
	$(SRC_DIR)/common/utils.cpp \
	-o $(BUILD_DIR)/client.exe

clean:
	# Remove arquivos e diretório de build (compatível com Linux)
	rm -f $(BUILD_DIR)/*.exe
	rmdir $(BUILD_DIR) || true

.PHONY: all server client clean