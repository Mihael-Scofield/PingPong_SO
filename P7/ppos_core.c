// Mihael Scofield de Azevedo - GRR20182621 - msa18

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <ucontext.h>
#include <signal.h>
#include <sys/time.h>

#include "ppos_data.h"
#include "ppos.h"
#include "queue.h"


/* Variaveis de gestao de Tarefas */
#define STACKSIZE 64*1024	/* tamanho de pilha das threads */

int qntTarefasUsuario, maiorID;
task_t *filaTarefas, tarefaMain, tarefaDispatcher, *tarefaAtual;

/* Variaveis de controle de tempo */

// estrutura que define um tratador de sinal (deve ser global ou static)
struct sigaction action ;

// estrutura de inicialização to timer
struct itimerval timer;

int estaPreemptando = -1; // -1 para FALSO, 0 para VERDADEIRO

uint tempoSistema;


/* Utilitarios para entregas especificas */

/* P6 */

void printContabilizacao(task_t* taskParaPrintar) {
    printf (
        "Task %d exit: execution time %d ms, processor time  %d ms, %d activations\n",
        taskParaPrintar->id,
        taskParaPrintar->tempoExecucao,
        taskParaPrintar->tempoProcessamento,
        taskParaPrintar->qntAtivacoes
    );
}

// gerência de tarefas =========================================================

/* P7 */
// Para acomodar as alteracoes necessarias do P7, foi optado por dividir a task_create em duas, para melhor generalizacao
// Essa funcao foca em setar as informacoes de uma tarefa no momento de sua criacao
void task_create_informacoes (task_t *task) {
    #ifdef DEBUG
        printf ("task_create_informacoes: Criando as informacoes iniciais da tarefa.\n");
    #endif

    /* Identificacao de Task */
    task->id = maiorID;
    maiorID++;
    qntTarefasUsuario++;
    task->status = 0; // Seta tarefa como pronta
    task->ehTarefaUsuario = 0; // Por default vem como tarefa de usuario

    /* Definicao de prioridade */
    task->prioridadeDinamica = 0;
    task->prioridadeEstatica = 0;

    /* Setagem de variaveis de controle de tempo */
    task->tempoExecucao = 0;
    task->tempoProcessamento = 0;
    task->qntAtivacoes = 0;

    /* A partir daqui a vida util do processo se inicia */
    task->tempoExecucao = systime(); // Futuramente, sera descontado do proprio systime para descobrir quanto tempo a tarefa teve de vida util

    return;
}

// Essa funcao foca na geracao do contexto de uma tarefa
void task_create_contexto (task_t *task,			        // descritor da nova tarefa
                          void (*start_func)(void *),	// funcao corpo da tarefa
                          void *arg) { 			        // argumentos para a tarefa 
    #ifdef DEBUG
        printf ("task_create_contexto: Gerando as informacoes iniciais de contexto da tarefa.\n");
    #endif
    getcontext( & (task->context) );

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
       fprintf(stderr, "Pilha da task %d apresentou erro. \n", maiorID);
       exit (1);
    }    

    /* Tratamento dos parametros */
    makecontext( & (task->context), (void *) ( * (*start_func) ), 1, arg); // Preparacao do Body da task, como visto em aula

    return;
}

uint systime () {
    return tempoSistema;
}

// Cria uma nova tarefa. Retorna um ID> 0 ou erro.
int task_create (task_t *task,			        // descritor da nova tarefa
                 void (*start_func)(void *),	// funcao corpo da tarefa
                 void *arg) { 			        // argumentos para a tarefa 
    #ifdef DEBUG
        printf ("task_create: Iniciando criacao de nova tarefa. \n");
    #endif

    task_create_contexto(task, start_func, arg);
    task_create_informacoes(task);
   
    /* Adiciona tarefa a fila de tarefas */
    task->prev = NULL;
    task->next = NULL;
    queue_append( (queue_t **) &filaTarefas, (queue_t *) task);
    #ifdef DEBUG
        printf ("task_create: Numero de tarefas %d na Fila de Prontas.\n",  queue_size ((queue_t*) filaTarefas));
    #endif

    /* Tratamento de saida */
    #ifdef DEBUG
        printf("task_create: Esta criada a tarefa de ID: %d. \n", task->id);
    #endif

    return task->id;
}

