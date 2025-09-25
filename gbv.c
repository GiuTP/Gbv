#include "gbv.h"
#include "util.h"   // format_date, write_bytes
#include <stdio.h>  // fopen, fclose, fread, fwrite, fseek, ftell, printf
#include <stdlib.h> // realloc
#include <string.h> // strcmp
#include <unistd.h> // fileno, ftruncate

// Estrutura do superbloco
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

    // arquivo é criado se não existir chamando gbv_create
    if (!arc){
        if(gbv_create(filename) != 0)
            return -2;

        lib->count = 0;
        lib->docs = NULL;

        return 0;
    }

    SBlock super_bloco;
    // dados iniciais do arquivo são lidos e feita alocações se existir
    if(fread(&super_bloco, sizeof(SBlock), 1, arc) != 1){
        fclose(arc);
        return -1;
    }
    
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
    if(fread(lib->docs, sizeof(Document), lib->count, arc) != lib->count){
        fclose(arc);
        return -1;
    }

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
    int index_doc_equal = -1;
    for (int i = 0; i < lib->count; i++){
        if(strcmp(docname, lib->docs[i].name) == 0){
            index_doc_equal = i;
            break;
        }
    }

    long doc_new_size, doc_old_size, doc_old_offset;
    // arquivo ja existe, substitui
    if (index_doc_equal >= 0){
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
            lib->docs[index_doc_equal].date = time(NULL);
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
                for(int i = index_doc_equal + 1; i < lib->count; i++){
                    lib->docs[i].offset += differencial;
                }
                
                // atualiza o superbloco
                super_bloco.offset += differencial;

                // trunca o arquivo
                int fd = fileno(arc);
                ftruncate(fd, super_bloco.offset + lib->count * sizeof(Document));
            }
            // atualiza os metadados
            lib->docs[index_doc_equal].date = time(NULL);
            lib->docs[index_doc_equal].size = doc_new_size;
        }
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

int gbv_remove(Library *lib, const char *archive, const char *docname){
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
        if(!(tmp_ptr = realloc(lib->docs, sizeof(Document) * lib->count))){
            free(lib->docs);
            return -2;
        }
        lib->docs = tmp_ptr;

        FILE *arc = fopen(archive, "r+b");
        if(!arc)
            return -2;

        SBlock super_bloco;
        fread(&super_bloco, sizeof(SBlock), 1, arc);

        // atualiza superbloco e area de diretorio
        super_bloco.count = lib->count;
        fseek(arc, 0, SEEK_SET);
        fwrite(&super_bloco, sizeof(SBlock), 1, arc);
        fseek(arc, super_bloco.offset, SEEK_SET);
        fwrite(lib->docs, sizeof(Document), lib->count, arc);

        // trunca o arquivo
        int fd = fileno(arc);
        ftruncate(fd, super_bloco.offset + sizeof(Document) * lib->count);

        fclose(arc);

        return 0;
    }

    // nao encontrado
    return -1;
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
    return 0;
}

// Função tá estranha, como será aberto o arquivo sem o nome?
int gbv_view(const Library *lib, const char *archive, const char *docname){
    int index;
    for (index = 0; index < lib->count; index++){
        if(strcmp(docname, lib->docs[index].name) == 0)
            break;
    }
    
    if(index < lib->count){
        FILE *arc = fopen(archive, "rb");

        if (!arc)
            return -2;

        long current_offset = lib->docs[index].offset;
        long lim_ceil = lib->docs[index].offset + lib->docs[index].size;
        long lim_floor = current_offset;
        char buffer[BUFFER_SIZE];

        while(1){
            long bytes_remaining, bytes_to_read;
            size_t bytes_readed;

            // mostra o conteudo do bloco
            fseek(arc, current_offset, SEEK_SET);
            bytes_remaining = lim_ceil - current_offset;
            bytes_to_read = bytes_remaining > BUFFER_SIZE ? BUFFER_SIZE : bytes_remaining;
            bytes_readed = fread(buffer, 1, bytes_to_read, arc);
            fwrite(buffer, 1, bytes_readed, stdout);
            printf("\n");

            // escolhe o proximo bloco
            char op;
            scanf(" %c", &op);
            // checa se o bloco sera imprimido ou atingiu algum limite
            if(op == 'n'){
                if(current_offset + BUFFER_SIZE >= lim_ceil)
                    printf("(Fim do documento)\n");
                else
                    current_offset += BUFFER_SIZE;
            }
            else if(op == 'p'){
                if(current_offset - BUFFER_SIZE < lim_floor)
                    printf("(Inicio do documento)\n");
                else
                    current_offset -= BUFFER_SIZE;
            }
            else if(op == 'q')
                break;
        }
        
        fclose(arc);
        return 0;
    }
    
    printf("Arquivo %s nao existe na biblioteca\n", docname);
    return -1;
}

int gbv_order(Library *lib, const char *archive, const char *criteria){
    return 0;
}