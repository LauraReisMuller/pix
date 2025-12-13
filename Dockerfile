# --- Estágio 1: Builder (Compilação) ---
FROM ubuntu:22.04 AS builder

# Instala compiladores e ferramentas
RUN apt-get update && apt-get install -y \
    build-essential \
    make \
    g++ \
    && rm -rf /var/lib/apt/lists/*

# Cria diretório de trabalho
WORKDIR /app

# Copia os arquivos necessários respeitando sua estrutura
# (Copiamos apenas o necessário para aproveitar o cache do Docker)
COPY Makefile .
COPY include/ ./include/
COPY src/ ./src/

# Compila o projeto
# O README diz para rodar make clean e make
RUN make clean && make

# --- Estágio 2: Runtime (Imagem Final Leve) ---
FROM ubuntu:22.04

WORKDIR /app

# Copia apenas os executáveis gerados
COPY --from=builder /app/servidor.exe .
COPY --from=builder /app/cliente.exe .

# Instala bibliotecas padrão (geralmente já vem, mas garante compatibilidade de threads)
RUN apt-get update && apt-get install -y libstdc++6 && rm -rf /var/lib/apt/lists/*
