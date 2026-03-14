# Batalha Naval em Rede

Miniprojeto da disciplina de Sistemas Operacionais 

Curso de Tecnologia em ADS, IFSC Campus São José.

Professor: Eraldo Silveira e Silva

## Grupo

- Luís Renato Freitas
- Nicolas Arthur

## Descrição

Implementação de um jogo de batalha naval distribuído em C, com foco em conceitos de sistemas operacionais: criação de processos (`fork`), comunicação entre processos via FIFO (named pipe), multiplexação de I/O (`select`) e comunicação em rede via HTTP.

## Componentes

- **Miniwebserver** — servidor HTTP em C que gerencia o estado do jogo, cria os processos navio e responde requisições locais e remotas
- **Processos navio** — processos independentes que se movimentam no tabuleiro e reportam posição via FIFO
- **Página HTML** — interface de visualização do tabuleiro com atualização automática via fetch
- **Processo atacante** — realiza ataques a outras equipes via HTTP
