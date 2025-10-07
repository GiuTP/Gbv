#include "gbv.h"
#include "util.h"   // format_date
#include <stdio.h>  // rewind, fopen, fclose, fread, fwrite, fseek, ftell, printf, snprintf
#include <stdlib.h> // realloc
#include <string.h> // strcmp
#include <unistd.h> // fileno, ftruncate

// Estrutura do superbloco
typedef struct {
    long offset;
    int count;
} SBlock;

void write_bytes(FILE *arc, FILE *doc){
    char buffer[BUFFER_SIZE];
    long bytes_readed;

    while((bytes_readed = fread(buffer, 1, BUFFER_SIZE, doc)) > 0)
        fwrite(buffer, 1, bytes_readed, arc);
}

void print_bytes(FILE *arc, long lim_ceil, long offset){
    long bytes_reamaining, bytes_to_read;
    size_t bytes_readed;
    char buffer[BUFFER_SIZE];

    fseek(arc, offset, SEEK_SET);
    bytes_reamaining = lim_ceil - offset;
    bytes_to_read = (bytes_reamaining > BUFFER_SIZE) ? BUFFER_SIZE : bytes_reamaining;
    bytes_readed = fread(buffer, 1, bytes_to_read, arc);

    for(size_t i = 0; i < bytes_readed; i++){
        printf("%02x ", (unsigned char)buffer[i]);
        if ((i + 1) % 8 == 0)
            printf(" ");
        if ((i + 1) % 16 == 0)
            printf("\n");
    }

    printf("\n");
}

/* Retorna:
    *-1: erro ao criar o arquivo ou erro de leitura/escrita;
    * 0: criou com sucesso.          */
int gbv_create(const char *filename){
    FILE *arc = fopen(filename, "wb");

    if(!arc)
        return -1;
    
    SBlock super_bloco = {sizeof(SBlock), 0};
    if(fwrite(&super_bloco, sizeof(SBlock), 1, arc) != 1){
        fclose(arc);
        return -1;
    }

    fclose(arc);

    return 0;
}

/* Retorna
    *-2: erro ao criar a arquivo .gbv ou erro de leitura/escrita;
    *-1: erro de alocação do vetor de documentos;
    * 0: abriu com sucesso.                          */
int gbv_open(Library *lib, const char *filename){
    FILE *arc = fopen(filename, "r+b");

    if(!arc){
        if(gbv_create(filename) != 0)
            return -2;

        lib->count = 0;
        lib->docs = NULL;

        return 0;
    }

    SBlock super_bloco;
    // leitura do superbloco
    if(fread(&super_bloco, sizeof(SBlock), 1, arc) != 1){
        fclose(arc);
        return -1;
    }
    // leitura dos metadados
    if(super_bloco.count > 0){
        if(!(lib->docs = malloc(sizeof(Document) * super_bloco.count))){
            fclose(arc);
            return -1;
        }
    }
    else
        lib->docs = NULL;

    lib->count = super_bloco.count;
    fseek(arc, super_bloco.offset, SEEK_SET);
    if(fread(&super_bloco, sizeof(SBlock), 1, arc) != 1){
        fclose(arc);
        return -1;
    }

    fclose(arc);

    return 0;
}

/* Retorna:
    *-2: erro ao abri o arquivo ou documento ou erro de leitura/escrita;
    *-1: erro ao realocar memória do vetor;
    * 0: adicionou com sucesso                  */
