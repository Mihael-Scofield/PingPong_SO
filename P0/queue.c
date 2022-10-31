#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "queue.h"

int queue_size (queue_t *queue) {
    /* Verifica erros */
    // Verifica se a vila eh vazia
    if (queue == NULL) {
        return 0;
    }

    /* Prepara as variaveis para contagem */
    int cont = 0;
    queue_t* ultimoElem = queue->prev; // Ultimo elemento a ser considerado
    queue_t* proximoElem = queue->next; // Elemento que caminhara pela lista, ja comecando pelo proximo
    
    /* Contagem propriamente dita */
    do {
        cont += 1;
        proximoElem = proximoElem->next;
    } while (proximoElem != ultimoElem); // "proximo" ser igual ao "ultimo" significa ja ter dado a volta, ou lista de elemento unico

    return cont;
}

void queue_print (char *name, queue_t *queue, void print_elem (void*) ) {
    /* Verifica erros */
    // Ve se a fila eh vazia
    if (queue == NULL) {
        return;
    }

    /* Impressao propriamente dita */
    queue_t* ultimoElem = queue->prev; // Ultimo elemento a ser considerado
    queue_t* proximoElem = queue; // Inicia no primeiro da fila dessa vez

    do {
        print_elem(proximoElem);
        proximoElem = proximoElem->next;
    } while (proximoElem != ultimoElem); // "proximo" ser igual ao "ultimo" significa ja ter dado a volta, ou lista de elemento unico
}

int queue_append (queue_t **queue, queue_t *elem) {
    /* Verifica erros */
    // - a fila deve existir
    if (queue == NULL) {
        return -1;
    }
    // - o elemento deve existir
    if (elem == NULL) {
        return -1;
    }
    // - o elemento nao deve estar em outra fila
    if ( (elem->next != NULL) || (elem->prev != NULL) ) {
        return -1;
    }

    /* Insercoes propriamente ditas */
    // Exemplo 2, insercao em fila vazia
    if (queue_size(*queue) == 0) {
        elem->next = elem;
        elem->prev = elem;
        *queue = elem; // "Inicio" da fila aponta para o elemento

        return 0;
    }

    // Exemplo 3, insercao no final, fila NAO vazia
    queue_t *primeiroElem = *queue;
    queue_t *ultimoElem = *queue;

    elem->prev = ultimoElem;     // Elem torna-se o ultimo elemento
    elem->next = primeiroElem;   // O ultimo elemento deve apontar para o inicio da fila
    ultimoElem->next = elem;     // O antigo ultimo elemento deve apontar para o novo ultimo elemento
    primeiroElem->prev = elem;   // O primeiro elemento deve apontar para o novo ultimo elemento

    return 0;
}

int queue_remove (queue_t **queue, queue_t *elem) {
    /* Verifica erros */
    // - a fila deve existir
    if (queue == NULL) {
        return -1;
    }
    // - o elemento deve existir
    if (elem == NULL) {
        return -1;
    }

    /* Caminha ate encontrar o elemento a ser removido */
    queue_t *aux = *queue; // Aux aponta para o inicio da fila
    while (aux != elem) {
        aux = aux->next;
    }

    /* Remocao Propriamente dita */
    // Pega os elementos da fila que devem apontar para uma nova localizacao
    queue_t *auxNext = aux->next;
    queue_t *auxPrev = aux->prev;
    
    auxPrev->next = aux->next; // O elemento anterior, agora, deve apontar para o proximo
    auxNext->prev = aux->prev; // O proximo elemento, agora, deve apontar para o anterior

    // Desvincula o elemento da lista
    elem->next = NULL; 
    elem->prev = NULL;

    return 0;
}