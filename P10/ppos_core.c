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
task_t *filaTarefas = NULL, *dormitorio = NULL, tarefaMain, tarefaDispatcher, *tarefaAtual;

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

void comeca_preempcao() {
    // Solucao Atomica sugerida pelo prof
    while (__sync_fetch_and_or (&estaPreemptando, 1));
}

void para_preempcao() {
    estaPreemptando = 0;
}

// Apenas verifica se o semaforo ja nao foi quebrado
int sem_destruido (semaphore_t *semaforo) {
    if ( (semaforo->contador < 0) && (semaforo->filaTarefasSemaforo == NULL) ){
        return 1; // Foi destruido
    }

    return 0; // Tudo Ok
}

void sem_soma_valor(semaphore_t* semaforo, int valor) {
    while(__sync_fetch_and_or ( & (semaforo->travaSemaforo), 1));
    semaforo->contador = (semaforo->contador + valor);
    semaforo->travaSemaforo = 0;
    return;
}

/* P10 */
int sem_create (semaphore_t *s, int value) {
    /* Guardas de verificacao */
    if (s == NULL) {
        fprintf(stderr, "sem_create: Semaforo ja foi criado anteriormente \n");
        return -1;
    }

    if (value < 0) {
        fprintf(stderr, "sem_create: Semaforo iniciado com valor negativo. \n");
        return -1;
    }

    /* Criacao do semaforo em si */
    #ifdef debug
        printf("sem_create: Criando semaforo de valor %d. \n", value)
    #endif
    s->contador = value;
    s->filaTarefasSemaforo = NULL;
    s->travaSemaforo = 0;
    return 0;
}

int sem_down (semaphore_t *s) {
    #ifdef debug
        printf("\n iniciando down \n");
    #endif
    comeca_preempcao();
    if (sem_destruido(s) == 1) {
        para_preempcao();
        return -1;
    }

    // Abaixa semaforo
    sem_soma_valor(s, -1);

    // Verifica se deve travar semaforo
    if (s->contador < 0) {
        // Adiciona tarefaAtual a fila de tarefas suspensas pelo semaforo
        queue_remove((queue_t**) &filaTarefas, (queue_t*) tarefaAtual);
        queue_append((queue_t**) &s->filaTarefasSemaforo, (queue_t*) tarefaAtual);
        task_yield();
    }
    para_preempcao();
    return 0;
}

// Funcao oposta ao sem_down
int sem_up (semaphore_t *s) {
    #ifdef debug
        printf("\n iniciando up \n");
    #endif    
    comeca_preempcao();
    if (sem_destruido(s) == 1) {
        para_preempcao();
        return -1;
    }

    // Levanta semaforo
    sem_soma_valor(s, 1);

    // Verifica se deve liberar alguem travado do semaforo
    if (s->contador < 1) {
        task_t * tarefaAcordando = s->filaTarefasSemaforo;
        // Adiciona tarefaAtual a fila de tarefas suspensas pelo semaforo
        queue_remove((queue_t**) &s->filaTarefasSemaforo, (queue_t*) tarefaAcordando);
        queue_append((queue_t**) &filaTarefas, (queue_t*) tarefaAcordando);
        task_yield();
    }
    para_preempcao();
    return 0;
}

int sem_destroy(semaphore_t *s) {
    comeca_preempcao();
    if (sem_destruido(s) == 1) {
        para_preempcao();
        return -1;
    }

    // Limpa a fila de tarefas que estava esperando
    s->contador = -1;
    while(s->filaTarefasSemaforo != NULL) {
        task_t* tarefaAcordando = s->filaTarefasSemaforo;
        queue_remove((queue_t**) &s->filaTarefasSemaforo, (queue_t*) tarefaAcordando);
        queue_append((queue_t**) &filaTarefas, (queue_t*) tarefaAcordando);
    }
    para_preempcao();
    return 0;
}






















/* P9 */
void task_sleep (int t) {
    // Para evitar preemptacoes indesejadas devido a ser uma operacao critica
    comeca_preempcao();

    // Retira tare
    if (queue_remove((queue_t**) &filaTarefas, (queue_t*) tarefaAtual) == -1) {
        fprintf(stderr, "task_sleep: tarefa %d nao esta na fila de prontas, nao pode dormir. \n", tarefaAtual->id);
        return;
    }

    // Adiciona tarefa ao dormitorio
    queue_append( (queue_t **) &dormitorio, (queue_t*) tarefaAtual);
    tarefaAtual->despertador = systime() + (uint) t; // Descobre a hora de levantar
    tarefaAtual->status = 2; // Define como suspensa
    task_yield();
    return;
}

void despertar_hospede(task_t* tarefaAcordando) {
    // Troca a tarefa de filas
    queue_remove( (queue_t**) &dormitorio, (queue_t *) tarefaAcordando);
    queue_append( (queue_t**) &filaTarefas, (queue_t *) tarefaAcordando);

    tarefaAcordando->status = 0; // define como pronta
    tarefaAcordando->prioridadeDinamica = tarefaAcordando->prioridadeEstatica; // Recomeca sua prioridade
    return;
}

