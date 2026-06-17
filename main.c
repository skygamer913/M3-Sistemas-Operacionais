#include <stdio.h>


int main(){
    int opcao = -1;

    while(opcao != 0){

        printf("\n---- Menu ----\n");
        printf("1. Listar conteúdo do disco\n");
        printf("2. Listar conteúdo de um arquivo\n");
        printf("3. Exibir os atributos de um arquivo\n");
        printf("4. Renomear um arquivo\n");
        printf("5. Inserir/criar um arquivo\n");
        printf("6. Apagar/remover um arquivo\n");
        printf("0. Sair\n");
        printf("Escolha uma opção: ");
        scanf("%d", &opcao);
        printf("\n-----------------\n");

        switch(opcao){
            case 1:
                // Listar conteúdo do disco
                break;
            case 2:
                // Listar conteúdo de um arquivo
                break;
            case 3:
                // Exibir os atributos de um arquivo
                break;
            case 4:
                // Renomear um arquivo
                break;
            case 5:
                // Inserir/criar um arquivo
                break;
            case 6:
                // Apagar/remover um arquivo
                break;
            case 0:
                printf("Saindo...\n");
                break;
            default:
                printf("Opção inválida. Tente novamente.\n");
        }
    }

    return 0;
}