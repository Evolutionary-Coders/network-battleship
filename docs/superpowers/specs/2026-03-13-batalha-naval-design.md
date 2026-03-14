# Batalha Naval em Rede — Design

## Visão Geral

Jogo de batalha naval distribuído em C para a disciplina de Sistemas Operacionais (IFSC). O sistema é composto por processos navio independentes, um miniwebserver central, uma página HTML de visualização e um processo atacante. Toda comunicação local usa FIFO; comunicação entre equipes usa HTTP.

## Tabuleiro

- Grade 10x10 (linhas 0-9, colunas 0-9)
- 6 navios distribuídos em 6 das 10 linhas (4 linhas ficam vazias)
- Alocação de linhas feita pelo miniwebserver na inicialização

## Navios

| Tipo | Quantidade | Pontos |
|------|-----------|--------|
| porta_avioes | 1 | 5 |
| submarino | 2 | 3 |
| fragata | 3 | 2 |

Cada navio é um processo independente criado com `fork()` pelo miniwebserver. Possui:
- ID único (0-5)
- Tipo
- Coluna atual (variável, reportada via FIFO)

O navio não sabe sua linha — o miniwebserver é quem atribui.

### Movimento

- Aleatório: a cada ciclo (mínimo 5s), o navio move +1 ou -1 coluna (`rand() % 2`)
- Tratamento de borda: se está na coluna 0, só pode ir para 1; se está na 9, só pode ir para 8
- Só escreve na FIFO quando a coluna muda

## FIFO

- Uma única FIFO compartilhada, criada pelo miniwebserver (ex: `/tmp/batalha_naval_fifo`)
- Formato da mensagem: texto `"id:coluna\n"` (ex: `"3:7\n"`)
- Parsing no servidor: `sscanf(buf, "%d:%d", &id, &col)`
- Navios abrem a FIFO para escrita; miniwebserver abre para leitura

## Miniwebserver

### Responsabilidades

1. Criar a FIFO e os 6 processos navio via `fork()`
2. Loop principal com `select()` monitorando: socket HTTP + fd da FIFO
3. Manter estado do jogo: posição dos navios, histórico de tiros, pontuação
4. Responder requisições HTTP
5. Arbitrar tiros recebidos (hit/miss/repetido)
6. Matar processo navio com `kill()` em caso de acerto

### Endpoints HTTP

#### `GET /tiro?linha=L&coluna=C`
Recebe ataque de outra equipe. Lógica:
1. Posição já atacada → `{"resultado":"repetido"}`
2. Navio presente na posição → `{"resultado":"acerto","tipo":"<tipo>","pontos":<pts>}` + registra atacante + `kill()` no navio
3. Sem navio → `{"resultado":"agua"}`

#### `GET /status`
Informação pública. Não revela colunas.
```json
{
  "linhas": [
    {"linha":2, "tipo":"porta_avioes"},
    {"linha":5, "tipo":"submarino"}
  ],
  "quantidade": {
    "porta_avioes": 1,
    "submarinos": 2,
    "fragatas": 3
  }
}
```

#### `GET /estado_local`
Informação completa para a página HTML.
```json
{
  "navios": [
    {"id":0, "tipo":"porta_avioes", "linha":2, "coluna":7, "vivo":true}
  ],
  "tiros": [
    {"linha":3, "coluna":5, "resultado":"agua", "atacante":"10.0.0.5"}
  ],
  "pontuacao_sofrida": 8
}
```

#### `GET /` (ou `GET /index.html`)
Serve o arquivo `index.html` do disco.

### Porta

8080 (conforme exemplos do PDF).

## Página HTML

- Arquivo `index.html` servido pelo miniwebserver (lido do disco)
- JS mínimo: `fetch("/estado_local")` em intervalo regular, atualiza o DOM
- Exibe: grade 10x10 com navios, histórico de tiros recebidos, pontuação sofrida
- Sem lógica de jogo — apenas visualização

## Processo Atacante

- Binário separado (`atacante`)
- Lê IPs alvo de `alvos.txt` (um IP por linha)
- Para cada equipe inimiga:
  1. Consulta `GET /status` para obter linhas com navios
  2. Prioriza por pontos: porta_avioes (5) → submarino (3) → fragata (2)
  3. Varre todas as 10 colunas de cada linha com navio via `GET /tiro?linha=L&coluna=C`
- Limite total: 100 tiros
- Pode usar `fork()` para atacar equipes em paralelo
- Ao final, gera relatório: navios afundados (geral e por IP), pontuação total, tiros usados

## Estrutura de Arquivos

```
batalha_naval/
├── Makefile
├── index.html
├── alvos.txt
├── src/
│   ├── main.c              # Entry point do miniwebserver
│   ├── navio.c / navio.h   # Lógica do processo navio
│   ├── servidor.c / servidor.h  # Loop select(), dispatch de requests
│   ├── http.c / http.h     # Parsing HTTP e envio de respostas
│   ├── json.c / json.h     # Geração de JSON
│   ├── jogo.c / jogo.h     # Estado do jogo (navios, tiros, pontuação)
│   ├── fifo.c / fifo.h     # Criação e leitura da FIFO
│   └── atacante.c          # Entry point do processo atacante
└── docs/
```

## Fluxo de Dados

```
Navio (fork'd)                    Miniwebserver                     Equipe remota
     │                                 │                                 │
     │── "3:7\n" via FIFO ──────────►  │                                 │
     │                                 │◄── GET /tiro?linha=3&coluna=7 ──│
     │                                 │── {"resultado":"acerto",...} ──► │
     │◄── kill(pid, SIGTERM) ────────  │                                 │
     │                                 │                                 │
     │                          Browser (local)                          │
     │                                 │                                 │
     │                                 │◄── GET /estado_local            │
     │                                 │── JSON estado completo ──►      │
```
