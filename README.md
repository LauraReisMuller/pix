# Pix Project

## Estrutura
Este projeto implementa um sistema cliente-servidor para simulação de operações Pix, com threads para descoberta, processamento e interface.

### Ideia principal

- **Um servidor** central e **vários clientes** conectados via rede.
- A comunicação é feita em **UDP** (User Datagram Protocol).
- O servidor processa as requisições de forma **concorrente com threads**.

### Funcionalidades principais

1. **Descoberta**
    - O cliente envia uma mensagem em broadcast para encontrar o servidor.
    - O servidor responde e registra o cliente, atribuindo um saldo inicial.

2. **Processamento de transações**
    - O cliente envia requisições no formato: *ID da transação, IP de destino e valor*.
    - O servidor verifica saldo, atualiza valores das contas, histórico e saldo total.
    - Uma resposta (ack) é enviada ao cliente confirmando ou negando a operação.
    - Caso a resposta não chegue, o cliente deve reenviar a requisição.
    
3. **Interface e consistência**
    - O servidor deve exibir logs com informações de cada operação.
    - O cliente deve mostrar o resultado de suas requisições (saldo atualizado, etc.).
    - Deve haver **controle de concorrência** para manter os saldos e histórico consistentes.


- `src/common/`: Definições comuns (protocolos, utilitários)
- `src/server/`: Código do servidor (main, descoberta, processamento, interface)
- `src/client/`: Código do cliente (main, descoberta, request, interface)

## Compilação

Use o Makefile para compilar:

```
make clean
make
```

Os executáveis serão gerados em `build/`.

## Execução

- Para rodar o servidor: `build/server.exe`
- Para rodar o cliente: `build/client.exe`

## Dependências
- g++
- pthread (para threads)

## Organização sugerida

```
include/
  common/
    protocol.h
    utils.h
  server/
    discovery.h
    processing.h
    interface.h
  client/
    discovery.h
    request.h
    interface.h
src/
  common/
    utils.cpp
  server/
    main.cpp
    discovery.cpp
    processing.cpp
    interface.cpp
  client/
    main.cpp
    discovery.cpp
    request.cpp
    interface.cpp
Makefile
README.md
```