// Termina a tarefa corrente, indicando um valor de status encerramento
void task_exit (int exit_code) {
    // // Para evitar erros em relacao ao contador na troca de contexto
    // estaPreemptando = 0;

    #ifdef DEBUG
        printf("task_exit: Terminando tarefa: %d. \n", tarefaAtual->id);
    #endif

    exit_code++; // APENAS PARA EVITAR WARNING

    tarefaAtual->status = -1;
    qntTarefasUsuario--;

    /* Tratamento de tempo da tarefa individual */
    tarefaAtual->tempoProcessamento += 20 - tarefaAtual->quantum; // Desconta o que sobraria de tempo
    tarefaAtual->tempoExecucao = systime() - tarefaAtual->tempoExecucao; // Desconta o tempo atual do momento que a tarefa iniciou (em task_create)

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
    // #ifdef DEBUG
    //     printf("task_switch: Troca realizada entre %d ---> %d. \n", tarefaAnterior->id, tarefaAtual->id);
    // #endif
    task->qntAtivacoes++;
    return 0;
}

// retorna o identificador da tarefa corrente (main deve ser 0)
int task_id () {
    return tarefaAtual->id;
}

// tarefa que faz o controle de quem sera executada pelo Dispatcher em seguida 
// Por enquanto usa um sistema basico de prioridade com envelhecimento
// criterio de desempate eh a fila que foi criada no P3. Quem esta mais a frente da fila roda primeiro (devido sua natureza ciclica)
task_t* scheduler () {
    #ifdef DEBUG
        printf("scheduler: Scheduler buscando a tarefa com maior prioridade \n");
    #endif

    /* Caminhando por toda a fila para descobrir a tarefa de maior prioridade */
    int qntTarefasAtivas = queue_size( (queue_t *) filaTarefas); // Pega o tamanho da fila, para saber o quanto caminhar
    
    task_t* tarefaAux = filaTarefas; // Pega a tarefa do momento na fila
    task_t* tarefaPrioritaria = tarefaAux;
    int maiorPrioridade = tarefaAux->prioridadeDinamica; // Note que a maior prioridade = -20, enquanto a meno eh = +20

    // Laco que faz a busca
    for (int i = 1; i <= qntTarefasAtivas; i++) { // Posso comecar em um pois caminharei assim que entrar no for
        tarefaAux = tarefaAux->next;
        if (maiorPrioridade > tarefaAux->prioridadeDinamica) { // Se encontrou a maior prioridade ate entao
            tarefaPrioritaria = tarefaAux;
            maiorPrioridade = tarefaPrioritaria->prioridadeDinamica;
        }
    }
    
    filaTarefas = tarefaPrioritaria; // Aponta a fila para a tarefa escolhida

    /* Caminhando novamente na fila envelhecendo as tarefas */
    // Preparacao de variaveis para o laco
    int novaPrioridade = 0;
    int taskAging = -1; // Por definicao do prof

    // Caminhando pela fila novamente
    tarefaAux = filaTarefas;
    tarefaAux->prioridadeDinamica = tarefaAux->prioridadeEstatica;
    for (int i = 1; i <= qntTarefasAtivas; i++) { // Posso comecar em um pois caminharei assim que entrar no for
        tarefaAux = tarefaAux->next;
        novaPrioridade = tarefaAux->prioridadeDinamica + (taskAging);
        if ( (novaPrioridade >= -20) && (novaPrioridade <= 20) ) { // Garantindo que os padroes UNIX sejam seguidos
            tarefaAux->prioridadeDinamica = novaPrioridade;
        } 
        else {
            tarefaAux->prioridadeDinamica = -20; // Concede prioridade maxima pois ele estrapolou o range para baixo
        }
    }

    // Retorna o proximo da fila
    #ifdef DEBUG
        printf("scheduler: Tarefa selecionada: %d. \n", tarefaPrioritaria->id);
    #endif    
    return filaTarefas;
}

