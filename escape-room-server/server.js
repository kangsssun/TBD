const express = require('express');
const http = require('http');
const WebSocket = require('ws');
const net = require('net');

const app = express();
const server = http.createServer(app);
const wss = new WebSocket.Server({ server });

// 1. GM HTML 화면을 띄워주기 위한 정적 파일 서빙
app.use(express.static('public'));

// 2. 서버 메모리에 현재 게임 상태 저장
let gameState = {};
let timerState = { timeRemaining: 45 * 60, running: false };

// 접속된 클라이언트(소켓)들을 관리하기 위한 Map (key: socket, value: 메타데이터)
const clients = new Map();

wss.on('connection', (ws) => {
    console.log('[SYSTEM] 새로운 클라이언트 연결됨');
    
    // 초기 메타데이터 (누군지 아직 모름)
    clients.set(ws, { role: 'unknown', team_id: null });

    ws.on('message', (message) => {
        const data = JSON.parse(message);
        const clientInfo = clients.get(ws);

        // A. 연결 등록 처리
        if (data.type === 'register') {
            clientInfo.role = data.role;
            clientInfo.team_id = data.team_id;
            clientInfo.team_name = data.team_name || null;
            console.log(`[REGISTER] Role: ${data.role}, ${data.team_id ? 'Team: ' + data.team_id : ''}, Name: ${data.team_name || '(미정)'}`);

            // GM 커넥션에게 현재 활성 노드 목록 업데이트 방송
            if (data.role === 'node') {
                if (data.team_id && !gameState[data.team_id]) {
                    gameState[data.team_id] = { mission: 1, progress: 0 };
                }
                broadcastToRole('gm', { type: 'node_connected', team_id: data.team_id, team_name: data.team_name || null });
            }

            // GM이 접속하면 현재까지의 상태를 한 번 보내줌
            if (data.role === 'gm') {
                ws.send(JSON.stringify({ type: 'init_state', state: gameState }));    
                ws.send(JSON.stringify({ type: 'timer_sync', timeRemaining: timerState.timeRemaining, running: timerState.running }));
                
                // 현재 접속 중인 노드 목록도 같이 보내주기
                const activeTeams = getActiveTeams();
                ws.send(JSON.stringify({ type: 'active_nodes', teams: activeTeams }));
            }
            return;
        }

        // B. 노드 상태 업데이트 수신
        if (data.type === 'update_progress' && clientInfo.role === 'node') {
            gameState[data.team_id] = { mission: data.mission, progress: data.progress };
            // 모든 접속자 중 GM에게만 해당 업데이트 내용을 브로드캐스트
            broadcastToRole('gm', data);
            // 모든 노드에게 평균 진행도 동기화
            broadcastProgressSync();
            return;
        }

        // C. 메시지/공지 전송
        if (data.type === 'chat_message') {
            if (data.target === 'ALL') {
                broadcastAll(data); // 모두에게 전송
            } else {
                // 특정 팀에게만 전송 + GM도 볼 수 있게 GM에게도 전송
                sendToTeam(data.target, data);
                broadcastToRole('gm', data);
            }
            return;
        }

        // D. GM 강제 제어 명령
        if (data.type === 'force_clear' && clientInfo.role === 'gm') {
            sendToTeam(data.target_team, data);
            return;
        }

        // E. 타이머 동기화 제어
        if (data.type === 'timer_control' && clientInfo.role === 'gm') {
            if (data.action === 'start') {
                timerState.running = true;
                startServerTimer();
            } else if (data.action === 'pause') {
                timerState.running = false;
                stopServerTimer();
            } else if (data.action === 'reset') {
                timerState.running = false;
                timerState.timeRemaining = 45 * 60;
                stopServerTimer();
            }
            // 노드에도 전달
            broadcastToRole('node', data);
            // GM에게도 현재 시간 동기화
            broadcastToRole('gm', { type: 'timer_sync', timeRemaining: timerState.timeRemaining, running: timerState.running });
            return;
        }
    });

    ws.on('close', () => {
        console.log('[SYSTEM] 클라이언트 연결 종료');
        const info = clients.get(ws);
        if (info && info.role === 'node') {
            broadcastToRole('gm', { type: 'node_disconnected', team_id: info.team_id });
        }
        clients.delete(ws);
    });
});

