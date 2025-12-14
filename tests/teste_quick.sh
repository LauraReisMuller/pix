#!/bin/bash

echo "🧪 Teste Rápido do Algoritmo Bully"
echo "=================================="

# Limpa processos antigos
killall servidor 2>/dev/null

# Limpa logs
rm -f test_logs/*.log

# Inicia 3 servidores
echo "▶️  Iniciando servidores..."
./build/servidor 1 4001 5001 &
PID1=$!
sleep 0.5

./build/servidor 2 4002 5002 &
PID2=$!
sleep 0.5

./build/servidor 3 4003 5003 &
PID3=$!

echo "✅ Servidores iniciados:"
echo "   Servidor 1 (PID $PID1)"
echo "   Servidor 2 (PID $PID2)"
echo "   Servidor 3 (PID $PID3) ← Deve ser LÍDER"
echo ""
echo "⏱️  Aguardando 5 segundos..."
sleep 5

echo ""
echo "🔪 Matando líder (Servidor 3)..."
kill $PID3
sleep 4

echo "✅ Servidor 2 deve ter assumido"
echo ""
echo "🔪 Matando novo líder (Servidor 2)..."
kill $PID2
sleep 4

echo "✅ Servidor 1 deve ter assumido"
echo ""
echo "🧹 Limpando..."
kill $PID1 2>/dev/null

echo ""
echo "📋 Logs salvos em test_logs/"
echo "   Execute: tail -f test_logs/server_*.log"