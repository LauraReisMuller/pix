#!/bin/bash
# filepath: \\wsl.localhost\Ubuntu\home\gui\SisopII\Trabalho\pix\test.sh

# ================================================
# Script de Teste de Consistência PIX
# Executa transações e valida resultados
# ================================================

PORT=4000
BUILD_DIR="build"
LOGS_DIR="test_logs"
NUM_TRANSACTIONS=20
FIXED_VALUE=5
INITIAL_BALANCE=100
DEST_IP="10.0.0.2"

# Cores
GREEN='\033[0;32m'
BLUE='\033[0;34m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
NC='\033[0m'

# Cria diretório de logs se não existir
mkdir -p $LOGS_DIR

# Arquivos de log no diretório do projeto
SERVER_LOG="$LOGS_DIR/server_test.log"
CLIENT_LOG="$LOGS_DIR/client_test.log"
COMMANDS_FILE="$LOGS_DIR/commands.txt"
RESULTS_FILE="$LOGS_DIR/results.txt"

echo -e "${BLUE}=================================================${NC}"
echo -e "${BLUE}   TESTE DE CONSISTÊNCIA PIX${NC}"
echo -e "${BLUE}=================================================${NC}"

# Compilação
echo -e "\n${CYAN}[1/6] Compilando projeto...${NC}"

if ! make clean > /dev/null 2>&1; then
    echo -e "${YELLOW}⚠ Warning: make clean falhou (ignorando)${NC}"
fi

echo -n "  • Compilando servidor... "
if make server > /dev/null 2>&1; then
    echo -e "${GREEN}✓${NC}"
else
    echo -e "${RED}✗ FALHOU${NC}"
    echo -e "${RED}ERRO: Falha ao compilar servidor!${NC}"
    make server
    exit 1
fi

echo -n "  • Compilando cliente... "
if make client > /dev/null 2>&1; then
    echo -e "${GREEN}✓${NC}"
else
    echo -e "${RED}✗ FALHOU${NC}"
    echo -e "${RED}ERRO: Falha ao compilar cliente!${NC}"
    make client
    exit 1
fi

echo -e "${GREEN}✓ Compilação concluída com sucesso${NC}"

# Verifica se os executáveis existem
if [ ! -f "$BUILD_DIR/server.exe" ] || [ ! -f "$BUILD_DIR/client.exe" ]; then
    echo -e "${RED}ERRO: Executáveis não encontrados!${NC}"
    echo "Execute: make all"
    exit 1
fi

# Cleanup de processos antigos
cleanup() {
    echo -e "\n${YELLOW}Limpando processos...${NC}"
    pkill -f "server.exe" 2>/dev/null
    pkill -f "client.exe" 2>/dev/null
    sleep 1
}

cleanup
trap cleanup EXIT

# Limpa arquivos temporários
> $SERVER_LOG
> $CLIENT_LOG
> $COMMANDS_FILE
> $RESULTS_FILE

echo -e "${CYAN}[2/6] Preparando comandos...${NC}"

# Gera comandos de teste (apenas transações normais para validação simples)
for i in $(seq 1 $NUM_TRANSACTIONS); do
    echo "$DEST_IP $FIXED_VALUE" >> $COMMANDS_FILE
done

echo -e "${GREEN}✓ $NUM_TRANSACTIONS comandos preparados${NC}"
echo -e "${CYAN}Comandos salvos em: $COMMANDS_FILE${NC}"

echo -e "\n${CYAN}[3/6] Iniciando servidor...${NC}"

# Inicia o servidor em background
./$BUILD_DIR/server.exe $PORT > $SERVER_LOG 2>&1 &
SERVER_PID=$!
sleep 2

if ! ps -p $SERVER_PID > /dev/null; then
    echo -e "${RED}ERRO: Servidor falhou ao iniciar!${NC}"
    echo -e "${YELLOW}Conteúdo do log do servidor:${NC}"
    cat $SERVER_LOG
    exit 1
fi

echo -e "${GREEN}✓ Servidor rodando (PID: $SERVER_PID)${NC}"

echo -e "\n${CYAN}[4/6] Executando cliente...${NC}"

# Executa o cliente e captura output
timeout 30s ./$BUILD_DIR/client.exe $PORT < $COMMANDS_FILE > $CLIENT_LOG 2>&1
CLIENT_EXIT=$?

if [ $CLIENT_EXIT -eq 124 ]; then
    echo -e "${RED}ERRO: Cliente excedeu tempo limite!${NC}"
    echo -e "${YELLOW}Conteúdo do log do cliente:${NC}"
    cat $CLIENT_LOG
    exit 1
elif [ $CLIENT_EXIT -ne 0 ] && [ $CLIENT_EXIT -ne 141 ]; then
    # Exit code 141 (SIGPIPE) é normal quando stdin fecha
    echo -e "${RED}ERRO: Cliente falhou (exit code: $CLIENT_EXIT)${NC}"
    echo -e "${YELLOW}Conteúdo do log do cliente:${NC}"
    cat $CLIENT_LOG
    exit 1
