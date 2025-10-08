# Autoria

GRR: 20240305

Nome: Giuliano Thiago Pinheiro Tavares

# Lista de arquivos
- gbv.c
- gbv.h
- main.c
- makefile
- util.c
- util.h

# Dificuldades enfrentadas

1. No início do trabalho a maior dificuldade foi iniciar-lo, foi necessário eu procurar em foruns e em sites como funcionava a leitura e escrita de arquivos binários em C, estudei e entendi função por função (fopen, fclose, fread, fwrite, etc.) para assim poder começar o trabalho.
2. Problema mais simples, mas outra dificuldade foi entender como a função que converte o valor de `time` para uma data formata.
3. A função mais complexa de implementar, sem dúvida, foi a função de adicionar arquivos quando tratamos da substituição de arquivos de mesmo nome, porém com tamanhos diferentes, sendo necessário ficar "empurrando" para abrir espaço para arquivos igual de tamanho maior e puxar para trás os documentos posteriores para "fechar" o buraco deixado por arquivo igual de tamanho menor. Por outro lado, isso facilitou a remove, pois foi basicamente a lógica da adição de mesmo nome de tamanho menor.
4. Analisar os bytes do arquivo `.gbv` foi um pouco desafiador, pois em certo momento, a função `list` estava mostrando corretamente os metadados dos documentos inclusos, porém quando usado o `hexdump` os bytes dos documentos estavam errados.
5. Organizar o código foi um pouco desafiador também, por ser um trabalho acadêmico, certas partes ficava em dúvida se era necessário incluir ou não, como a checagem dos `fread` e `fwrite`, incluindo a checagem aumentava algumas linhas o código e na minha visão deixava mais feio, mas foi incluído no código final, menos nas funções auxiliares `print_bytes` e `write_bytes`. Comentários foi outra dúvida quanto a organização também.

# Bugs conhecidos
1. Único "bug" conhecido é, se caso a função de `add` ou `remove` falharem no meio do processo, será fechado os arquivos e retornado o código de erro, porém a biblioteca ficará corrompida, pois nenhuma ação de reversão é feita.