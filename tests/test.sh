#!/bin/bash
# ================================================
# Script de Teste Completo
# Inicia servidor e múltiplos clientes
# ================================================

PORT=4000
SERVER_STARTUP_DELAY=2
CLIENT_DELAY=1

# Cores
GREEN='\033[0;32m'
BLUE='\033[0;34m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
NC='\033[0m'

echo -e "${CYAN}=================================================${NC}"
echo -e "${CYAN}   Teste Completo do Sistema PIX${NC}"
echo -e "${CYAN}=================================================${NC}"

# Verifica compilação
if [ ! -f "servidor.exe" ] || [ ! -f "cliente.exe" ]; then
    echo -e "${RED}ERRO: Executáveis não encontrados!${NC}"
    echo "Execute: make"
    exit 1
fi

# Limpa processos anteriores
echo -e "\n${YELLOW}Limpando processos anteriores...${NC}"
pkill -f "servidor.exe" 2>/dev/null
pkill -f "cliente.exe" 2>/dev/null
sleep 1

# Inicia o servidor em background
echo -e "${BLUE}Iniciando servidor na porta $PORT...${NC}"
./servidor.exe $PORT > server_output.log 2>&1 &
SERVER_PID=$!
sleep $SERVER_STARTUP_DELAY

if ! kill -0 $SERVER_PID 2>/dev/null; then
    echo -e "${RED}ERRO: Servidor falhou ao iniciar!${NC}"
    cat server_output.log
    exit 1
fi

echo -e "${GREEN}✓ Servidor iniciado (PID: $SERVER_PID)${NC}"

# Função para cleanup
cleanup() {
    echo -e "\n${YELLOW}Encerrando processos...${NC}"
    pkill -f "servidor.exe" 2>/dev/null
    pkill -f "cliente.exe" 2>/dev/null
    wait $SERVER_PID 2>/dev/null
    echo -e "${GREEN}✓ Teste finalizado${NC}"
    exit 0
}

trap cleanup SIGINT SIGTERM

# Inicia Cliente 1
echo -e "\n${BLUE}Iniciando Cliente 1...${NC}"
./tests/run_client.sh > client1_output.log 2>&1 &
CLIENT1_PID=$!
sleep $CLIENT_DELAY

# Inicia Cliente 2
echo -e "${BLUE}Iniciando Cliente 2...${NC}"
./tests/run_client2.sh > client2_output.log 2>&1 &
CLIENT2_PID=$!

# Aguarda clientes finalizarem
echo -e "\n${YELLOW}Aguardando clientes finalizarem...${NC}"
wait $CLIENT1_PID
wait $CLIENT2_PID

echo -e "\n${GREEN}✓ Clientes finalizados${NC}"

# Exibe resumo
echo -e "\n${CYAN}=================================================${NC}"
echo -e "${CYAN}   Resumo dos Logs${NC}"
echo -e "${CYAN}=================================================${NC}"

echo -e "\n${BLUE}--- Servidor ---${NC}"
tail -20 server_output.log

echo -e "\n${BLUE}--- Cliente 1 ---${NC}"
tail -10 client1_output.log

echo -e "\n${BLUE}--- Cliente 2 ---${NC}"
tail -10 client2_output.log

cleanup