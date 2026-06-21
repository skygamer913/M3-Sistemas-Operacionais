# Trabalho M3 - Sistemas Operacionais

- Conteúdo: **Implementação de Funções de Manipulação de Sistema de Arquivos FAT16**
- Grupo: Seiki Takao Iizuka e Eduardo Pascotini Terra

## Comandos para execução do código

- `gcc main.c -o main`
- `./main`

## Explicação do Código

- Para facilitar as operações no arquivo *IMG* no código, criamos *structs* do **Boot Record** e **Entrada de Diretório**:
    ``` C
    // Estrutura do Boot Record do FAT16
    typedef struct {
        uint8_t bootstrap[3];
        char oem_name[8];
        uint16_t bytes_per_sector;   
        uint8_t sectors_per_cluster; 
        uint16_t reserved_sectors;   
        uint8_t fat_count;
        uint16_t root_entry_count, total_sectors_16;
        uint8_t media_type;
        uint16_t sectors_per_fat, sectors_per_track, head_count;
        uint32_t hidden_sectors, total_sectors_32;
        uint8_t drive_number, reserved, boot_signature;
        uint32_t volume_id;
        char volume_label[11], file_system_type[8];
        uint8_t boot_code[448];
        uint16_t boot_sector_signature;
    } FAT16_BootRecord;

    // Estrutura de uma Entrada de Diretório do FAT16 (32 bytes)
    typedef struct {
        char filename[8], ext[3];
        uint8_t attributes, reserved, creation_time_ms;
        uint16_t creation_time, creation_date, last_access_date, first_cluster_high, last_modified_time,
        last_modified_date, first_cluster_low;  
        uint32_t file_size;          
    } FAT16_DirEntry;
    ```
- **No main**, logo no começo é pedido para o usuário digitar o nome do arquivo *IMG*, com uma condição de entrada em que caso não encontre ou o arquivo for inválido encerra o programa com erro. Caso o arquivo digitado for válido, um menu será exibido na tela, com as opções: **listar conteúdo do disco, listar conteúdo de um arquivo, exibir os atributos de um arquivo, renomear um arquivo, inserir/criar um arquivo, apagar/remover um arquivo e a de sair**.
    ``` C
    int main() {
        FAT16_Context ctx;
        ctx.disk_file = NULL;
        int opcao;
        char disco[100];

        printf("Nome do disco:\n");
        scanf("%s", disco);
        
        if (!init_filesystem(&ctx, disco)) return 1;

        do {
            printf("\n---- Menu ----\n");
            printf("1. Listar conteúdo do disco\n");
            printf("2. Listar conteúdo de um arquivo\n");
            printf("3. Exibir os atributos de um arquivo\n");
            printf("4. Renomear um arquivo\n");
            printf("5. Inserir/criar um arquivo\n");
            printf("6. Apagar/remover um arquivo\n");
            printf("0. Sair\n");
            printf("Escolha uma opção: ");
            if (scanf("%d", &opcao) != 1) break;
            printf("\n-----------------\n");

            switch(opcao){
                case 1:
                    list_root_directory(&ctx);
                    break;

                case 2:
                    read_file_content(&ctx);
                    break;

                case 3:
                    display_file_attributes(&ctx);
                    break;

                case 4:
                    rename_file(&ctx);
                    break;

                case 5:
                    insert_file(&ctx);
                    break;

                case 6:
                    delete_file(&ctx);
                    break;

                case 0:
                    printf("Saindo...\n");
                    if (ctx.disk_file) fclose(ctx.disk_file);
                    break;

                default:
                    printf("Opção inválida. Tente novamente.\n");
            }
        } while(opcao != 0);

        return 0;
    }
    ```
