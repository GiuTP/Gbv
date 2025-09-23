#include "gbv.h"
#include "util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
    FILE *arc = fopen(archive, "r+b");
    FILE *doc = fopen(docname, "rb");
    char buffer[BUFFER_SIZE];
    size_t bytes_readed;

    if (!arc || !doc)
        return -2;
    
    // procura onde o novo arquivo sera inserido
    SBlock super_bloco;
    fread(&super_bloco, sizeof(SBlock), 1, arc);
    fseek(arc, super_bloco.offset, SEEK_SET);
    
    // armeza o arquivo no final da area de documento usando buffer
    while ((bytes_readed = fread(buffer, 1, BUFFER_SIZE, doc)) > 0)
        fwrite(buffer, 1, bytes_readed, arc);
    
    // aumenta o vetor de metadados
    Document *ptr_tmp;
    if(!(ptr_tmp= realloc(lib->docs, sizeof(Document) * (lib->count + 1))))
        return -1;
    
    lib->docs = ptr_tmp;
    // adiciona os metadados do novo documento ao vetor (RAM)
    snprintf(lib->docs[lib->count].name, MAX_NAME, "%s", docname);
    lib->docs[lib->count].date = time(NULL);
    lib->docs[lib->count].size = ftell(doc);
    lib->docs[lib->count].offset = super_bloco.offset;
    lib->count++;

    // atualiza os metadados dos arquivos em disco
    fseek(arc, super_bloco.offset + lib->docs[lib->count - 1].size, SEEK_SET);
    fwrite(lib->docs, sizeof(Document), lib->count, arc);

    // atualiza o super bloco em disco
    super_bloco.count++;
    super_bloco.offset += lib->docs[lib->count - 1].size;
    fseek(arc, 0, SEEK_SET);
    fwrite(&super_bloco, sizeof(SBlock), 1, arc);
    
    fclose(doc);
    fclose(arc);
    
    return 0;
}

// Mesma situacao do view, como sera salvo o arquivo sem o nome em questao
int gbv_remove(Library *lib, const char *docname){
    long index_removed = -1;
    // verifica se o docname existe na biblioteca
    for (long i = 0; i < lib->count; i++){
        if(strcmp(docname, lib->docs[i].name) == 0){
            index_removed = i;
            break;
        }
    }

    // remove o elemento se existir
    if (index_removed >= 0){
        for (long i = index_removed; i < (lib->count - 1); i++)
            lib->docs[i] = lib->docs[i+1];
    
        lib->count--;
        Document *tmp_ptr;
        if(!(tmp_ptr = realloc(lib->docs, sizeof(Document) * lib->count)))
            return -2;

        // escreve no disco com o fwrite, mas falta o nome...
        // atualiza o superbloco tbm, mas falta o nome tbm...
        
    }

    // nao encontrado
    return -1;
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