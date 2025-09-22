#include "gbv.h"
#include "util.h"
#include <stdio.h>
#include <stdlib.h>

typedef struct {
    long offset;
    int count;
} SBlock;

int gbv_create(const char *filename){

    FILE *arc = fopen(filename, "wb");
    if(!arc)
        return -1;
    
    SBlock super_bloco = {sizeof(SBlock), 0};
    if(fwrite(&super_bloco, sizeof(SBlock), 1, arc) != 1)
        return -1;

    fclose(arc);

    return 0;
}

int gbv_open(Library *lib, const char *filename){
    FILE *arc = fopen(filename, "rb");

    // arquivo é criado se não existir com gbv_create
    if (!arc){
        if(gbv_create(filename) != 0)
            return -2;

        lib->count = 0;
        lib->docs = NULL;

        return 0;
    }

    SBlock super_bloco;
    // dados iniciais do arquivo são lidos e feita alocações se existir
    if(fread(&super_bloco, sizeof(SBlock), 1, arc) != 1)
        return -1;
    
    if(super_bloco.count > 0){
        if(!(lib->docs = malloc(sizeof(Document) * super_bloco.count)))
            return -1;
    }
    else
        lib->docs = NULL;

    lib->count = super_bloco.count;
    fseek(arc, super_bloco.offset, SEEK_SET);
    if(fread(lib->docs, sizeof(Document), lib->count, arc) != lib->count)
        return -1;

    fclose(arc);
    
    return 0;
}

int gbv_add(Library *lib, const char *archive, const char *docname){
    

    return 0;
}

int gbv_remove(Library *lib, const char *docname){
    return 0;
}

int gbv_list(const Library *lib){

    for(int i = 0; i < lib->count; i++){
        char date_readable[50];
        format_date(lib->docs[i].date, date_readable, 50);
        printf("Nome: %s | Tamanho: %ld bytes | Data: %s | Offset: %ld\n", 
                lib->docs[i].name, 
                lib->docs[i].size, 
                date_readable, 
                lib->docs[i].offset);
    }

    return 0;
}

// Função tá estranha, como será aberto o arquivo sem o nome?
int gbv_view(const Library *lib, const char *docname){
    FILE *arc = fopen(docname, "rb");
    if (!arc)
        return -1;
    
    
    
    return 0;
}

int gbv_order(Library *lib, const char *archive, const char *criteria){
    return 0;
}