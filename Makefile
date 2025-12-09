# Makefile para compilar o projeto Pix

CXX = g++
CXXFLAGS = -Wall -pthread -std=c++17 -Iinclude
SRC_DIR = src

# Portas padrão
SERVER_PORT = 4000
CLIENT_PORT = 4000
BACKUP_PORT = 4001

all: server client

server:
	$(CXX) $(CXXFLAGS) \
	$(SRC_DIR)/server/main.cpp \
	$(SRC_DIR)/server/discovery.cpp \
	$(SRC_DIR)/server/processing.cpp \
	$(SRC_DIR)/server/interface.cpp \
	$(SRC_DIR)/server/database.cpp \
	$(SRC_DIR)/server/locks.cpp \
	$(SRC_DIR)/common/utils.cpp \
	$(SRC_DIR)/server/replication.cpp \
	-o ./servidor.exe

client:
	$(CXX) $(CXXFLAGS) \
	$(SRC_DIR)/client/main.cpp \
	$(SRC_DIR)/client/discovery.cpp \
	$(SRC_DIR)/client/request.cpp \
	$(SRC_DIR)/client/interface.cpp \
	$(SRC_DIR)/common/utils.cpp \
	-o ./cliente.exe

# === SHORTCUTS PARA TESTE DE REPLICAÇÃO (ETAPA 2) ===

# Roda o LÍDER (Porta 4000, ID 0, Leader=1)
run-leader: server
	@echo "--- Iniciando LÍDER na porta $(SERVER_PORT) ---"
	./servidor.exe $(SERVER_PORT) 0 1

# Roda o BACKUP (Porta 4001, ID 1, Leader=0)
run-backup: server
	@echo "--- Iniciando BACKUP na porta $(BACKUP_PORT) ---"
	./servidor.exe $(BACKUP_PORT) 1 0

# Novos targets para executar
run-server: server
	@echo "Iniciando servidor na porta $(SERVER_PORT)..."
	./servidor.exe $(SERVER_PORT)

run-client: client
	@echo "Iniciando cliente na porta $(CLIENT_PORT)..."
	./cliente.exe $(CLIENT_PORT)

# Target para rodar servidor em background
start-server: server
	@echo "Iniciando servidor em background na porta $(SERVER_PORT)..."
	./servidor.exe $(SERVER_PORT) &
	@echo "Servidor iniciado. PID: $$!"

# Target para testar (roda servidor em background e depois o cliente)
test: server client
	@echo "Iniciando teste completo..."
	@echo "Iniciando servidor em background..."
	./servidor.exe $(SERVER_PORT) & \
	SERVER_PID=$$!; \
	sleep 2; \
	echo "Iniciando cliente..."; \
	./cliente.exe $(CLIENT_PORT); \
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
	@if [ -f ./servidor.exe ]; then \
		echo "✓ Servidor compilado"; \
	else \
		echo "✗ Servidor não encontrado"; \
	fi
	@if [ -f ./cliente.exe ]; then \
		echo "✓ Cliente compilado"; \
	else \
		echo "✗ Cliente não encontrado"; \
	fi

clean:	
	@echo "Limpando arquivos compilados..."
	rm -f ./servidor.exe ./cliente.exe
	@echo "Limpeza concluída."

# Target para matar processos do servidor (útil se ficou rodando)
kill-server:
	@echo "Procurando processos do servidor..."
	@pkill -f "servidor.exe" || echo "Nenhum processo do servidor encontrado"

.PHONY: all server client run-server run-client start-server test check help clean kill-server \
 	run-tests-client run-tests-client2 run-tests-server run-tests