// Funcao responsavel por matar tarefas,
// nao eh possivel usar a tarefaAtual pois isso pode gerar perca de ponteiro
int task_kill (task_t *tarefaMorrendo) {
    #ifdef DEBUG
        printf ("task_kill: Matando a tarefa %d. \n", tarefaMorrendo->id);
    #endif

    // Guarda de seguranca para nao remover o dispatcher, evitando
    if (tarefaMorrendo->id != 1) {
        // Remove da fila de tarefas
        if (filaTarefas->prev != NULL) {
            filaTarefas = filaTarefas->prev; // Retorna para garantir a propriedade FCFS
        }
        queue_remove( (queue_t**) &filaTarefas, (queue_t*) tarefaMorrendo);    
    }
    
    // Libera a memoria da Stack
    free((tarefaMorrendo->context).uc_stack.ss_sp);

    printContabilizacao(tarefaMorrendo);
    return 0;
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
    uint tempProcDispatcherAtual = systime();

    #ifdef DEBUG
        printf("dispatcher_body: Inicializando o Dispatcher. \n");
    #endif

    task_t *tarefaAtualUsuario; // Necessario para nao haver perca de ponteiro entre essa e a tarefaAtual global  
    int auxErro = 0;

    // enquanto houverem tarefas de usuário
    // enquanto ( userTasks > 0 )
    while ( (qntTarefasUsuario > 0) || (tarefaMain.status != -1) ) {
        // escolhe a próxima tarefa a executar
        // próxima = scheduler()
        tarefaAtualUsuario = scheduler();

        // escalonador escolheu uma tarefa?      
        // se próxima <> NULO então
        if (tarefaAtualUsuario != NULL) {
            tarefaAtualUsuario->quantum = 20; // Quantum definido em 20 por especificacao
            estaPreemptando = -1;
            // transfere controle para a próxima tarefa
            // task_switch (próxima)
            tarefaDispatcher.tempoProcessamento += ( systime() - tempProcDispatcherAtual ); // Atualiza tempo do dispatcher
            task_switch(& ( * tarefaAtualUsuario) );
            tempProcDispatcherAtual = systime(); // Mantem o looping de controle de tempo do dispatcher
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
    /* controle de tempo do dispatcher */
    tarefaDispatcher.tempoExecucao = ( systime() - tarefaDispatcher.tempoExecucao );
    tarefaDispatcher.tempoProcessamento += ( systime() - tempProcDispatcherAtual );
    printContabilizacao( & (tarefaDispatcher) );

    /* Tratamento de saida */
    task_kill(&tarefaDispatcher);
    task_switch(&tarefaMain); // Garante o retorno para a main, acabando o codigo
    return;
}

void tratadorTemporizador() {
    tempoSistema++;

    if (tarefaAtual->ehTarefaUsuario == 0) { // Guarda de seguranca, apenas preempta tarefa de usuarios
        if (tarefaAtual->quantum == 0) { // Guarda para previnir preempcoes antes da hora
            if (estaPreemptando == -1) { // Guarda para previnir preempcoes em momentos indesejados
                estaPreemptando = 0;
                tarefaAtual->tempoProcessamento += 20;
                task_yield(); // Retorna controle ao dispatcher
                return;
            }
            return;
        }
        tarefaAtual->quantum--; // Caminha em direcao ao 0
        return;
    }
    return;
}

// define a prioridade estática de uma tarefa (ou a tarefa atual)
void task_setprio (task_t *task, int prio) {
    /* Verificacao de Erros */
    if ( (prio < -20) || (prio > 20) ) {
        fprintf(stderr, "A prioridade passada esta fora das limitacoes permitidas");
        return;
    }

    /* Usuario pede a prioridade da task default, isto eh, task atual */
    if (task == NULL) { 
        #ifdef DEBUG
            printf("task_setprio: Setando a prioridade da tarefa atual ao usuario (ID: %d, prio: %d). \n", tarefaAtual->id, prio);
        #endif

        tarefaAtual->prioridadeEstatica = prio; // calcula a partir daqui os envelhecimentos sempre
        tarefaAtual->prioridadeDinamica = prio;
        return;
    }

    /* Seta prioridade da task passada */
    #ifdef DEBUG
        printf("task_setprio: Setando a prioridade da tarefa escolhido pelo usuario (ID: %d, prio: %d). \n", task->id, prio);
    #endif

    task->prioridadeEstatica = prio;
    task->prioridadeDinamica = prio;
    return;
}

// retorna a prioridade estática de uma tarefa (ou a tarefa atual)
int task_getprio (task_t *task) {
    /* Usuario pede a prioridade da task default, isto eh, task atual */
    if (task == NULL) {
        #ifdef DEBUG
            printf("task_getprio: Retornando a prioridade da tarefa atual (ID: %d) ao usuario. \n", tarefaAtual->id);
        #endif        
        return tarefaAtual->prioridadeEstatica;
    }


    #ifdef DEBUG
        printf("task_getprio: Retornando a prioridade da tarefa solicitada (ID: %d) ao usuario. \n", task->id);
    #endif
    return task->prioridadeEstatica;
}

// Inicializa o sistema operacional; deve ser chamada no inicio do main()
void ppos_init () { 
    #ifdef DEBUG
        printf("ppos_init: Hello World! By: Ping Pong SO / V.p4 - Mihael Scofield de Azevedo");                
    #endif

    /* desativa o buffer da saida padrao (stdout), usado pela função printf */
    /* linha necessaria por definicao */
    setvbuf (stdout, 0, _IONBF, 0) ;

    /* Preparacao de variaveis globais do SO */
    tarefaAtual = (task_t *) &(tarefaMain);
    task_create_informacoes(&tarefaMain);
    tarefaMain.ehTarefaUsuario = 0;

    maiorID = 1; // Main seria a ID = 0, enquanto o Dispatcher ID = 1
    qntTarefasUsuario = 0;

    /* Criacao Dispatcher */
    task_create_contexto(&tarefaDispatcher, dispatcher_body, NULL);
    task_create_informacoes(&tarefaDispatcher);
    tarefaDispatcher.ehTarefaUsuario = -1;

    queue_append((queue_t **) &filaTarefas, (queue_t*) (&tarefaMain));

    /* Temporizador */
    #ifdef DEBUG
        printf ("ppos_init: Iniciando o temporizador do SO.\n");
    #endif

    /* CODIGO RETIRADO DO EXEMPLO PASSADO */

    // registra a ação para o sinal de timer SIGALRM
    action.sa_handler = tratadorTemporizador;
    sigemptyset (&action.sa_mask) ;
    action.sa_flags = 0 ;
    if (sigaction (SIGALRM, &action, 0) < 0)
    {
        perror ("Erro em sigaction: ") ;
        exit (1) ;
    }

    // ajusta valores do temporizador
    timer.it_value.tv_usec = 1000;          // primeiro disparo, em micro-segundos
    timer.it_value.tv_sec  = 0;            // primeiro disparo, em segundos
    timer.it_interval.tv_usec = 1000;      // disparos subsequentes, em micro-segundos
    timer.it_interval.tv_sec  = 0;         // disparos subsequentes, em segundos

    // arma o temporizador ITIMER_REAL (vide man setitimer)
    if (setitimer (ITIMER_REAL, &timer, 0) < 0)
    {
        perror ("Erro em setitimer: ") ;
        exit (1) ;
    }    
    
    /* FIM DO CODIGO RETIRADO DO EXEMPLO PASSADO */

    task_yield();
    return;
}