int gbv_add(Library *lib, const char *archive, const char *docname){
    FILE *arc = fopen(archive, "r+b");
    FILE *doc = fopen(docname, "rb");
    char buffer[BUFFER_SIZE];

    if (!arc || !doc){
        if(arc)
            fclose(arc);
        if(doc)
            fclose(doc);

        return -2;
    }

    SBlock super_bloco;
    if(fread(&super_bloco, sizeof(SBlock), 1, arc) != 1){
        fclose(arc);
        fclose(doc);
        return -2;
    }

    // procura se arquivo já existe
    long index_doc_equal;
    for (index_doc_equal = 0; index_doc_equal < lib->count; index_doc_equal++){
        if(strcmp(docname, lib->docs[index_doc_equal].name) == 0)
            break;
    }

    // substituição em caso de mesmo nome
    if (index_doc_equal != lib->count){
        long doc_new_size, doc_old_size, doc_old_offset;
        doc_old_size = lib->docs[index_doc_equal].size;
        doc_old_offset = lib->docs[index_doc_equal].offset;

        fseek(doc, 0, SEEK_END);
        doc_new_size = ftell(doc);
        rewind(doc);

        // documento de mesmo tamanho
        if(doc_new_size == doc_old_size){
            fseek(arc, doc_old_offset, SEEK_SET);
            write_bytes(arc, doc);
        }
        else{
            long bytes_remaining, read_position, write_position;
            long differencial = doc_new_size - doc_old_size;
            long start_read_position = doc_old_offset + doc_old_size;
            long total_bytes_to_move = super_bloco.offset - start_read_position;

            // documento de tamanho maior
            if(doc_new_size > doc_old_size){; 
                bytes_remaining = total_bytes_to_move;
                
                // abre um espaço de tamanho "differencial"
                while(bytes_remaining > 0){
                    long size_to_move = bytes_remaining > BUFFER_SIZE ? BUFFER_SIZE : bytes_remaining;
                    read_position = start_read_position + bytes_remaining - size_to_move;
                    write_position = read_position + differencial;

                    // blocos subsequentes são deslocados para frente
                    fseek(arc, read_position, SEEK_SET);
                    if(fread(buffer, 1, size_to_move, arc) != size_to_move){
                        fclose(arc);
                        fclose(doc);
                        return -2;
                    }
                    fseek(arc, write_position, SEEK_SET);
                    if(fwrite(buffer, 1, size_to_move, arc) != size_to_move){
                        fclose(arc);
                        fclose(doc);
                        return -2;
                    }

                    bytes_remaining -= size_to_move;
                }

                fseek(arc, doc_old_offset, SEEK_SET);
                write_bytes(arc, doc);
            }
            // documento de tamanho menor
            else{
                fseek(arc, doc_old_offset, SEEK_SET);
                write_bytes(arc, doc);
                
                read_position = start_read_position;
                write_position = start_read_position + differencial;
                long bytes_moved = 0;

                // preenche o espaço deixado pelo documento antigo
                while(bytes_moved < total_bytes_to_move){
                    bytes_remaining = total_bytes_to_move - bytes_moved;
                    long size_to_move = bytes_remaining > BUFFER_SIZE ? BUFFER_SIZE : bytes_remaining;

                    read_position += bytes_moved;
                    write_position += bytes_moved;
                    fseek(arc, read_position, SEEK_SET);
                    if(fread(buffer, 1, size_to_move, arc) != size_to_move){
                        fclose(arc);
                        fclose(doc);
                        return -2;
                    }
                    fseek(arc, write_position, SEEK_SET);
                    if(fwrite(buffer, 1, size_to_move, arc) != size_to_move){
                        fclose(arc);
                        fclose(doc);
                        return -2;
                    }

                    bytes_moved += size_to_move;
                }

                int fd = fileno(arc);
                ftruncate(fd, super_bloco.offset + lib->count * sizeof(Document));
            }
            for(int i = index_doc_equal + 1; i < lib->count; i++)
                    lib->docs[i].offset += differencial;

            super_bloco.offset += differencial;
        }
        lib->docs[index_doc_equal].date = time(NULL);
        lib->docs[index_doc_equal].size = doc_new_size;
    }
    // adição em caso de novo arquivo
    else{
        fseek(arc, super_bloco.offset, SEEK_SET);
        write_bytes(arc, doc);
        
        Document *ptr_tmp;
        if(!(ptr_tmp = realloc(lib->docs, sizeof(Document) * (lib->count + 1)))){
            fclose(arc);
            fclose(doc);
            return -1;
        }
        lib->docs = ptr_tmp;

        // metadados do novo documento
        snprintf(lib->docs[lib->count].name, MAX_NAME, "%s", docname);
        lib->docs[lib->count].size = ftell(doc);
        lib->docs[lib->count].date = time(NULL);
        lib->docs[lib->count].offset = super_bloco.offset;
        lib->count++;

        super_bloco.count++;
        super_bloco.offset += lib->docs[lib->count - 1].size;
    }

    fseek(arc, super_bloco.offset, SEEK_SET);
    if(fwrite(lib->docs, sizeof(Document), lib->count, arc) != lib->count){
        fclose(arc);
        fclose(doc);
        return -2;
    }
    fseek(arc, 0, SEEK_SET);
    if(fwrite(&super_bloco.count, sizeof(int), 1, arc) != 1){
        fclose(arc);
        fclose(doc);
        return -2;
    }
    
    fclose(doc);
    fclose(arc);
    
    return 0;
}

/* Retorna:
    *-3: arquivo não encontrado;
    *-2: erro ao abrir o arquivo ou erro de leitura/escrita;
    *-1: erro ao realocar memória no vetor de documento;
    * 0: removeu com sucesso o documento.                   */
