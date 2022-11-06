// Mihael Scofield de Azevedo - GRR20182621 - msa18

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <ucontext.h>
#include "ppos_data.h"
#include "ppos.h"

#define STACKSIZE 64*1024	/* tamanho de pilha das threads */

task_t Main, *tarefaAtual;
int numTask;

// Inicializa o sistema operacional; deve ser chamada no inicio do main()
void ppos_init () {
    /* desativa o buffer da saida padrao (stdout), usado pela função printf */
    setvbuf (stdout, 0, _IONBF, 0) ;

    /* Preparacao de variaveis globais do SO */
    numTask = 0;
    numTask++;
    Main.id = 0;
    tarefaAtual = (task_t *) &(Main);

    return;
}

// gerência de tarefas =========================================================

// Cria uma nova tarefa. Retorna um ID> 0 ou erro.
int task_create (task_t *task,			        // descritor da nova tarefa
                 void (*start_func)(void *),	// funcao corpo da tarefa
                 void *arg) { 			        // argumentos para a tarefa 
    #ifdef DEBUG
        printf ("task_create: Iniciando criacao de nova tarefa. \n");
    #endif

    /* "Ajeitando" a nova task dentro da Stack */
    char *stack; // Posso recriar a stack pois o ponteiro dela estara na task atual
    getcontext( & (task->context) );

    stack = malloc (STACKSIZE) ;
    if (stack) {
       (task->context).uc_stack.ss_sp = stack ;
       (task->context).uc_stack.ss_size = STACKSIZE ;
       (task->context).uc_stack.ss_flags = 0 ;
       (task->context).uc_link = 0 ;
    }
    else {
       perror ("Erro na criação da pilha: ") ;
       exit (1) ;
    }    


    /* Tratamento dos parametros */
    getcontext (&(task->context));
    makecontext ( &(task->context), (void *) (*(*start_func)), 1, arg ); // Preparacao do Body da task
    
    /* Identificacao de Task */
    task->id = numTask;
    numTask++;

    /* Tratamento de saida */
    #ifdef DEBUG
        printf("task_create: Criada a tarefa: %d. \n", task->id);
    #endif
    return task->id;
}

// Termina a tarefa corrente, indicando um valor de status encerramento
void task_exit (int exit_code) {
    #ifdef DEBUG
        printf("task_exit: Terminando tarefa: %d. \n", tarefaAtual->id);
    #endif 

    exit_code++; // APENAS PARA EVITAR WARNING

    task_switch(&(Main));
    return;
}

// alterna a execução para a tarefa indicada
int task_switch (task_t *task) {
    /* Preparacao de tarefas a trocar */
    task_t *tarefaAnterior = tarefaAtual; 
    tarefaAtual = ( & (* task));

    #ifdef DEBUG
        printf("task_switch: Trocando as seguintes tarefas: %d ---> %d. \n", tarefaAnterior->id, tarefaAtual->id);
    #endif 

    /* Troca propriamente dita */
    int erro;
    erro = swapcontext ( ( & (tarefaAnterior->context)), ( & (task->context)) );
    if (erro == -1) {
        fprintf(stderr, "task_switch: Erro na troca de contexto. \n");
        return -1;
    }

    /* Tratamento de saida */
    #ifdef DEBUG
        printf("task_switch: Troca realizada entre %d ---> %d. \n", tarefaAnterior->id, tarefaAtual->id);
    #endif
    return 0;
}

// retorna o identificador da tarefa corrente (main deve ser 0)
int task_id () {
    return tarefaAtual->id;
}
