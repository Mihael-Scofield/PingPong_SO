// Mihael Scofield de Azevedo - GRR20182621 - msa18

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <ucontext.h>

#include "ppos_data.h"
#include "ppos.h"
#include "queue.h"

#define STACKSIZE 64*1024	/* tamanho de pilha das threads */

int qntTarefasUsuario, maiorID;
task_t *filaTarefas, tarefaMain, tarefaDispatcher, *tarefaAtual;

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
    task->id = maiorID;
    maiorID++;
    qntTarefasUsuario++;
    task->status = 0; // Seta tarefa como pronta

    /* Adiciona tarefa a fila de tarefas */
    if (task->id > 1) { // Adiciona apenas se for "task de usuario"
        task->prev = NULL;
        task->next = NULL;
        queue_append( (queue_t **) &filaTarefas, (queue_t *) task);
        #ifdef DEBUG
            printf ("task_create: Numero de tarefas %d na Fila de Prontas.\n",  queue_size ((queue_t*) filaTarefas));
        #endif        
    } else { // Nao eh task de usuario, nao precisa ser adicionada a fila
        qntTarefasUsuario--;
    }

    /* Tratamento de saida */
    #ifdef DEBUG
        printf("task_create: Esta criada a tarefa de ID: %d. \n", task->id);
    #endif
    return task->id;
}

// Termina a tarefa corrente, indicando um valor de status encerramento
void task_exit (int exit_code) {
    #ifdef DEBUG
        printf("task_exit: Terminando tarefa: %d. \n", tarefaAtual->id);
    #endif

    exit_code++; // APENAS PARA EVITAR WARNING

    tarefaAtual->status = -1;
   
    qntTarefasUsuario--;
    task_switch( &(tarefaDispatcher) ); // Agora quem controla a exclusao de tarefas eh o dispatcher
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

// tarefa que faz o controle de quem sera executada pelo Dispatcher em seguida 
// por enquanto esta implementada com o padrao FCFS, devido a fila ser ciclica,
// apenas pegar o proximo a partir do primeiro garante a propriedade de FCFS
task_t* scheduler () {
    #ifdef DEBUG
        printf("scheduler: Scheduler pegando o proximo da fila de tarefas no padrão FCFS. \n");
    #endif
    
    // Pega o proximo da fila
    filaTarefas = filaTarefas->next;

    // Retorna o proximo da fila
    #ifdef DEBUG
        printf("scheduler: Tarefa selecionada: %d. \n", filaTarefas->id);
    #endif    
    return filaTarefas;
}

// Funcao responsavel por matar tarefas,
// nao eh possivel usar a tarefaAtual pois isso pode gerar perca de ponteiro
int task_kill (task_t *tarefaMorrendo) {
    #ifdef DEBUG
        printf ("task_kill: Matando a tarefa %d. \n", tarefaMorrendo->id);
    #endif

    // Remove da fila de tarefas
    if (filaTarefas->prev != NULL) {
        filaTarefas = filaTarefas->prev; // Retorna para garantir a propriedade FCFS
    }        
    int statusRemocao = queue_remove( (queue_t**) &filaTarefas, (queue_t*) tarefaMorrendo);
    
    // Libera a memoria da Stack
    free((tarefaMorrendo->context).uc_stack.ss_sp);

    #ifdef DEBUG
        if (statusRemocao == 0) {
            printf ("task_kill: Tarefa %d esta morta. \n", tarefaMorrendo->id);
        } 
        else {
            printf ("task_kill: Erro ao matar a tarefa: %d. \n", tarefaMorrendo->id);
        }
    #endif

    return statusRemocao;
}

// libera o processador para a próxima tarefa, retornando à fila de tarefas
// prontas ("ready queue")
void task_yield () {
    #ifdef DEBUG
        printf ("task_yield: Yield solicitado pela tarefa %d. \n", tarefaAtual->id);
    #endif

    task_switch( & (tarefaDispatcher) );
    return;
}

void dispatcher_body () {
    #ifdef DEBUG
        printf("dispatcher_body: Inicializando o Dispatcher. \n");
    #endif

    task_t *tarefaAtualUsuario; // Necessario para nao haver perca de ponteiro entre essa e a tarefaAtual global
    tarefaAtualUsuario = filaTarefas;
    task_switch(& ( * tarefaAtualUsuario) ); // Apenas para garantir que o looping comece usando a tarefa inicial
    
    int auxErro = 0;

   // enquanto houverem tarefas de usuário
   // enquanto ( userTasks > 0 )
   while ( qntTarefasUsuario > 0 ) {
      // escolhe a próxima tarefa a executar
      // próxima = scheduler()
      tarefaAtualUsuario = scheduler();

      // escalonador escolheu uma tarefa?      
      // se próxima <> NULO então
      if (tarefaAtualUsuario != NULL) {
        // transfere controle para a próxima tarefa
        // task_switch (próxima)
        task_switch(& ( * tarefaAtualUsuario) );
      }
                 
        // voltando ao dispatcher, trata a tarefa de acordo com seu estado
        //  caso o estado da tarefa "próxima" seja:
        //      -1  = TERMINADA : ...
        //       0  = PRONTA    : ...
        //       2  = SUSPENSA  : ...
        //     (etc)
        //  fim caso
        switch (tarefaAtualUsuario->status) {
            // TERMINADA
            case -1: 
                auxErro = task_kill(tarefaAtualUsuario);
                if (auxErro == -1) {
                    fprintf(stderr, "Erro ao matar a Tarefa %d", tarefaAtualUsuario->id);
                }                
                break;
            }
    }

    // encerra a tarefa dispatcher
    #ifdef DEBUG
        printf("dispatcher_body: Encerrando o Dispatcher. \n");
    #endif    
   
   // task_exit(0);
   task_switch(&tarefaMain); // Garante o retorno para a main, acabando o codigo
}

// Inicializa o sistema operacional; deve ser chamada no inicio do main()
void ppos_init () {
    /* desativa o buffer da saida padrao (stdout), usado pela função printf */
    /* linha necessaria por definicao */
    setvbuf (stdout, 0, _IONBF, 0) ;

    /* Preparacao de variaveis globais do SO */
    tarefaMain.id = 0; 
    tarefaAtual = (task_t *) &(tarefaMain);

    maiorID = 1; // Main seria a ID = 0, enquanto o Dispatcher ID = 1
    qntTarefasUsuario = 0;

    task_create(&tarefaDispatcher, dispatcher_body, NULL);

    return;
}