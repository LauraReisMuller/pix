bin/bash
# ================================================
# Script para Cliente 1 (192.168.0.25)
# EXECUÇÃO DETERMINÍSTICA (VALOR FIXO)
# Inclui requisições de consulta de saldo (valor 0)
# ================================================

CLIENT_IP="192.168.0.25"
DEST_IP="10.0.0.2"
PORT=4000
NUM_TRANSACTIONS=10
FIXED_VALUE=5
QUERY_PROB=0.2      # 20% das requisições serão consultas
BUILD_DIR="build"

# Cores
GREEN='\033[0;32m'
BLUE='\033[0;34m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m'

echo -e "${BLUE}=================================================${NC}"
echo -e "${BLUE}   Cliente 1 - $CLIENT_IP${NC}"
echo -e "${BLUE}   Enviando para: $DEST_IP (Valor Fixo: $FIXED_VALUE)${NC}"
echo -e "${BLUE}=================================================${NC}"

if [ ! -f "$BUILD_DIR/client.exe" ]; then
    echo -e "${RED}ERRO: Cliente não compilado!${NC}"
    echo "Execute: make client"
    exit 1
fi

pkill -f "client.exe" 2>/dev/null

COMMANDS_FILE="/tmp/client1_commands.txt"
> $COMMANDS_FILE

echo -e "\n${BLUE}Preparando $NUM_TRANSACTIONS operações...${NC}"

for i in $(seq 1 $NUM_TRANSACTIONS); do
    if (( $(echo "$RANDOM/32767 < $QUERY_PROB" | bc -l) )); then
        # 🧾 Consulta de saldo
        RAND_IP="10.0.0.$((RANDOM % 200 + 3))"
        echo "$RAND_IP 0" >> $COMMANDS_FILE
        echo -e "\r[${YELLOW}Consulta${NC}] $RAND_IP 0  ($i/$NUM_TRANSACTIONS)"
    else
        # 💸 Transação normal
        echo "$DEST_IP $FIXED_VALUE" >> $COMMANDS_FILE
        echo -e "\r[${GREEN}Transação${NC}] $DEST_IP $FIXED_VALUE  ($i/$NUM_TRANSACTIONS)"
    fi
done
echo ""

echo -e "${GREEN}✓ Comandos preparados${NC}"
echo -e "\n${YELLOW}Iniciando cliente...${NC}"
echo -e "${YELLOW}Pressione Ctrl+C para encerrar${NC}\n"

./$BUILD_DIR/client.exe $PORT < $COMMANDS_FILE

echo -e "\n${BLUE}Encerrando...${NC}"
rm -f $COMMANDS_FILE
echo -e "${GREEN}✓ Cliente 1 finalizado${NC}"