// --- 활성 팀 목록 가져오기 헬퍼 ---
function getActiveTeams() {
    const teams = [];
    const seen = new Set();
    for (let [client, info] of clients.entries()) {
        if (info.role === 'node' && info.team_id && !seen.has(String(info.team_id))) {
            seen.add(String(info.team_id));
            teams.push({ team_id: info.team_id, team_name: info.team_name || null });
        }
    }
    for (let [client, info] of tcpClients.entries()) {
        if (info.role === 'node' && info.team_id && !seen.has(String(info.team_id))) {
            seen.add(String(info.team_id));
            teams.push({ team_id: info.team_id, team_name: info.team_name || null });
        }
    }
    return teams;
}

// --- 브로드캐스트 헬퍼 함수 ---
function broadcastAll(data) {
    const payload = JSON.stringify(data);
    // WebSocket 브로드캐스트
    for (let client of wss.clients) {
        if (client.readyState === WebSocket.OPEN) {
            client.send(payload);
        }
    }
    // TCP 브로드캐스트
    for (let [client, info] of tcpClients.entries()) {
        client.write(payload + '\n');
    }
}

function broadcastToRole(role, data) {
    const payload = JSON.stringify(data);
    // WebSocket 브로드캐스트
    for (let [client, info] of clients.entries()) {
        if (info.role === role && client.readyState === WebSocket.OPEN) {
            client.send(payload);
        }
    }
    // TCP 브로드캐스트
    for (let [client, info] of tcpClients.entries()) {
        if (info.role === role) {
            client.write(payload + '\n');
        }
    }
}

function sendToTeam(teamId, data) {
    const payload = JSON.stringify(data);
    // WebSocket 브로드캐스트
    for (let [client, info] of clients.entries()) {
        if (info.role === 'node' && String(info.team_id) === String(teamId) && client.readyState === WebSocket.OPEN) {
            client.send(payload);
        }
    }
    // TCP 브로드캐스트
    for (let [client, info] of tcpClients.entries()) {
        if (info.role === 'node' && String(info.team_id) === String(teamId)) {
            client.write(payload + '\n');
        }
    }
}

// --- TCP 소켓 서버 (Qt C++ 내장 소켓용) ---
const tcpClients = new Map();
const tcpServer = net.createServer((socket) => {
    console.log('[SYSTEM] 새로운 TCP 클라이언트 연결됨');
    tcpClients.set(socket, { role: 'unknown', team_id: null });

    let buffer = '';

    socket.on('data', (data) => {
        buffer += data.toString();
        let parts = buffer.split('\n');
        buffer = parts.pop(); // incomplete message

        for (let part of parts) {
            if (!part.trim()) continue;
            try {
                const msg = JSON.parse(part);
                const clientInfo = tcpClients.get(socket);

                if (msg.type === 'register') {
                    clientInfo.role = msg.role;
                    clientInfo.team_id = msg.team_id;
                    clientInfo.team_name = msg.team_name || null;
                    console.log(`[TCP REGISTER] Role: ${msg.role}, Team: ${msg.team_id}, Name: ${msg.team_name || '(미정)'}`);
                    
                    if (msg.role === 'node') {
                        if (msg.team_id && !gameState[msg.team_id]) {
                            gameState[msg.team_id] = { mission: 1, progress: 0 };
                        }
                        broadcastToRole('gm', { type: 'node_connected', team_id: msg.team_id, team_name: msg.team_name || null });
                        // 새로 연결된 노드에게 현재 타이머 동기화
                        socket.write(JSON.stringify({ type: 'timer_sync', timeRemaining: timerState.timeRemaining, running: timerState.running }) + '\n');
                    }
                    continue;
                }

                if (msg.type === 'update_progress' && clientInfo.role === 'node') {
                    gameState[msg.team_id] = { mission: msg.mission, progress: msg.progress };
                    broadcastToRole('gm', msg);
                    broadcastProgressSync();
                    continue;
                }

                if (msg.type === 'chat_message') {
                    if (msg.target === 'ALL') {
                        broadcastAll(msg);
                    } else {
                        sendToTeam(msg.target, msg);
                        broadcastToRole('gm', msg);
                    }
                    continue;
                }
            } catch (err) {
                console.error('[TCP] JSON 파싱 에러:', err);
            }
        }
    });

    socket.on('close', () => {
        console.log('[SYSTEM] TCP 클라이언트 연결 종료');
        const info = tcpClients.get(socket);
        if (info && info.role === 'node') {
            broadcastToRole('gm', { type: 'node_disconnected', team_id: info.team_id });
        }
        tcpClients.delete(socket);
    });

    socket.on('error', (err) => {
        console.error('[SYSTEM] TCP 클라이언트 연결 에러:', err.message);
        const info = tcpClients.get(socket);
        if (info && info.role === 'node') {
            broadcastToRole('gm', { type: 'node_disconnected', team_id: info.team_id });
        }
        tcpClients.delete(socket);
    });
});

