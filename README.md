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

## Compilação

```bash
make
```

Gera dois binários: `miniwebserver` e `atacante`.

Para limpar:

```bash
make clean
```

## Executando o servidor

```bash
./miniwebserver
```

O servidor inicia na porta 8080, cria a FIFO em `/tmp/batalha_naval_fifo` e faz fork de 6 processos navio automaticamente. Para encerrar, pressione `Ctrl+C` (os processos navio são finalizados junto).

## Visualizando o tabuleiro

Abra no navegador:

```
http://127.0.0.1:8080/
```

A página atualiza automaticamente a cada 2 segundos, mostrando a posição dos navios, tiros recebidos e pontuação.

## Endpoints HTTP

### Estado local (uso interno da página)

```bash
curl http://127.0.0.1:8080/estado_local
```

### Status público (visível para outras equipes)

```bash
curl http://127.0.0.1:8080/status
```

Retorna linhas com navios ativos e contagem por tipo. Não revela a coluna dos navios.

### Disparar tiro

```bash
curl "http://127.0.0.1:8080/tiro?linha=3&coluna=5"
```

Respostas possíveis:
- `{"resultado":"agua"}` — sem navio na posição
- `{"resultado":"acerto","tipo":"fragata","pontos":2}` — navio atingido
- `{"resultado":"repetido"}` — posição já atacada anteriormente

## Atacando outras equipes

1. Edite `alvos.txt` com os IPs das equipes alvo (um por linha):

```
10.0.0.12
10.0.0.15
```

2. Execute o atacante:

```bash
./atacante
```

O atacante consulta o `/status` de cada alvo, prioriza navios de maior valor e varre as colunas. Ao final exibe um relatório com acertos, pontuação e tiros usados. Limite: 100 tiros no total.
