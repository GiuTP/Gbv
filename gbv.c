#include "gbv.h"
#include "util.h"   // format_date
#include <stdio.h>  // fopen, fclose, fread, fwrite, fseek, ftell, printf
#include <stdlib.h> // realloc
#include <string.h> // strcmp
#include <unistd.h> // fileno, ftruncate

// Estrutura do superbloco
typedef struct {
    long offset;
    int count;
} SBlock;

static FILE *lib_arc = NULL;

void write_bytes(FILE *arc, FILE *doc){
    char buffer[BUFFER_SIZE];
    long bytes_readed;

    while((bytes_readed = fread(buffer, 1, BUFFER_SIZE, doc)) > 0)
        fwrite(buffer, 1, bytes_readed, arc);
}

void print_bytes(long lim_ceil, long offset){
    long bytes_reamaining, bytes_to_read;
    size_t bytes_readed;
    char buffer[BUFFER_SIZE];

    fseek(lib_arc, offset, SEEK_SET);
    bytes_reamaining = lim_ceil - offset;
    bytes_to_read = (bytes_reamaining > BUFFER_SIZE) ? BUFFER_SIZE : bytes_reamaining;
    bytes_readed = fread(buffer, 1, bytes_to_read, lib_arc);

    for(size_t i = 0; i < bytes_readed; i++){
        printf("%02x ", (unsigned char)buffer[i]);
        if ((i + 1) % 8 == 0)
            printf(" ");
        if ((i + 1) % 16 == 0)
            printf("\n");
    }

    printf("\n");
}

// int check_fread(void *ptr, size_t size, size_t nmemb, FILE *stream){
//     return (fread(ptr, size, nmemb, stream) == nmemb) ? 0 : -1;
// }

// int check_fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream){
//     return (fwrite(ptr, size, nmemb, stream) == nmemb) ? 0 : -1;
// }

int gbv_create(const char *filename){
    FILE *arc = fopen(filename, "wb");

    if(!arc)
        return -1;
    
    SBlock super_bloco = {sizeof(SBlock), 0};
    fwrite(&super_bloco, sizeof(SBlock), 1, arc);

    fclose(arc);

    return 0;
}

int gbv_open(Library *lib, const char *filename){
    lib_arc = fopen(filename, "r+b");
    FILE *arc = fopen(filename, "r+b");

    // arquivo é criado se não existir chamando gbv_create
    if (!lib_arc){
        if(gbv_create(filename) != 0)
            return -2;

        lib_arc = fopen(filename, "r+b");
        if(!lib_arc)
            return -2;

        lib->count = 0;
        lib->docs = NULL;

        return 0;
    }

    // dados iniciais do arquivo são lidos e feita alocações se existir
    SBlock super_bloco;
    fread(&super_bloco, sizeof(SBlock), 1, arc);
    
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
    fread(lib->docs, sizeof(Document), super_bloco.count, arc);

    fclose(arc);
    return 0;
}