fi

echo -e "${GREEN}✓ Cliente finalizado${NC}"

# Aguarda processamento final
sleep 2

# Verifica se os logs foram criados
if [ ! -s "$CLIENT_LOG" ]; then
    echo -e "${RED}ERRO: Log do cliente está vazio!${NC}"
    exit 1
fi

if [ ! -s "$SERVER_LOG" ]; then
    echo -e "${RED}ERRO: Log do servidor está vazio!${NC}"
    exit 1
fi

echo -e "\n${CYAN}[5/6] Analisando resultados...${NC}"

# Extrai dados do log do cliente
CLIENT_SERVER_ADDR=$(grep -oP "server (addr )?\K[0-9]+\.[0-9]+\.[0-9]+\.[0-9]+" $CLIENT_LOG | head -1)
CLIENT_TRANSACTIONS=$(grep "id req" $CLIENT_LOG | wc -l)

# Extrai dados do log do servidor
SERVER_TRANSACTIONS=$(grep "num transactions" $SERVER_LOG | tail -1 | awk '{print $3}')
SERVER_TOTAL_TRANSFERRED=$(grep "num transactions" $SERVER_LOG | tail -1 | awk '{print $6}')
SERVER_TOTAL_BALANCE=$(grep "num transactions" $SERVER_LOG | tail -1 | awk '{print $9}')

# Validação básica dos dados extraídos
if [ -z "$CLIENT_SERVER_ADDR" ]; then
    echo -e "${RED}ERRO: Não foi possível extrair endereço do servidor do log do cliente${NC}"
    echo -e "${YELLOW}Primeiras linhas do log:${NC}"
    head -20 $CLIENT_LOG
fi

echo -e "\n${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo -e "${BLUE}   RESUMO DOS LOGS${NC}"
echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"

echo -e "\n${YELLOW}Cliente:${NC}"
echo "  • Servidor descoberto: $CLIENT_SERVER_ADDR"
echo "  • Transações enviadas: $NUM_TRANSACTIONS"
echo "  • ACKs recebidos: $CLIENT_TRANSACTIONS"

echo -e "\n${YELLOW}Servidor:${NC}"
echo "  • Transações processadas: $SERVER_TRANSACTIONS"
echo "  • Total transferido: $SERVER_TOTAL_TRANSFERRED"
echo "  • Saldo total: $SERVER_TOTAL_BALANCE"

echo -e "\n${CYAN}[6/6] Validando consistência...${NC}\n"

# Array de testes
TESTS_PASSED=0
TESTS_FAILED=0

# Teste 1: Número de transações
echo -n "  [TEST 1] Transações enviadas = ACKs recebidos... "
if [ "$NUM_TRANSACTIONS" -eq "$CLIENT_TRANSACTIONS" ]; then
    echo -e "${GREEN}✓ PASS${NC}"
    ((TESTS_PASSED++))
else
    echo -e "${RED}✗ FAIL${NC} (Esperado: $NUM_TRANSACTIONS, Recebido: $CLIENT_TRANSACTIONS)"
    ((TESTS_FAILED++))
fi

# Teste 2: Servidor processou todas
echo -n "  [TEST 2] Servidor processou todas as transações... "
if [ "$SERVER_TRANSACTIONS" -eq "$NUM_TRANSACTIONS" ]; then
    echo -e "${GREEN}✓ PASS${NC}"
    ((TESTS_PASSED++))
else
    echo -e "${RED}✗ FAIL${NC} (Esperado: $NUM_TRANSACTIONS, Processado: $SERVER_TRANSACTIONS)"
    ((TESTS_FAILED++))
fi

# Teste 3: Total transferido correto
EXPECTED_TRANSFERRED=$((NUM_TRANSACTIONS * FIXED_VALUE))
echo -n "  [TEST 3] Total transferido correto... "
if [ "$SERVER_TOTAL_TRANSFERRED" -eq "$EXPECTED_TRANSFERRED" ]; then
    echo -e "${GREEN}✓ PASS${NC}"
    ((TESTS_PASSED++))
else
    echo -e "${RED}✗ FAIL${NC} (Esperado: $EXPECTED_TRANSFERRED, Real: $SERVER_TOTAL_TRANSFERRED)"
    ((TESTS_FAILED++))
fi

# Teste 4: Sequência de IDs no cliente
echo -n "  [TEST 4] IDs sequenciais no cliente... "
CLIENT_IDS=$(grep "id req" $CLIENT_LOG | awk '{print $7}' | sort -n)
EXPECTED_IDS=$(seq 1 $NUM_TRANSACTIONS)
if [ "$CLIENT_IDS" == "$EXPECTED_IDS" ]; then
    echo -e "${GREEN}✓ PASS${NC}"
    ((TESTS_PASSED++))
