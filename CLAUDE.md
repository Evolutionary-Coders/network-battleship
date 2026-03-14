# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Distributed Battleship game ("Batalha Naval em Rede") for an Operating Systems course at IFSC. **Everything is written in C** (including HTTP handling and JSON generation — no external libraries). A página HTML usa JS mínimo apenas para polling (`fetch()` + atualização do DOM) — toda lógica fica no C. The focus is on OS concepts: process creation (`fork`), IPC via named pipes (FIFO), concurrency with `select()`, and network communication via a custom HTTP server.

## Build & Run

```bash
# Build (once a Makefile exists)
make

# Run the miniwebserver (spawns ship processes automatically)
./miniwebserver

# Test endpoints
curl http://127.0.0.1:8080/estado_local
curl http://127.0.0.1:8080/status
curl "http://127.0.0.1:8080/tiro?linha=3&coluna=5"

# Run the attacker process
./atacante
```

## Architecture

The system has four main components that communicate as follows:

```
Ship processes (fork'd children)
        │  write position updates
        ▼
  Named FIFO pipe ──► Miniwebserver (select() loop)
                            │
              ┌─────────────┼─────────────┐
              ▼             ▼             ▼
        JSON state    HTTP endpoints   Attack arbitration
              │        /estado_local      (kill ship on hit)
              ▼        /status
        HTML page      /tiro?linha=L&coluna=C
        (fetch polling)
```

### Ship processes (`processos navio`)
- Created via `fork()` at miniwebserver startup
- Each ship: unique ID, type, fixed row, moving column
- Types: 1 porta-avioes (5pts), 2 submarinos (3pts each), 3 fragatas (2pts each)
- Ships move along their row (contiguous columns), staying at least 5s per column
- Communicate column position to miniwebserver via a shared named FIFO

### Miniwebserver
- Central component; written in C with raw sockets (no external HTTP libraries)
- Uses `select()` to multiplex between the FIFO (ship updates) and HTTP socket
- Maintains authoritative game state (ship positions, attack history)
- HTTP endpoints return JSON; `/status` must NOT reveal exact ship columns
- On a hit (`/tiro`): records attacker IP, timestamp, position, ship type; kills the ship process with `kill()`

### HTML page
- Visualization only — no game logic
- Polls `/estado_local` via `fetch()` at regular intervals
- Shows: board grid, ship positions, received shots history, score

### Attacker process
- Separate binary that reads target IPs from a file
- Queries `/status` on enemy servers, then fires via `/tiro`
- Max 100 shots total; produces a report (ships sunk per IP, total score, shots used)
- May use `fork()` for parallel attacks

## Code Organization

- **KISS (Keep It Simple, Stupid)** — sempre preferir a solução mais simples que funcione
- Arquivos pequenos e focados — cada arquivo deve ter uma responsabilidade clara
- Separar em módulos: HTTP parsing, JSON generation, game state, FIFO handling, ship logic, attacker logic
- Headers (`.h`) para interfaces públicas, implementação (`.c`) separada
- Evitar arquivos monolíticos; preferir composição de módulos pequenos

## Key Constraints

- All IPC between ships and miniwebserver must use named pipes (FIFO)
- The miniwebserver must use `select()` for I/O multiplexing
- Ship processes must be independent processes created with `fork()`, not threads
- JSON responses must match the exact format specified in the project spec (see `MiniprojetoBatalhaNavalEmRede.pdf`)
- The `/status` endpoint must never expose ship column positions
- **Todo o projeto deve ser implementado em C** (servidor, navios, atacante, geração de JSON, serving de HTML). Não usar bibliotecas externas para HTTP ou JSON.
