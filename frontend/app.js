var ROWS = 10;
var COLS = 10;

function buildBoard() {
    var board = document.getElementById('board');
    var html = '<tr><th></th>';
    for (var c = 0; c < COLS; c++) html += '<th>' + c + '</th>';
    html += '</tr>';
    for (var r = 0; r < ROWS; r++) {
        html += '<tr><th>' + r + '</th>';
        for (var c = 0; c < COLS; c++) {
            html += '<td id="cell-' + r + '-' + c + '" class="water"></td>';
        }
        html += '</tr>';
    }
    board.innerHTML = html;
}

function updateBoard(state) {
    /* Reset all cells */
    for (var r = 0; r < ROWS; r++) {
        for (var c = 0; c < COLS; c++) {
            var cell = document.getElementById('cell-' + r + '-' + c);
            cell.className = 'water';
            cell.textContent = '';
        }
    }

    /* Draw ships */
    for (var i = 0; i < state.navios.length; i++) {
        var ship = state.navios[i];
        if (ship.vivo) {
            var cell = document.getElementById('cell-' + ship.linha + '-' + ship.coluna);
            cell.className = 'ship-' + ship.tipo;
            cell.textContent = ship.tipo.charAt(0).toUpperCase();
        }
    }

    /* Draw shots */
    for (var i = 0; i < state.tiros.length; i++) {
        var shot = state.tiros[i];
        var cell = document.getElementById('cell-' + shot.linha + '-' + shot.coluna);
        if (shot.resultado === 'acerto') {
            cell.className = 'shot-acerto';
            cell.textContent = 'X';
        } else if (shot.resultado === 'agua') {
            cell.className = 'shot-agua';
            cell.textContent = 'O';
        }
    }

    /* Ship list */
    var list = document.getElementById('ship-list');
    var html = '';
    for (var i = 0; i < state.navios.length; i++) {
        var ship = state.navios[i];
        var cls = ship.vivo ? ' class="alive"' : ' class="dead"';
        var status = ship.vivo ? 'ativo' : 'destruido';
        html += '<li' + cls + '>' + ship.tipo + ' (linha ' + ship.linha + ') - ' + status + '</li>';
    }
    list.innerHTML = html;

    /* Score */
    document.getElementById('score').textContent = state.pontuacao_sofrida;

    /* Shot log */
    var tbody = document.querySelector('#shot-log tbody');
    html = '';
    for (var i = state.tiros.length - 1; i >= 0; i--) {
        var shot = state.tiros[i];
        html += '<tr><td>' + shot.linha + '</td><td>' + shot.coluna + '</td><td>' + shot.resultado + '</td><td>' + shot.atacante + '</td></tr>';
    }
    tbody.innerHTML = html;
}

function fetchState() {
    fetch('/estado_local')
        .then(function(res) { return res.json(); })
        .then(function(state) { updateBoard(state); })
        .catch(function(err) { console.error('Fetch error:', err); });
}

buildBoard();
fetchState();
setInterval(fetchState, 2000);