void verifica_dormitorio() {
    if (dormitorio == NULL) {
        return;
    }

    int qntHospedes = queue_size( (queue_t *) dormitorio);
    task_t* tarefaAux = dormitorio;
    for (int i = 0; i < qntHospedes; i++) {
        if(tarefaAux->despertador <= systime()) { // Hora de acordar!
            task_t* tarefaAcordando = tarefaAux;
            tarefaAux = tarefaAux->next; // Redundancia necessaria para evitar erro de ponteiro em queue (ele identifica o elemento em outra fila)
            despertar_hospede(tarefaAcordando);
        } else {
            tarefaAux = tarefaAux->next; 
        }
    }
    return;
}

/* P8 */
// Faz o "join" de uma tarefa a tarefaAtual, carinhosamente chamada aqui de "afiliamento"
int task_join (task_t *task) {
    /* Verificacoes de guarda para as operacoes */
    if (task == NULL) {
        fprintf(stderr, "task_join: Tarefa %d nao pode fazer o Join. \n", tarefaAtual->id);
        return -1;
    }
    #ifdef DEBUG
        printf ("task_join: Tarefa %d fazendo join em %d.\n", tarefaAtual->id, task->id);
    #endif

    if (task->status == -1) {
        return -1;
    }

    comeca_preempcao();

    /* Remocao da fila de prontas */
    if (queue_remove( (queue_t**) &filaTarefas, (queue_t*) tarefaAtual) == -1) {
        fprintf(stderr, "task_join: Tarefa %d nao esta na fila de prontas, join incompleto. \n", task->id);
    }

    /* Suspende a funcao apos sua remocao e coloca na fila de suspensas */
    tarefaAtual->status = 2; // Suspensa
    queue_append( (queue_t **) &(task->tarefasAfiliadas), (queue_t *) tarefaAtual);
    task_yield();
    return task->exit_code;
}

// Libera todas as afiliadas que estao conectadas a tarefa que acabou de morrer
void libera_afiliadas(task_t *filaAfiliadas) {
    // While fixo, nao precisamos caminhar com o ponteiro pois o remove ja atualiza ele para nos
    while(filaAfiliadas != NULL) {
        #ifdef DEBUG
            printf ("libera_afiliadas: Tarefa %d esta liberando a tarefa %d.\n", tarefaAtual->id, filaAfiliadas->id);
        #endif
        task_t* tarefaAux = filaAfiliadas;

        queue_remove( (queue_t **) &filaAfiliadas, (queue_t *) tarefaAux);

        // Deixa a tarefa pronta para ser executada na fila de threads
        tarefaAux->status = 0;
        queue_append( (queue_t **) &filaTarefas, (queue_t *) tarefaAux);
    }
    return;
}


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

    /* Seta para o P8 as tarefas que esta apadrinhando */
    task->tarefasAfiliadas = NULL;

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
    comeca_preempcao();

    #ifdef DEBUG
        printf("task_exit: Terminando tarefa: %d. \n", tarefaAtual->id);
    #endif

    tarefaAtual->status = -1;
    qntTarefasUsuario--;

    /* Tratamento de tempo da tarefa individual */
    tarefaAtual->tempoProcessamento += 20 - tarefaAtual->quantum; // Desconta o que sobraria de tempo
    tarefaAtual->tempoExecucao = systime() - tarefaAtual->tempoExecucao; // Desconta o tempo atual do momento que a tarefa iniciou (em task_create)

    tarefaAtual->exit_code = exit_code;

    libera_afiliadas(tarefaAtual->tarefasAfiliadas);
    
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
    // #ifdef DEBUG
    //     printf("scheduler: Scheduler buscando a tarefa com maior prioridade \n");
    // #endif

    if (filaTarefas == NULL) {
        return NULL;
    }

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

    tarefaAtual->tempoProcessamento += (20 - tarefaAtual->quantum);
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
        verifica_dormitorio();
        tarefaAtualUsuario = scheduler();

        // escalonador escolheu uma tarefa?      
        // se próxima <> NULO então
        if (tarefaAtualUsuario != NULL) {
            tarefaAtualUsuario->quantum = 20; // Quantum definido em 20 por especificacao
            para_preempcao();
            // transfere controle para a próxima tarefa
            // task_switch (próxima)
            tarefaDispatcher.tempoProcessamento += ( systime() - tempProcDispatcherAtual ); // Atualiza tempo do dispatcher
            task_switch(& ( * tarefaAtualUsuario) );
            tempProcDispatcherAtual = systime(); // Mantem o looping de controle de tempo do dispatcher

        /* APOS P9 EH NECESSARIO O SWITCH DENTRO DO IF DA FILA DE PRONTA */

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
        if (tarefaAtualUsuario == NULL) {
            qntTarefasUsuario = 0;
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

    if (tarefaAtual->id != 1) { // Nao preempta cm dispatcher nunca
        if (tarefaAtual->quantum == 0) {
            comeca_preempcao();
            task_yield();
            return;
        }
        tarefaAtual->quantum--;
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
    tarefaMain.quantum = 20; // Necessario adicionar Quantom pois ela recebera yield sem passar pelo dispatcher, uma das armadilhas do P9

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