- Primeiro temos a função de **listar o conteúdo de disco**, que exibe os nomes do arquivos na imagem de disco.
    ``` C
    // 1. Listar conteúdo do disco
    void list_root_directory(FAT16_Context *ctx) {
        fseek(ctx->disk_file, ctx->root_dir_offset, SEEK_SET);
        FAT16_DirEntry entry;
        char printed_name[14];
        int i;

        printf("\n%-15s %-12s %s\n", "Nome", "Tamanho", "Tipo");
        printf("---------------------------------------------\n");

        for (i = 0; i < ctx->boot_record.root_entry_count; i++) {
            fread(&entry, sizeof(FAT16_DirEntry), 1, ctx->disk_file);

            if (entry.filename[0] == 0x00) break;
            if ((uint8_t)entry.filename[0] == 0xE5) continue;
            if (entry.attributes == ATTR_LONG_NAME || (entry.attributes & ATTR_VOLUME_ID)) continue;

            format_name(printed_name, entry.filename, entry.ext);
            
            if (entry.attributes & ATTR_DIRECTORY) printf("%-15s %-12s <DIR>\n", printed_name, "-"); 
            else printf("%-15s %-10u Bytes\n", printed_name, entry.file_size);
        }
    }
    ```
- Depois, temos a função de **listar o conteúdo de um arquivo**, que exibe o conteúdo do arquivo digitado pelo usuário, caso o arquivo não seja identificado, retorna ao menu.
    ``` C
    // 2. Listar conteúdo de um arquivo
    void read_file_content(FAT16_Context *ctx) {
        char target_name[15], fat_name[8], fat_ext[3];
        int i, found = 0;

        printf("Digite o nome do arquivo para ler (ex: TESTE.TXT): ");
        scanf("%s", target_name);

        parse_to_fat_name(target_name, fat_name, fat_ext);
        fseek(ctx->disk_file, ctx->root_dir_offset, SEEK_SET);
        FAT16_DirEntry entry;

        for (i = 0; i < ctx->boot_record.root_entry_count; i++) {
            fread(&entry, sizeof(FAT16_DirEntry), 1, ctx->disk_file);

            if (entry.filename[0] == 0x00) break;
            if ((uint8_t)entry.filename[0] == 0xE5) continue;

            if (memcmp(entry.filename, fat_name, 8) == 0 && memcmp(entry.ext, fat_ext, 3) == 0) {
                found = 1;
                if (entry.attributes & ATTR_DIRECTORY) {
                    printf("Erro: '%s' é um diretório.\n", target_name);
                    return;
                }

                printf("\n--- Conteúdo do Arquivo [%s] ---\n", target_name);
                uint16_t current_cluster = entry.first_cluster_low;
                uint32_t bytes_remaining = entry.file_size;
                uint32_t cluster_size = ctx->boot_record.bytes_per_sector * ctx->boot_record.sectors_per_cluster;
                char *buffer = malloc(cluster_size);

                while (current_cluster >= 0x0002 && current_cluster <= 0xFFEF && bytes_remaining > 0) {
                    uint32_t offset = get_cluster_offset(ctx, current_cluster);
                    fseek(ctx->disk_file, offset, SEEK_SET);
                    fread(buffer, 1, cluster_size, ctx->disk_file);

                    uint32_t to_print = (bytes_remaining < cluster_size) ? bytes_remaining : cluster_size;
                    for (uint32_t j = 0; j < to_print; j++) {
                        putchar(buffer[j]);
                    }

                    bytes_remaining -= to_print;
                    current_cluster = read_fat_entry(ctx, current_cluster);
                }

                free(buffer);
                printf("\n--------------------------------\n");
                break;
            }
        }

        if (!found) printf("Arquivo não encontrado.\n");
    }
    ```