int gbv_add(Library *lib, const char *archive, const char *docname){
    FILE *arc = fopen(archive, "r+b");
    FILE *doc = fopen(docname, "rb");
    char buffer[BUFFER_SIZE];

    if (!arc || !doc)
        return -2;

    // carrega informacoes do superbloco
    SBlock super_bloco;
    fread(&super_bloco, sizeof(SBlock), 1, arc);

    // verifica se o arquivo ja existe
    long index_doc_equal;
    for (index_doc_equal = 0; index_doc_equal < lib->count; index_doc_equal++){
        if(strcmp(docname, lib->docs[index_doc_equal].name) == 0)
            break;
    }

    long doc_new_size, doc_old_size, doc_old_offset;
    // arquivo ja existe, substitui
    if (index_doc_equal != lib->count){
        doc_old_size = lib->docs[index_doc_equal].size;
        doc_old_offset = lib->docs[index_doc_equal].offset;

        // calcula o tamanho do novo documento
        fseek(doc, 0, SEEK_END);
        doc_new_size = ftell(doc);
        rewind(doc);

        // arquivo igual com mesmo tamanho
        if(doc_new_size == doc_old_size){
            fseek(arc, doc_old_offset, SEEK_SET);
            write_bytes(arc, doc);
        }
        else{
            long differencial = doc_new_size - doc_old_size;
            long start_read_position = doc_old_offset + doc_old_size;
            long total_bytes_to_move = super_bloco.offset - start_read_position;

            // novo arquivo possui tamanho maior do que o atual
            if(doc_new_size > doc_old_size){; 
                long bytes_remaining = total_bytes_to_move;
                // abre espaco para escrever o novo arquivo
                while(bytes_remaining > 0){
                    long size_to_move = bytes_remaining > BUFFER_SIZE ? BUFFER_SIZE : bytes_remaining;
                    long read_position = start_read_position + bytes_remaining - size_to_move;
                    long write_position = read_position + differencial;

                    // fica empurrando os blocos para frente
                    fseek(arc, read_position, SEEK_SET);
                    fread(buffer, 1, size_to_move, arc);
                    fseek(arc, write_position, SEEK_SET);
                    fwrite(buffer, 1, size_to_move, arc);

                    bytes_remaining -= size_to_move;
                }

                // escreve o novo documento substituindo o antigo de mesmo nome
                fseek(arc, doc_old_offset, SEEK_SET);
                write_bytes(arc, doc);

                // atualiza os metadados do documento novo e dos documentos movidos
                for (int i = index_doc_equal+1; i < lib->count; i++)
                    lib->docs[i].offset += differencial;

                lib->docs[index_doc_equal].size = doc_new_size;
                lib->docs[index_doc_equal].date = time(NULL);

                // atualiza o offset do superbloco e salva em disco
                super_bloco.offset += differencial;
            }
            // novo arquivo possui tamanho menor do que o atual
            else{
                // substitui o antigo documento pelo novo. Um "buraco" eh criado
                fseek(arc, doc_old_offset, SEEK_SET);
                write_bytes(arc, doc);
            
                long start_write_position = start_read_position + differencial;
                long bytes_moved = 0;

                // tapando o "buraco"
                while(bytes_moved < total_bytes_to_move){
                    long bytes_remaining = total_bytes_to_move - bytes_moved;
                    long size_to_move = bytes_remaining > BUFFER_SIZE ? BUFFER_SIZE : bytes_remaining;

                    long read_position = start_read_position + bytes_moved;
                    long write_position = start_write_position + bytes_moved;
                    fseek(arc, read_position, SEEK_SET);
                    fread(buffer, 1, size_to_move, arc);
                    fseek(arc, write_position, SEEK_SET);
                    fwrite(buffer, 1, size_to_move, arc);

                    bytes_moved += size_to_move;
                }

                // atualizando os metadados
                for(int i = index_doc_equal + 1; i < lib->count; i++)
                    lib->docs[i].offset += differencial;

                // atualiza o superbloco
                super_bloco.offset += differencial;

                // trunca o arquivo
                int fd = fileno(arc);
                ftruncate(fd, super_bloco.offset + lib->count * sizeof(Document));
            }
        }
        lib->docs[index_doc_equal].date = time(NULL);
        lib->docs[index_doc_equal].size = doc_new_size;
    }
    // novo arquivo
    else{
        // armeza o arquivo no final da area de documento usando buffer
        fseek(arc, super_bloco.offset, SEEK_SET);
        write_bytes(arc, doc);
        
        // aumenta o vetor de metadados
        Document *ptr_tmp;
        if(!(ptr_tmp= realloc(lib->docs, sizeof(Document) * (lib->count + 1)))){
            free(lib->docs);
            lib->docs = NULL;
            lib->count = 0;

            fclose(arc);
            fclose(doc);
            return -1;
        }
        
        lib->docs = ptr_tmp;
        // adiciona os metadados do novo documento ao vetor (RAM)
        snprintf(lib->docs[lib->count].name, MAX_NAME, "%s", docname);
        lib->docs[lib->count].date = time(NULL);
        lib->docs[lib->count].size = ftell(doc);
        lib->docs[lib->count].offset = super_bloco.offset;
        lib->count++;

        // atualiza o super bloco
        super_bloco.count++;
        super_bloco.offset += lib->docs[lib->count - 1].size;
        
    }

    // atualiza os dados modificados em disco
    fseek(arc, super_bloco.offset, SEEK_SET);
    fwrite(lib->docs, sizeof(Document), lib->count, arc);
    fseek(arc, 0, SEEK_SET);
    fwrite(&super_bloco, sizeof(SBlock), 1, arc);
    
    fclose(doc);
    fclose(arc);
    
    return 0;
}

