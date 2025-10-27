#!/bin/bash
# ================================================
# Script para Cliente 2 (10.0.0.5)
# EXECUÇÃO COM VARIAÇÃO DE VALORES
# Inclui requisições de consulta de saldo (valor 0)
# ================================================

CLIENT_IP="10.0.0.5"
DEST_IP="192.168.0.25"
PORT=4000
NUM_TRANSACTIONS=10
MIN_VALUE=1
MAX_VALUE=20
QUERY_PROB=0.2

# Cores
GREEN='\033[0;32m'
BLUE='\033[0;34m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m'

echo -e "${BLUE}=================================================${NC}"
echo -e "${BLUE}   Cliente 2 - $CLIENT_IP${NC}"
echo -e "${BLUE}   Enviando para: $DEST_IP (Valores: $MIN_VALUE-$MAX_VALUE)${NC}"
echo -e "${BLUE}=================================================${NC}"

if [ ! -f "cliente.exe" ]; then
    echo -e "${RED}ERRO: Cliente não compilado!${NC}"
    echo "Execute: make client"
    exit 1
fi

pkill -f "cliente.exe" 2>/dev/null

COMMANDS_FILE="/tmp/client2_commands.txt"
> $COMMANDS_FILE

echo -e "\n${BLUE}Preparando $NUM_TRANSACTIONS operações...${NC}"

for i in $(seq 1 $NUM_TRANSACTIONS); do
    if (( $(echo "$RANDOM/32767 < $QUERY_PROB" | bc -l) )); then
        # Consulta de saldo
        RAND_IP="10.0.0.$((RANDOM % 200 + 3))"
        echo "$RAND_IP 0" >> $COMMANDS_FILE
        echo -e "\r[${YELLOW}Consulta${NC}] $RAND_IP 0  ($i/$NUM_TRANSACTIONS)" >&2
    else
        # Transação com valor aleatório
        VALUE=$((RANDOM % (MAX_VALUE - MIN_VALUE + 1) + MIN_VALUE))
        echo "$DEST_IP $VALUE" >> $COMMANDS_FILE
        echo -e "\r[${GREEN}Transação${NC}] $DEST_IP $VALUE  ($i/$NUM_TRANSACTIONS)" >&2
    fi
done
echo "" >&2

echo -e "${GREEN}✓ Comandos preparados${NC}" >&2
echo -e "\n${YELLOW}Iniciando cliente...${NC}" >&2
echo -e "${YELLOW}(Saída do cliente abaixo)${NC}" >&2
echo -e "${BLUE}─────────────────────────────────────────────────${NC}\n" >&2

./cliente.exe $PORT < $COMMANDS_FILE

echo -e "\n${BLUE}─────────────────────────────────────────────────${NC}" >&2
echo -e "${BLUE}Encerrando...${NC}" >&2
rm -f $COMMANDS_FILE
echo -e "${GREEN}✓ Cliente 2 finalizado${NC}" >&2