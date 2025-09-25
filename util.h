#ifndef UTIL_H
#define UTIL_H

#include <time.h>
#include <stdio.h>

// Converte time_t para string formatada
void format_date(time_t t, char *buffer, int max);

void write_bytes(FILE *arc, FILE *doc);

#endif

