# Makefile para compilar o projeto Pix

CXX = g++
# Adiciona -Iinclude para o compilador encontrar os headers
CXXFLAGS = -Wall -pthread -std=c++17 -Iinclude
SRC_DIR = src
BUILD_DIR = build

# Portas padrão
SERVER_PORT = 4000
CLIENT_PORT = 4000

# Variáveis para o Teste de Concorrência
TEST_SRC = $(SRC_DIR)/server/concurrency_test.cpp
TEST_EXE = $(BUILD_DIR)/concurrency_test.exe

# Lista de todos os arquivos .cpp necessários para o link do Teste de Concorrência
TEST_OBJS_DEPS = \
    $(SRC_DIR)/server/database.cpp \
    $(SRC_DIR)/common/utils.cpp \
    $(SRC_DIR)/server/locks.cpp

# ==============================================================================
# TARGETS PRINCIPAIS
# ==============================================================================

all: $(BUILD_DIR) server client

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

server: $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) \
	$(SRC_DIR)/server/main.cpp \
	$(SRC_DIR)/server/discovery.cpp \
	$(SRC_DIR)/server/processing.cpp \
	$(SRC_DIR)/server/interface.cpp \
	$(SRC_DIR)/server/database.cpp \
	$(SRC_DIR)/server/locks.cpp \
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

# ==============================================================================
# TARGETS DE EXECUÇÃO E TESTE DE REDE (Original)
# ==============================================================================

run-server: server
	@echo "Iniciando servidor na porta $(SERVER_PORT)..."
	./$(BUILD_DIR)/server.exe $(SERVER_PORT)

run-client: client
	@echo "Iniciando cliente na porta $(CLIENT_PORT)..."
	./$(BUILD_DIR)/client.exe $(CLIENT_PORT)

# Target para testar (mantido para testes de rede)
test: server client
	@echo "Iniciando teste completo de rede..."
	@echo "Iniciando servidor em background..."
	./$(BUILD_DIR)/server.exe $(SERVER_PORT) & \
	SERVER_PID=$$!; \
	sleep 2; \
	echo "Iniciando cliente..."; \
	./$(BUILD_DIR)/client.exe $(CLIENT_PORT); \
	echo "Encerrando servidor..."; \
	kill $$SERVER_PID 2>/dev/null || true

# ==============================================================================
# TARGET DE TESTE DE THREAD SAFETY (CONCORRÊNCIA)
# ==============================================================================

# 1. Regra para compilar o executável de teste (build/concurrency_test.exe)
$(TEST_EXE): $(TEST_SRC) $(TEST_OBJS_DEPS)
	$(CXX) $(CXXFLAGS) \
	$(TEST_SRC) \
	$(TEST_OBJS_DEPS) \
	-o $@

# 2. Regra para executar o teste de concorrência
concurrency-test-db: $(TEST_EXE)
	@echo "Rodando Teste de Atomicidade e Consistência (8 Threads)..."
	./$(TEST_EXE)

# ==============================================================================
# TARGETS DE LIMPEZA E UTILS
# ==============================================================================

clean:	
	@echo "Limpando arquivos compilados..."
	rm -f $(BUILD_DIR)/*.exe
	rmdir $(BUILD_DIR) 2>/dev/null || true
	@echo "Limpeza concluída."

kill-server:
	@echo "Procurando processos do servidor..."
	@pkill -f "server.exe" || echo "Nenhum processo do servidor encontrado"

.PHONY: all server client run-server run-client test clean kill-server concurrency-test-db