// --- 진행도 동기화 ---
function broadcastProgressSync() {
    const teamIds = Object.keys(gameState);
    if (teamIds.length === 0) return;
    const totalProgress = teamIds.reduce((sum, tid) => sum + (gameState[tid].progress || 0), 0);
    const avgProgress = Math.round(totalProgress / teamIds.length);

    // 각 노드에게 자기 진행도 + 평균 진행도 전송
    for (let [client, info] of clients.entries()) {
        if (info.role === 'node' && info.team_id && client.readyState === 1) {
            const myProgress = gameState[info.team_id] ? gameState[info.team_id].progress : 0;
            client.send(JSON.stringify({ type: 'progress_sync', my_progress: myProgress, avg_progress: avgProgress }));
        }
    }
    for (let [client, info] of tcpClients.entries()) {
        if (info.role === 'node' && info.team_id) {
            const myProgress = gameState[info.team_id] ? gameState[info.team_id].progress : 0;
            client.write(JSON.stringify({ type: 'progress_sync', my_progress: myProgress, avg_progress: avgProgress }) + '\n');
        }
    }
}

// --- 서버 타이머 ---
let serverTimerInterval = null;
function startServerTimer() {
    if (serverTimerInterval) return;
    serverTimerInterval = setInterval(() => {
        if (timerState.running && timerState.timeRemaining > 0) {
            timerState.timeRemaining--;
            // 매 초 GM에게 동기화
            broadcastToRole('gm', { type: 'timer_sync', timeRemaining: timerState.timeRemaining, running: true });
            // 10초마다 노드에도 동기화
            if (timerState.timeRemaining % 10 === 0) {
                broadcastToRole('node', { type: 'timer_sync', timeRemaining: timerState.timeRemaining, running: true });
            }
        } else if (timerState.timeRemaining <= 0) {
            timerState.running = false;
            stopServerTimer();
            broadcastAll({ type: 'timer_sync', timeRemaining: 0, running: false });
        }
    }, 1000);
}
function stopServerTimer() {
    if (serverTimerInterval) { clearInterval(serverTimerInterval); serverTimerInterval = null; }
}

// 3. 서버 실행
const PORT = 8080;
const TCP_PORT = 5000;

server.listen(PORT, '0.0.0.0', () => {
    console.log(`[SYSTEM] 방탈출 백엔드 서버(WebSocket/HTTP)가 http://127.0.0.1:${PORT} 에서 실행 중입니다.`);
});

tcpServer.listen(TCP_PORT, '0.0.0.0', () => {
    console.log(`[SYSTEM] 방탈출 TCP 서버(C++ Node용)가 포트 ${TCP_PORT} 에서 대기 중입니다.`);
});
