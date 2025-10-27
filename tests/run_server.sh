#!/bin/bash
# ================================================
# Script para Servidor PIX
# Inicia o servidor na porta configurada
# ================================================

PORT=4000

# Cores
GREEN='\033[0;32m'
BLUE='\033[0;34m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m'

echo -e "${BLUE}=================================================${NC}"
echo -e "${BLUE}   Servidor PIX${NC}"
echo -e "${BLUE}   Porta: $PORT${NC}"
echo -e "${BLUE}=================================================${NC}"

# Verifica se o servidor foi compilado
if [ ! -f "servidor.exe" ]; then
    echo -e "${RED}ERRO: Servidor não compilado!${NC}"
    echo "Execute: make server"
    exit 1
fi

# Mata processos anteriores do servidor
pkill -f "servidor.exe" 2>/dev/null
sleep 1

echo -e "\n${YELLOW}Iniciando servidor...${NC}"
echo -e "${YELLOW}Pressione Ctrl+C para encerrar${NC}"
echo -e "${BLUE}─────────────────────────────────────────────────${NC}\n"

# Executa o servidor
./servidor.exe $PORT

echo -e "\n${BLUE}─────────────────────────────────────────────────${NC}"
echo -e "${GREEN}✓ Servidor encerrado${NC}"