- Também temos a função de **exibir os atributos de um arquivo**, que caso o nome do arquivo digitado seja identificado, exibe todos os atributos do arquivo.
    ``` C
    // 3. Exibir os atributos de um arquivo
    void display_file_attributes(FAT16_Context *ctx) {
        char target_name[15], fat_name[8], fat_ext[3];
        int i;

        printf("Digite o nome do arquivo para ver os atributos (ex: TEXTO2.TXT): ");
        scanf("%s", target_name);

        parse_to_fat_name(target_name, fat_name, fat_ext);
        fseek(ctx->disk_file, ctx->root_dir_offset, SEEK_SET);

        FAT16_DirEntry entry;
        int found = 0;

        for (i = 0; i < ctx->boot_record.root_entry_count; i++) {
            fread(&entry, sizeof(FAT16_DirEntry), 1, ctx->disk_file);
            if (entry.filename[0] == 0x00) break;
            if ((uint8_t)entry.filename[0] == 0xE5) continue;

            if (memcmp(entry.filename, fat_name, 8) == 0 && memcmp(entry.ext, fat_ext, 3) == 0) {
                found = 1;
                printf("\nAtributos de '%s':\n", target_name);
                
                // 1. Tipo do Arquivo
                printf("  Tipo do Arquivo:   %s\n", entry.ext);

                // 2. Atributos da FAT
                printf("  Somente Leitura:   %s\n", (entry.attributes & ATTR_READ_ONLY) ? "SIM" : "NÃO");
                printf("  Oculto:            %s\n", (entry.attributes & ATTR_HIDDEN) ? "SIM" : "NÃO");
                printf("  Sistema:           %s\n", (entry.attributes & ATTR_SYSTEM) ? "SIM" : "NÃO");
                printf("  Arquivo (Archive): %s\n", (entry.attributes & ATTR_ARCHIVE) ? "SIM" : "NÃO");
                printf("  Tamanho:           %u bytes\n", entry.file_size);
                
                // 3. Data e Hora de Criação
                uint16_t d_cria = entry.creation_date;
                uint16_t t_cria = entry.creation_time;
                printf("  Data de Criação:   %02d/%02d/%04d\n", d_cria & 0x1F, (d_cria >> 5) & 0x0F, ((d_cria >> 9) & 0x7F) + 1980);
                printf("  Hora de Criação:   %02d:%02d:%02d\n", (t_cria >> 11) & 0x1F, (t_cria >> 5) & 0x3F, (t_cria & 0x1F) * 2);

                // 4. Última Modificação
                uint16_t d_mod = entry.last_modified_date;
                uint16_t t_mod = entry.last_modified_time;
                // Verifica Há Data de Modificação
                if (d_mod <= d_cria) printf("  Últ. Modificação:  N/A\n");
                else printf("  Últ. Modificação:  %02d/%02d/%04d às %02d:%02d:%02d\n", 
                    d_mod & 0x1F, (d_mod >> 5) & 0x0F, ((d_mod >> 9) & 0x7F) + 1980,
                    (t_mod >> 11) & 0x1F, (t_mod >> 5) & 0x3F, (t_mod & 0x1F) * 2);

                // 5. Localização (Mapeia o Cluster e o endereço de início no arquivo binário)
                if (entry.first_cluster_low == 0) {
                    printf("  Localização:       Diretório Raiz (Vazio)\n");
                } else {
                    uint32_t offset_fisico = get_cluster_offset(ctx, entry.first_cluster_low);
                    printf("  Localização:       Cluster Inicial %u (Offset: 0x%08X)\n", entry.first_cluster_low, offset_fisico);
                }
            }
        }

        if (!found) printf("Arquivo não encontrado.\n");
    }
    ```
