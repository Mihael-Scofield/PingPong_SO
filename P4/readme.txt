// Mihael Scofield de Azevedo - GRR20182621 - msa18

Para executar basta rodar "$make", opções "$make debug", "$make clean" e "$make purge" 
também estão disponíveis.

Após executar o Make o programa estará disponível em ./pingpong-scheduler

Durante a criação do P3 tive o problema de que ao matar as tarefas,
meu código estava pulando da tarefa 1, para a 3, para a 5, então indo para a 2 e por fim a 4
ao invés de seguir a tradicional 1, 2, 3, 4, 5. 
Para corrigir essa peculiaridade eu retornei "um" espaço antes de remover o quem eu gostaria (linha 183).

Para resolver o problema do critério de desempate em P4, preferi manter a minha fila de tasks,
quem está mais perto do início vence em caso de empate.