int gbv_remove(Library *lib, const char *docname){
    long index_removed;
    // verifica se o docname existe na biblioteca
    for (index_removed = 0; index_removed < lib->count; index_removed++){
        if(strcmp(docname, lib->docs[index_removed].name) == 0)
            break;
    }

    // arquivo nao encontrado
    if(index_removed == lib->count){
        printf("Arquivo %s nao existe na biblioteca\n", docname);
        return -1;
    }

    if(!lib_arc)
        return -2;

    SBlock super_bloco;
    fseek(lib_arc, 0, SEEK_SET);
    fread(&super_bloco, sizeof(Document), 1, lib_arc);

    long doc_size = lib->docs[index_removed].size;
    long read_position = lib->docs[index_removed].offset + doc_size;
    long write_position = read_position - doc_size;
    long total_bytes_to_move = super_bloco.offset - read_position;
    long bytes_moved = 0;

    // tampa o buraco deixado pelo documento removido
    while (bytes_moved < total_bytes_to_move){
        char buffer[BUFFER_SIZE];

        long bytes_remaining = total_bytes_to_move - bytes_moved;
        long size_to_move = bytes_remaining > BUFFER_SIZE ? BUFFER_SIZE : bytes_remaining;

        read_position += bytes_moved;
        write_position += bytes_moved;
        fseek(lib_arc, read_position, SEEK_SET);
        fread(buffer, 1, size_to_move, lib_arc);
        fseek(lib_arc, write_position, SEEK_SET);
        fwrite(buffer, 1, size_to_move, lib_arc);

        bytes_moved += size_to_move;
    }
    
    // diminui e atualizar os metadados
    for (long i = index_removed; i < (lib->count - 1); i++){
        lib->docs[i+1].offset -= doc_size;
        lib->docs[i] = lib->docs[i+1];
    }
    lib->count--;

    Document *tmp_ptr;
    if((lib->count > 0) && !(tmp_ptr = realloc(lib->docs, sizeof(Document) * lib->count))){
        free(lib->docs);
        return -2;
    }
    lib->docs = tmp_ptr;    

    // atualiza superbloco e area de diretorio
    super_bloco.count = lib->count;
    super_bloco.offset -= doc_size;

    fseek(lib_arc, 0, SEEK_SET);
    fwrite(&super_bloco, sizeof(SBlock), 1, lib_arc);
    fseek(lib_arc, super_bloco.offset, SEEK_SET);
    fwrite(lib->docs, sizeof(Document), lib->count, lib_arc);

    // trunca o arquivo
    int fd = fileno(lib_arc);
    ftruncate(fd, super_bloco.offset + sizeof(Document) * lib->count);

    return 0;
}

int gbv_list(const Library *lib){
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

    fclose(lib_arc);
    return 0;
}

// Função tá estranha, como será aberto o arquivo sem o nome?
int gbv_view(const Library *lib, const char *docname){
    int index_view;
    for (index_view = 0; index_view < lib->count; index_view++){
        if(strcmp(docname, lib->docs[index_view].name) == 0)
            break;
    }

    if(index_view == lib->count){
        printf("Arquivo %s nao existe na biblioteca\n", docname);
        fclose(lib_arc);
        return -1;
    }

    if(!lib_arc)
        return -2;

    long lim_floor, current_offset;
    lim_floor = current_offset = lib->docs[index_view].offset;
    long lim_ceil = lib->docs[index_view].offset + lib->docs[index_view].size;
    
    print_bytes(lim_ceil, current_offset);

    int keep_reading = 1;
    while(keep_reading){
        char op;
        scanf(" %c", &op);
        switch (op){
        case 'n':
            if(current_offset + BUFFER_SIZE < lim_ceil){
                current_offset += BUFFER_SIZE;
                print_bytes(lim_ceil, current_offset);
            }
            break;
        case 'p':
            if(current_offset - BUFFER_SIZE >= lim_floor){
                current_offset -= BUFFER_SIZE;
                print_bytes(lim_ceil, current_offset);
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

    fclose(lib_arc);
    return 0;
}

int gbv_order(Library *lib, const char *archive, const char *criteria){
    return 0;
}