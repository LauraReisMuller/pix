FROM gcc:latest

# Instala utilitários básicos (opcional, útil para debug)
RUN apt-get update && apt-get install -y net-tools iputils-ping

WORKDIR /app

# Copia todo o código fonte para o container
COPY . .

# Compila o projeto
RUN make

# O comando de entrada será definido no docker-compose, mas deixamos um padrão aqui
#CMD ["./servidor", "1", "4001", "5001"]

# Mantém o container rodando para podermos entrar nele
CMD ["tail", "-f", "/dev/null"]