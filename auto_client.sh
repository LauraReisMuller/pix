#!/bin/bash
# filepath: /home/max/sisop2/pix/auto_client.sh

# Configurações
TARGET_CLIENT="cliente-2"
MY_CLIENT="cliente-1"

# 1. Descobre o IP do destino
DEST_IP=$(docker inspect -f '{{range .NetworkSettings.Networks}}{{.IPAddress}}{{end}}' $TARGET_CLIENT)
echo "-> IP Destino: $DEST_IP"

# Função geradora de comandos
gerar_comandos() {
    # Lote 1
    for i in {1..40}; do
        echo "$DEST_IP 1"
    done

    # Pausa para você derrubar o servidor
    echo "!!! PAUSA - DERRUBE O SERVIDOR AGORA !!!" >&2
    for i in {5..1}; do
        echo -ne "$i...\r" >&2
        sleep 5
    done
    echo -e "\n-> Retomando envio..." >&2

    # Lote 2 (Pós-falha)
    for i in {1..60}; do
        echo "$DEST_IP 1"
    done
}

# AGORA FUNCIONA: Envia comandos para o processo PID 1 existente
# O --sig-proxy=false impede que Ctrl+C no script mate o container
gerar_comandos | docker attach --sig-proxy=false $MY_CLIENT