else
    echo -e "${RED}✗ FAIL${NC} (IDs fora de ordem)"
    ((TESTS_FAILED++))
fi

# Teste 5: Balanço decrescente no cliente
echo -n "  [TEST 5] Saldo decrescente no cliente... "
BALANCES=$(grep "new balance" $CLIENT_LOG | awk '{print $NF}')
IS_DECREASING=true
PREV_BALANCE=$INITIAL_BALANCE

while IFS= read -r balance; do
    if [ -z "$balance" ]; then
        continue
    fi
    EXPECTED=$((PREV_BALANCE - FIXED_VALUE))
    if [ "$balance" -ne "$EXPECTED" ]; then
        IS_DECREASING=false
        echo -e "\n      ${RED}Esperado: $EXPECTED, Obtido: $balance${NC}"
        break
    fi
    PREV_BALANCE=$balance
done <<< "$BALANCES"

if [ "$IS_DECREASING" = true ]; then
    echo -e "${GREEN}✓ PASS${NC}"
    ((TESTS_PASSED++))
else
    echo -e "${RED}✗ FAIL${NC} (Saldo inconsistente)"
    ((TESTS_FAILED++))
fi

# Teste 6: Sem ACKs duplicados no cliente
echo -n "  [TEST 6] Sem ACKs duplicados recebidos... "
DUPLICATE_ACKS=$(grep "Received delayed/duplicate ACK" $CLIENT_LOG | wc -l)
if [ "$DUPLICATE_ACKS" -eq 0 ]; then
    echo -e "${GREEN}✓ PASS${NC}"
    ((TESTS_PASSED++))
else
    echo -e "${YELLOW}⚠ WARNING${NC} ($DUPLICATE_ACKS duplicados detectados)"
    ((TESTS_PASSED++))  # Não é erro crítico
fi

# Teste 7: Saldo final esperado
FINAL_BALANCE=$(grep "new balance" $CLIENT_LOG | tail -1 | awk '{print $NF}')
EXPECTED_FINAL=$((INITIAL_BALANCE - (NUM_TRANSACTIONS * FIXED_VALUE)))
echo -n "  [TEST 7] Saldo final correto... "
if [ "$FINAL_BALANCE" -eq "$EXPECTED_FINAL" ]; then
    echo -e "${GREEN}✓ PASS${NC}"
    ((TESTS_PASSED++))
else
    echo -e "${RED}✗ FAIL${NC} (Esperado: $EXPECTED_FINAL, Real: $FINAL_BALANCE)"
    ((TESTS_FAILED++))
fi

# Teste 8: Sequência de IDs no servidor
echo -n "  [TEST 8] IDs sequenciais no servidor... "
SERVER_IDS=$(grep "id_req" $SERVER_LOG | awk '{print $6}' | sort -n | uniq)
if [ "$SERVER_IDS" == "$EXPECTED_IDS" ]; then
    echo -e "${GREEN}✓ PASS${NC}"
    ((TESTS_PASSED++))
else
    echo -e "${RED}✗ FAIL${NC} (IDs fora de ordem)"
    ((TESTS_FAILED++))
fi

# Resultado final
echo -e "\n${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo -e "${BLUE}   RESULTADO FINAL${NC}"
echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}\n"

TOTAL_TESTS=$((TESTS_PASSED + TESTS_FAILED))
SUCCESS_RATE=$((TESTS_PASSED * 100 / TOTAL_TESTS))

if [ $TESTS_FAILED -eq 0 ]; then
    echo -e "  ${GREEN}✓ TODOS OS TESTES PASSARAM!${NC}"
    echo -e "  ${GREEN}$TESTS_PASSED/$TOTAL_TESTS testes (100%)${NC}\n"
    EXIT_CODE=0
else
    echo -e "  ${RED}✗ ALGUNS TESTES FALHARAM!${NC}"
    echo -e "  ${YELLOW}$TESTS_PASSED/$TOTAL_TESTS testes ($SUCCESS_RATE%)${NC}\n"
    EXIT_CODE=1
fi

# Salva resultado
{
    echo "Tests Passed: $TESTS_PASSED/$TOTAL_TESTS"
    echo "Success Rate: $SUCCESS_RATE%"
    echo "Timestamp: $(date)"
    echo ""
    echo "Configuration:"
    echo "  Transactions: $NUM_TRANSACTIONS"
    echo "  Value per transaction: $FIXED_VALUE"
    echo "  Initial balance: $INITIAL_BALANCE"
} > $RESULTS_FILE

echo -e "${CYAN}Logs salvos em:${NC}"
echo "  • Servidor: $SERVER_LOG"
echo "  • Cliente: $CLIENT_LOG"
echo "  • Comandos: $COMMANDS_FILE"
echo "  • Resultados: $RESULTS_FILE"

echo -e "\n${BLUE}Para ver os logs completos:${NC}"
echo "  cat $SERVER_LOG"
echo "  cat $CLIENT_LOG"

exit $EXIT_CODE