- Depois tem a função de **renomear o arquivo**, em que primeiro o usuário digita o nome do arquivo que deseja renomear, e quando o arquivo é identificado, será pedido um novo nome do arquivo, e será atualizado a **data de modificação**.
    ``` C
    // 4. Renomear um arquivo
    void rename_file(FAT16_Context *ctx) {
        char target_name[15], new_name[15], old_fat_name[8], old_fat_ext[3], new_fat_name[8], new_fat_ext[3];
        FAT16_DirEntry entry;
        long entry_offset;
        int i;

        printf("Digite o nome do arquivo atual: ");
        scanf("%s", target_name);
        printf("Digite o novo nome (ex: NOVO.TXT): ");
        scanf("%s", new_name);

        parse_to_fat_name(target_name, old_fat_name, old_fat_ext);
        parse_to_fat_name(new_name, new_fat_name, new_fat_ext);

        fseek(ctx->disk_file, ctx->root_dir_offset, SEEK_SET);

        for (i = 0; i < ctx->boot_record.root_entry_count; i++) {
            entry_offset = ftell(ctx->disk_file);
            fread(&entry, sizeof(FAT16_DirEntry), 1, ctx->disk_file);
            if (entry.filename[0] == 0x00) break;
            if ((uint8_t)entry.filename[0] == 0xE5) continue;

            if (memcmp(entry.filename, old_fat_name, 8) == 0 && memcmp(entry.ext, old_fat_ext, 3) == 0) {
                memcpy(entry.filename, new_fat_name, 8);
                memcpy(entry.ext, new_fat_ext, 3);

                fseek(ctx->disk_file, entry_offset, SEEK_SET);
                fwrite(&entry, sizeof(FAT16_DirEntry), 1, ctx->disk_file);
                printf("Arquivo renomeado com sucesso.\n");
                return;
            }
        }
        printf("Arquivo original não encontrado.\n");
    }
    ```
- Temos a função de **inserir/criar um arquivo novo**, em que primeiro será pedido o nome de um arquivo externo a ser injetado, depois será pedido o nome do novo arquivo na imagem FAT16.
    ``` C
    // 5. Inserir/criar um arquivo externo na imagem
    void insert_file(FAT16_Context *ctx) {
        char ext_path[256], disk_filename[15], fat_name[8], fat_ext[3];
        FAT16_DirEntry entry;
        long target_entry_offset = -1;
        int i;

        printf("Digite o caminho do arquivo externo a ser injetado: ");
        scanf("%s", ext_path);
        printf("Nome de destino na imagem FAT16 (ex: COPIA.TXT): ");
        scanf("%s", disk_filename);

        FILE *ext_file = fopen(ext_path, "rb");
        if (!ext_file) {
            printf("Erro: Não foi possível abrir o arquivo externo '%s'.\n", ext_path);
            return;
        }

        fseek(ext_file, 0, SEEK_END);
        uint32_t file_size = ftell(ext_file);
        fseek(ext_file, 0, SEEK_SET);

        parse_to_fat_name(disk_filename, fat_name, fat_ext);
        fseek(ctx->disk_file, ctx->root_dir_offset, SEEK_SET);

        for (i = 0; i < ctx->boot_record.root_entry_count; i++) {
            long current_offset = ftell(ctx->disk_file);
            fread(&entry, sizeof(FAT16_DirEntry), 1, ctx->disk_file);
            if (entry.filename[0] == 0x00 || (uint8_t)entry.filename[0] == 0xE5) {
                target_entry_offset = current_offset;
                break;
            }
        }

        if (target_entry_offset == -1) {
            printf("Erro: Diretório Raiz cheio.\n");
            fclose(ext_file);
            return;
        }

        uint32_t cluster_size = ctx->boot_record.sectors_per_cluster * ctx->boot_record.bytes_per_sector;
        // CORREÇÃO 1: Usa calloc para iniciar o buffer completamente zerado
        char *buffer = calloc(1, cluster_size); 
        if (!buffer) {
            printf("Erro de alocação de memória.\n");
            fclose(ext_file);
            return;
        }
        
        uint16_t first_cluster = 0, prev_cluster = 0;
        uint32_t remaining = file_size;

        if (remaining > 0) {
            while (remaining > 0) {
                uint16_t free_cluster = 0;
                for (uint16_t c = 2; c < 0xFFF0; c++) {
                    if (read_fat_entry(ctx, c) == 0x0000) {
                        free_cluster = c;
                        break;
                    }
                }

                if (free_cluster == 0) {
                    printf("Erro: Sem espaço em disco FAT16.\n");
                    free(buffer);
                    fclose(ext_file);
                    return;
                }

                if (first_cluster == 0) first_cluster = free_cluster;
                if (prev_cluster != 0) write_fat_entry(ctx, prev_cluster, free_cluster);

                // CORREÇÃO 2: Limpa explicitamente o buffer antes de cada leitura
                memset(buffer, 0, cluster_size);
                
                uint32_t bytes_to_write = (remaining < cluster_size) ? remaining : cluster_size;
                fread(buffer, 1, bytes_to_write, ext_file);

                fseek(ctx->disk_file, get_cluster_offset(ctx, free_cluster), SEEK_SET);
                fwrite(buffer, 1, cluster_size, ctx->disk_file);

                write_fat_entry(ctx, free_cluster, 0xFFFF); 
                prev_cluster = free_cluster;
                remaining -= bytes_to_write;
            }
        }

        memset(&entry, 0, sizeof(FAT16_DirEntry));
        memcpy(entry.filename, fat_name, 8);
        memcpy(entry.ext, fat_ext, 3);

        entry.attributes = ATTR_ARCHIVE;
        entry.first_cluster_low = first_cluster;
        entry.file_size = file_size;
        
        time_t currentTime;
        time(&currentTime);
        struct tm *tm_info = localtime(&currentTime);

        uint16_t ano_fat = (tm_info->tm_year + 1900 - 1980) << 9;
        uint16_t mes_fat = (tm_info->tm_mon + 1) << 5;
        uint16_t dia_fat = tm_info->tm_mday;
        entry.creation_date = ano_fat | mes_fat | dia_fat;

        uint16_t hora_fat = tm_info->tm_hour << 11;
        uint16_t min_fat = tm_info->tm_min << 5;
        uint16_t seg_fat = (tm_info->tm_sec / 2);
        entry.creation_time = hora_fat | min_fat | seg_fat;

        fseek(ctx->disk_file, target_entry_offset, SEEK_SET);
        fwrite(&entry, sizeof(FAT16_DirEntry), 1, ctx->disk_file);

        free(buffer);
        fclose(ext_file);
        printf("Arquivo '%s' inserido com sucesso (%u bytes).\n", disk_filename, file_size);
    }
    ```
