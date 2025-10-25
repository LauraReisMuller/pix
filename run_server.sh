#!/bin/bash
# filepath: /home/max/sisop2/pix/run_server.sh

# ================================================
# Script para o Servidor
# Execute este script na máquina do Servidor
# ================================================

PORT=4000
BUILD_DIR="build"

# Cores
GREEN='\033[0;32m'
BLUE='\033[0;34m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m'

echo -e "${BLUE}=================================================${NC}"
echo -e "${BLUE}   Servidor PIX - Porta $PORT${NC}"
echo -e "${BLUE}=================================================${NC}"

# Verifica se o servidor está compilado
if [ ! -f "$BUILD_DIR/server.exe" ]; then
    echo -e "${RED}ERRO: Servidor não compilado!${NC}"
    echo "Execute: make server"
    exit 1
fi

# Função de cleanup
cleanup() {
    echo -e "\n${YELLOW}Encerrando servidor...${NC}"
    pkill -f "server.exe" 2>/dev/null
    echo -e "${GREEN}✓ Servidor finalizado${NC}"
    exit 0
}

trap cleanup SIGINT SIGTERM

# Limpa processos antigos
pkill -f "server.exe" 2>/dev/null
sleep 1

echo -e "\n${GREEN}Iniciando servidor...${NC}"
echo -e "${YELLOW}Pressione Ctrl+C para encerrar${NC}\n"

# Executa o servidor
./$BUILD_DIR/server.exe $PORT

# Se o servidor terminar inesperadamente
echo -e "\n${RED}Servidor encerrou inesperadamente!${NC}"
exit 1