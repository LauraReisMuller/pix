# Makefile para compilar o projeto Pix

CXX = g++
CXXFLAGS = -Wall -pthread -std=c++17 -Iinclude
SRC_DIR = src
BUILD_DIR = build

# Portas padrão
SERVER_PORT = 4000
CLIENT_PORT = 4000

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

# Novos targets para executar
run-server: server
	@echo "Iniciando servidor na porta $(SERVER_PORT)..."
	./$(BUILD_DIR)/server.exe $(SERVER_PORT)

run-client: client
	@echo "Iniciando cliente na porta $(CLIENT_PORT)..."
	./$(BUILD_DIR)/client.exe $(CLIENT_PORT)

# Target para rodar servidor em background
start-server: server
	@echo "Iniciando servidor em background na porta $(SERVER_PORT)..."
	./$(BUILD_DIR)/server.exe $(SERVER_PORT) &
	@echo "Servidor iniciado. PID: $$!"

# Target para testar (roda servidor em background e depois o cliente)
test: server client
	@echo "Iniciando teste completo..."
	@echo "Iniciando servidor em background..."
	./$(BUILD_DIR)/server.exe $(SERVER_PORT) & \
	SERVER_PID=$$!; \
	sleep 2; \
	echo "Iniciando cliente..."; \
	./$(BUILD_DIR)/client.exe $(CLIENT_PORT); \
	echo "Encerrando servidor..."; \
	kill $$SERVER_PID 2>/dev/null || true

# Targets to run individual test scripts in the tests/ folder
run-tests-client:
	@echo "Running tests/run_client.sh..."
	bash tests/run_client.sh

run-tests-client2:
	@echo "Running tests/run_client2.sh..."
	bash tests/run_client2.sh

run-tests-server:
	@echo "Running tests/run_server.sh..."
	bash tests/run_server.sh

run-tests:
	@echo "Running tests/test.sh..."
	bash tests/test.sh

# Target para verificar se os executáveis existem
check:
	@echo "Verificando executáveis..."
	@if [ -f $(BUILD_DIR)/server.exe ]; then \
		echo "✓ Server compilado"; \
	else \
		echo "✗ Server não encontrado"; \
	fi
	@if [ -f $(BUILD_DIR)/client.exe ]; then \
		echo "✓ Client compilado"; \
	else \
		echo "✗ Client não encontrado"; \
	fi

clean:	
	@echo "Limpando arquivos compilados..."
	rm -f $(BUILD_DIR)/*.exe
	rmdir $(BUILD_DIR) 2>/dev/null || true
	@echo "Limpeza concluída."

# Target para matar processos do servidor (útil se ficou rodando)
kill-server:
	@echo "Procurando processos do servidor..."
	@pkill -f "server.exe" || echo "Nenhum processo do servidor encontrado"

.PHONY: all server client run-server run-client start-server test check help clean kill-server \
 	run-tests-client run-tests-client2 run-tests-server run-tests