int gbv_remove(Library *lib, const char *archive, const char *docname){
    long index_removed;

    // procura se o arquivo existe
    for (index_removed = 0; index_removed < lib->count; index_removed++){
        if(strcmp(docname, lib->docs[index_removed].name) == 0)
            break;
    }

    if(index_removed == lib->count){
        printf("Arquivo %s nao existe na biblioteca\n", docname);
        return -3;
    }

    FILE *arc = fopen(archive, "r+b");

    if(!arc)
        return -2;

    SBlock super_bloco;
    fseek(arc, 0, SEEK_SET);
    if(fread(&super_bloco.count, sizeof(Document), 1, arc) != 1){
        fclose(arc);
        return -2;   
    }

    long doc_size = lib->docs[index_removed].size;
    long read_position = lib->docs[index_removed].offset + doc_size;
    long write_position = read_position - doc_size;
    long total_bytes_to_move = super_bloco.offset - read_position;
    long bytes_moved = 0;

    // preenche o espaço deixado pelo documento removido
    while (bytes_moved < total_bytes_to_move){
        char buffer[BUFFER_SIZE];

        long bytes_remaining = total_bytes_to_move - bytes_moved;
        long size_to_move = bytes_remaining > BUFFER_SIZE ? BUFFER_SIZE : bytes_remaining;

        read_position += bytes_moved;
        write_position += bytes_moved;
        fseek(arc, read_position, SEEK_SET);
        if(fread(buffer, 1, size_to_move, arc) != size_to_move){
            fclose(arc);
            return -2;
        }
        fseek(arc, write_position, SEEK_SET);
        if(fwrite(buffer, 1, size_to_move, arc) != size_to_move){
            fclose(arc);
            return -2;
        }

        bytes_moved += size_to_move;
    }

    // atualiza os metadados dos demais documentos
    for (long i = index_removed; i < (lib->count - 1); i++){
        lib->docs[i+1].offset -= doc_size;
        lib->docs[i] = lib->docs[i+1];
    }
    lib->count--;

    Document *tmp_ptr;
    if((lib->count > 0) && !(tmp_ptr = realloc(lib->docs, sizeof(Document) * lib->count))){
        fclose(arc);
        return -1;
    }
    lib->docs = tmp_ptr;    

    super_bloco.count = lib->count;
    super_bloco.offset -= doc_size;

    fseek(arc, 0, SEEK_SET);
    if(fwrite(&super_bloco, sizeof(SBlock), 1, arc) != 1){
        fclose(arc);
        return -2;
    }
    fseek(arc, super_bloco.offset, SEEK_SET);
    if(fwrite(lib->docs, sizeof(Document), lib->count, arc) != lib->count){
        fclose(arc);
        return -2;   
    }

    int fd = fileno(arc);
    ftruncate(fd, super_bloco.offset + sizeof(Document) * lib->count);

    return 0;
}

/* Retorna:
    *-1: erro na alocação do vetor de documentos;
    * 0: imprimiu os documentos com sucesso.        */
int gbv_list(const Library *lib){
    if(!lib->docs)
        return -1;
    
    for(int i = 0; i < lib->count; i++){
        char date_readable[50];
        format_date(lib->docs[i].date, date_readable, 50);

        printf("-------------------\n");
        printf("--- Documento %d ---\n", i + 1);
        printf(" %-9s %s\n", "Nome:", lib->docs[i].name);
        printf(" %-9s %ld bytes\n", "Tamanho:", lib->docs[i].size);
        printf(" %-9s %ld\n", "Offset:", lib->docs[i].offset);
        printf(" %-9s %s\n", "Data:", date_readable);
        printf("-------------------\n");
    }

    return 0;
}

/* Retorna:
    *-2: arquivo não encontrado;
    *-1: erro ao abrir o arquivo;
    * 0: navegou com sucesso        */
int gbv_view(const Library *lib, const char *archive, const char *docname){
    int index_view;
    for (index_view = 0; index_view < lib->count; index_view++){
        if(strcmp(docname, lib->docs[index_view].name) == 0)
            break;
    }

    if(index_view == lib->count)
        return -2;

    FILE *arc = fopen(archive, "r+b");
    if(!arc)
        return -1;

    long lim_floor, current_offset;
    lim_floor = current_offset = lib->docs[index_view].offset;
    long lim_ceil = lib->docs[index_view].offset + lib->docs[index_view].size;
    
    print_bytes(arc, lim_ceil, current_offset);

    int keep_reading = 1;
    while(keep_reading){
        char op;
        scanf(" %c", &op);
        switch (op){
        case 'n':
            if(current_offset + BUFFER_SIZE < lim_ceil){
                current_offset += BUFFER_SIZE;
                print_bytes(arc, lim_ceil, current_offset);
            }
            break;
        case 'p':
            if(current_offset - BUFFER_SIZE >= lim_floor){
                current_offset -= BUFFER_SIZE;
                print_bytes(arc, lim_ceil, current_offset);
            }
            break;
        case 'q':
            keep_reading = 0;
            break;
        default:
            printf("Opcao invalida\n");
            break;
        }
    }

    fclose(arc);

    return 0;
}