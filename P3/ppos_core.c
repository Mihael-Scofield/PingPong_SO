// Mihael Scofield de Azevedo - GRR20182621 - msa18

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <ucontext.h>
#include "ppos_data.h"
#include "ppos.h"

#define STACKSIZE 64*1024	/* tamanho de pilha das threads */

int numTask;
task_t Main, *tarefaAtual;

// Inicializa o sistema operacional; deve ser chamada no inicio do main()
void ppos_init () {
    /* desativa o buffer da saida padrao (stdout), usado pela função printf */
    /* linha necessaria por definicao */
    setvbuf (stdout, 0, _IONBF, 0) ;

    /* Preparacao de variaveis globais do SO */
    Main.id = 0;
    numTask = 1;    
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
    /* linhas necessarias como visto em aula   */
    char *stack; // Posso recriar a stack pois o ponteiro dela estara na task atual
    stack = malloc (STACKSIZE);
    if (stack) {
       (task->context).uc_stack.ss_sp = stack;
       (task->context).uc_stack.ss_size = STACKSIZE;
       (task->context).uc_stack.ss_flags = 0;
       (task->context).uc_link = 0;
    }
    else {
       perror ("Erro na criação da pilha. \n") ;
       exit (1);
    }    

    /* Tratamento dos parametros */
    getcontext( & (task->context) );
    makecontext( & (task->context), (void *) ( * (*start_func) ), 1, arg); // Preparacao do Body da task, como visto em aula
    
    /* Identificacao de Task */
    task->id = numTask;
    numTask++;

    /* Tratamento de saida */
    #ifdef DEBUG
        printf("task_create: Esta criada a tarefa de ID: %d. \n", task->id);
    #endif
    return task->id;
}

// Termina a tarefa corrente, indicando um valor de status encerramento
void task_exit (int exit_code) {
    exit_code++; // APENAS PARA EVITAR WARNING, VARIAVEL SERA USADA NO FUTURO

    #ifdef DEBUG
        printf("task_exit: Terminando tarefa: %d. \n", tarefaAtual->id);
    #endif

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
    int error;
    error = swapcontext ( ( & (tarefaAnterior->context)), ( & (task->context)) );
    if (error == -1) {
        perror("task_switch: Erro na troca de contexto. \n");
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


/*

função dispatcher
início
   // enquanto houverem tarefas de usuário
   enquanto ( userTasks > 0 )

      // escolhe a próxima tarefa a executar
      próxima = scheduler()

      // escalonador escolheu uma tarefa?      
      se próxima <> NULO então

         // transfere controle para a próxima tarefa
         task_switch (próxima)
         
         // voltando ao dispatcher, trata a tarefa de acordo com seu estado
         caso o estado da tarefa "próxima" seja:
            PRONTA    : ...
            TERMINADA : ...
            SUSPENSA  : ...
            (etc)
         fim caso         

      fim se

   fim enquanto

   // encerra a tarefa dispatcher
   task_exit(0)
fim

*/