- Temos a função de **apagar/remover um arquivo**, em que será pedido para o usuário o nome do arquivo a ser excluído no disco FAT16, e assim que o arquivo é identificado, o arquivo é apagado no disco.
    ``` C
    // 6. Apagar/remover um arquivo
    void delete_file(FAT16_Context *ctx) {
        char target_name[15], fat_name[8], fat_ext[3];
        FAT16_DirEntry entry;
        long entry_offset;
        int i;

        printf("Digite o nome do arquivo a remover: ");
        scanf("%s", target_name);

        parse_to_fat_name(target_name, fat_name, fat_ext);
        fseek(ctx->disk_file, ctx->root_dir_offset, SEEK_SET);

        for (i = 0; i < ctx->boot_record.root_entry_count; i++) {
            entry_offset = ftell(ctx->disk_file);
            fread(&entry, sizeof(FAT16_DirEntry), 1, ctx->disk_file);
            if (entry.filename[0] == 0x00) break;
            if ((uint8_t)entry.filename[0] == 0xE5) continue;

            if (memcmp(entry.filename, fat_name, 8) == 0 && memcmp(entry.ext, fat_ext, 3) == 0) {
                uint16_t current_cluster = entry.first_cluster_low;
                
                while (current_cluster >= 0x0002 && current_cluster <= 0xFFEF) {
                    uint16_t next = read_fat_entry(ctx, current_cluster);
                    write_fat_entry(ctx, current_cluster, 0x0000); 
                    current_cluster = next;
                }

                entry.filename[0] = (char)0xE5;
                fseek(ctx->disk_file, entry_offset, SEEK_SET);
                fwrite(&entry, sizeof(FAT16_DirEntry), 1, ctx->disk_file);

                printf("Arquivo '%s' removido com sucesso.\n", target_name);
                return;
            }
        }
        printf("Arquivo não encontrado.\n");
    }
    ```

## Extras

- Ferramenta de exploração de discos: [Hexed](https://hexed.it/)
- [Informações sobre FAT16](http://www.maverick-os.dk/FileSystemFormats/FAT16_FileSystem.html)
- [Informações sobre FAT](https://www.tavi.co.uk/phobos/fat.html)
