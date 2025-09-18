sequenceDiagram
    participant Cliente
    participant Servidor
    participant Tabela as "Tabela de clientes/histórico"

    %% FASE DE DESCOBERTA
    Cliente->>+Servidor: DESCOBERTA (broadcast)
    Servidor-->>-Cliente: DESC_ACK (saldo inicial, confirma endereço)
    note right of Cliente: Cliente agora sabe o IP do servidor

    %% FASE DE PROCESSAMENTO - transação normal
    loop Para cada requisição
        Cliente->>+Servidor: REQ(id=1, dest=10.1.1.3, valor=50)
        note right of Servidor: Thread criada para processar
        Servidor->>Tabela: verifica saldo e atualiza contas/histórico
        Tabela-->>Servidor: confirmação de atualização
        Servidor-->>-Cliente: REQ_ACK(id=1, novo saldo=50)
    end

    %% FASE DE TIMEOUT + RETRANSMISSÃO
    Cliente->>+Servidor: REQ(id=2, dest=10.1.1.4, valor=30)
    note right of Cliente: Timeout (sem resposta)
    Cliente->>+Servidor: REQ(id=2, retransmissão)
    Servidor->>Tabela: processa requisição
    Tabela-->>Servidor: confirmação de atualização
    Servidor-->>-Cliente: REQ_ACK(id=2, novo saldo=20)

    %% FASE DE DUPLICATA OU ID FORA DE ORDEM
    Cliente->>+Servidor: REQ(id=2, duplicado)
    Servidor-->>-Cliente: DUP!! Ack último id processado (id=2)

    %% Sincronização e threads (visão conceitual)
    note over Servidor,Tabela: Escrita bloqueante (mutex) para atualizar histórico/saldo
    note over Cliente,Servidor: Send/receive bloqueante para garantir entrega e ordem