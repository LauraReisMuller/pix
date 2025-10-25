#!/bin/bash
# filepath: /home/max/sisop2/pix/run_client1.sh

# ================================================
# Script para Cliente 1 (143.54.55.13)
# Execute este script na máquina do Cliente 1
# ================================================

CLIENT_IP="192.168.0.25"
DEST_IP="10.0.0.2"
PORT=4000
NUM_TRANSACTIONS=50
BUILD_DIR="build"

# Cores
GREEN='\033[0;32m'
BLUE='\033[0;34m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m'

echo -e "${BLUE}=================================================${NC}"
echo -e "${BLUE}   Cliente 1 - $CLIENT_IP${NC}"
echo -e "${BLUE}   Enviando para: $DEST_IP${NC}"
echo -e "${BLUE}=================================================${NC}"

# Verifica se o cliente está compilado
if [ ! -f "$BUILD_DIR/client.exe" ]; then
    echo -e "${RED}ERRO: Cliente não compilado!${NC}"
    echo "Execute: make client"
    exit 1
fi

# Limpa processos antigos
pkill -f "client.exe" 2>/dev/null

# Cria arquivo de comandos
echo -e "\n${BLUE}Preparando $NUM_TRANSACTIONS transações...${NC}"
COMMANDS_FILE="/tmp/client1_commands.txt"
> $COMMANDS_FILE

for i in $(seq 1 $NUM_TRANSACTIONS); do
    # Valores aleatórios entre 1 e 50
    VALUE=$((RANDOM % 50 + 1))
    echo "$DEST_IP $VALUE" >> $COMMANDS_FILE
    echo -ne "\rComandos preparados: $i/$NUM_TRANSACTIONS"
done
echo ""

echo -e "${GREEN}✓ Comandos preparados${NC}"
echo -e "\n${YELLOW}Iniciando cliente...${NC}"
echo -e "${YELLOW}Pressione Ctrl+C para encerrar${NC}\n"

# Executa o cliente
./$BUILD_DIR/client.exe $PORT < $COMMANDS_FILE

# Cleanup
echo -e "\n${BLUE}Encerrando...${NC}"
rm -f $COMMANDS_FILE
echo -e "${GREEN}✓ Cliente 1 finalizado${NC}"