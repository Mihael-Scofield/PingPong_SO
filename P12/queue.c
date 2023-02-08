// Mihael Scofield de Azevedo - GRR20182621 - msa18

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "queue.h"

int queue_size (queue_t *queue) {
    /* Verifica erros */
    // Verifica se a vila eh vazia
    if (queue == NULL) {
        fprintf(stderr, "Fila passada para contagem foi nula, fila vazia. \n");
        return 0;
    }

    /* Prepara as variaveis para contagem */
    queue_t* proximoElem = queue->next; // Elemento que caminhara pela lista, ja comecando pelo proximo
    int cont = 1;
    
    /* Contagem propriamente dita */
    while (proximoElem != queue) {
        proximoElem = proximoElem->next;        
        cont++;
    }

    return cont;
}

void queue_print (char *name, queue_t *queue, void print_elem (void*) ) {
    /* Verifica erros */
    // Ve se a fila eh vazia
    if (queue == NULL) {
        printf("%s: []\n", name); 
        return;
    }

    /* Impressao propriamente dita */
    printf("%s: [", name);
    print_elem(queue); // impressao do primeiro elemento
    queue_t *qAux = queue->next; // o auxiliar caminhara pela fila ate bater no inicio novamente
    
    while (qAux != queue) {
        printf(" "); 
        print_elem(qAux);
        qAux = qAux->next; // Caminha para proximo;
        if (qAux == NULL) {
            fprintf(stderr, "Elemento nulo identificado, BROKEN CHAIN");
            return;
        }
    }
    printf("]\n");
    return;
}

int queue_append (queue_t **queue, queue_t *elem) {
    /* Verifica erros */
    // - a fila deve existir
    if (queue == NULL) {
        fprintf(stderr, "Fila passada para adissao nao existe");
        return -1;
    }
    // - o elemento deve existir
    if (elem == NULL) {
        fprintf(stderr, "O elemento a adicionar eh nulo");
        return -1;
    }
    // - o elemento nao deve estar em outra fila
    if ( (elem->next != NULL) || (elem->prev != NULL) ) {
        fprintf(stderr, "O elemento ja pertence a outra fila.");
        return -1;
    }

    /* Insercoes propriamente ditas */
    // Exemplo 2, insercao em fila vazia
    if ((*queue) == NULL) {
        elem->next = elem;
        elem->prev = elem;
        (*queue) = elem; // "Inicio" da fila aponta para o elemento

        return 0;
    }

    // Exemplo 3, insercao no final, fila NAO vazia
    // A estrategia eh caminhar pela fila ate encontrar o ultimo elemento, 
    // para garantir que o elemento ja nao esteja na fila.
    queue_t *proximoElem = (*queue)->next;

    do {
        if (proximoElem == NULL) {
            fprintf(stderr, "Elemento nulo identificado, BROKEN CHAIN");
            return -1;
        }
        if (proximoElem == elem) {
            fprintf(stderr, "Elemento passado ja estava conectado a fila");
            return -1;
        }
        proximoElem = proximoElem->next; // Atualiza o proximo elemento
    } while (proximoElem != (*queue)->next); // Bati no final da fila

    // Insercao propriamente dita 
    (*queue)->prev->next = elem; // O proximo do "ultimo" seria o novo elemento
    elem->prev = (*queue)->prev; // o antecessor do elemento eh o "ultimo"
    elem->next = (*queue);       // O proximo do elemento eh o primeiro, pois a fila eh duplamente encadeada
    (*queue)->prev = elem;       // O Antecessor ao "primeiro" eh elem, isto eh, elem vai pro final
    
    return 0;
}

int queue_remove (queue_t **queue, queue_t *elem) {
    /* Verifica erros */
    // - a fila deve existir
    if (queue == NULL) {
        fprintf(stderr, "Fila passada para remocao nao existe");
        return -1;
    }
    // - o elemento deve existir
    if (elem == NULL) {
        fprintf(stderr, "Elemento passado para remocao nao existe");
        return -1;
    }
    queue_t *qAux = (*queue);

    /* Caso 1 - o elemento a ser retirado eh o primeiro */
    if (qAux == elem) {
        // Caso 1.1 - a fila tenha apenas um unico elemento
        if (qAux->next == qAux) {
            elem->next = NULL;            
            elem->prev = NULL;
            (*queue) = NULL; // Matamos a fila
            return 0;
        }
        // Caso 1.2 - O A fila nao eh vazia, o segundo elemento agora eh o inicio da fila
        (*queue)->prev->next = (*queue)->next; // O ultimo proximo agora eh o segundo
        (*queue)->next->prev = (*queue)->prev; // O antecessor do segundo agora eh o ultimo
        (*queue) = (*queue)-> next; // O primeiro agora eh o antigo segundo

        elem->next = NULL; // Limpeza do elemento
        elem->prev = NULL;
        return 0;
    }

    /* Caso 2 - O elemento a ser retirado esta no meio da fila */
    qAux = qAux->next; // Caminho para o proximo, pois ja busquei o primeiro

    // Busca deve seguir enquanto nao encontrar o elemento ou bater novamente no inicio da fila
    while (qAux != elem && qAux != (*queue)) { 
        qAux = qAux->next;
        if(qAux == NULL) {
            fprintf(stderr, "Elemento nulo identificado, BROKEN CHAIN \n");
            return -1;            
        }
    }

    // Caso qAux tenha voltado ao primeiro elemento,
    // quer dizer que o elemento passado nao foi encontrado
    if (qAux == (*queue)) {
        fprintf(stderr, "Elemento para remocao nao foi encontrado na fila. \n");
        return -1;
    }

    // Busca efetuada, agora qAux possui o endereco de quem deve ser retirado
    elem->prev->next = elem->next; // Antecessor do elemento aponta para o proximo
    elem->next->prev = qAux->prev; // O antecessor do proximo elemento aponta para o antecessor de quem saiu
    
    elem->next = NULL; // Limpa elemento
    elem->prev = NULL;

    return 0;
}