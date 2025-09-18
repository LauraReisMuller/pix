// Funções utilitárias (timestamp, formatação de logs)
#include "utils.h"
#include <stdio.h>
#include <time.h>

void print_timestamp() {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    printf("[%02d:%02d:%02d] ", t->tm_hour, t->tm_min, t->tm_sec);
}

void log_message(const char *msg) {
    print_timestamp();
    printf("%s\n", msg);
}
