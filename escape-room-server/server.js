const express = require('express');
const http = require('http');
const WebSocket = require('ws');
const net = require('net');
const fs = require('fs');
const path = require('path');
const jsQR = require('jsqr');
const Jimp = require('jimp');

const app = express();
const server = http.createServer(app);
const wss = new WebSocket.Server({ server });

// 1. GM HTML 화면을 띄워주기 위한 정적 파일 서빙
app.use(express.static('public'));

// 2. 서버 메모리에 현재 게임 상태 저장
let gameState = {};
let timerState = { timeRemaining: 45 * 60, running: false };

// IP 주소 기반 노드 상태 저장 (재연결 시 복원용)
// key: IP주소, value: { team_id, team_name, mission, progress }
const savedNodes = {};

// 연결 순서 기반 팀 ID 자동 할당 카운터
let nextTeamId = 1;

// 접속된 클라이언트(소켓)들을 관리하기 위한 Map (key: socket, value: 메타데이터)
const clients = new Map();

wss.on('connection', (ws, req) => {
    const clientIp = req.socket.remoteAddress || '';
    console.log('[SYSTEM] 새로운 클라이언트 연결됨, IP:', clientIp);
    
    // 초기 메타데이터 (누군지 아직 모름)
    clients.set(ws, { role: 'unknown', team_id: null, ip: clientIp });

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
                const nodeIp = clientInfo.ip;
                // IP 기반 저장 데이터 확인 (재연결 복원)
                if (nodeIp && savedNodes[nodeIp]) {
                    // 팀명이 새로 전달되면 업데이트
                    if (data.team_name) {
                        savedNodes[nodeIp].team_name = data.team_name;
                        clientInfo.team_name = data.team_name;
                    }
                    const saved = savedNodes[nodeIp];
                    clientInfo.team_id = saved.team_id;
                    clientInfo.team_name = saved.team_name;
                    gameState[saved.team_id] = { mission: saved.mission, progress: saved.progress };
                    console.log(`[RESTORE] IP ${nodeIp} → Team ${saved.team_id} (${saved.team_name}), M${saved.mission}, ${saved.progress}%`);
                    // 노드에게 복원 데이터 전송
                    ws.send(JSON.stringify({
                        type: 'restore_state',
                        team_id: saved.team_id,
                        team_name: saved.team_name,
                        mission: saved.mission,
                        progress: saved.progress
                    }));
                    // 복원 후 평균 진행도 동기화
                    setTimeout(() => broadcastProgressSync(), 500);
                } else {
                    // 새 노드 — 서버가 team_id 할당
                    const assignedId = nextTeamId++;
                    clientInfo.team_id = assignedId;
                    gameState[assignedId] = { mission: 1, progress: 0 };
                    if (nodeIp) {
                        savedNodes[nodeIp] = { team_id: assignedId, team_name: data.team_name || null, mission: 1, progress: 0 };
                    }
                    console.log(`[ASSIGN] IP ${nodeIp} → Team ${assignedId}`);
                    // 노드에게 할당된 team_id 알려줌
                    ws.send(JSON.stringify({ type: 'assign_team_id', team_id: assignedId }));
                }
                broadcastToRole('gm', { type: 'node_connected', team_id: clientInfo.team_id, team_name: clientInfo.team_name || null });
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
            // IP 기반 저장 업데이트
            if (clientInfo.ip && savedNodes[clientInfo.ip]) {
                savedNodes[clientInfo.ip].mission = data.mission;
                savedNodes[clientInfo.ip].progress = data.progress;
            }
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

        // F-1. GM 관리자 커맨드
        if (data.type === 'gm_command' && clientInfo.role === 'gm') {
            const tid = String(data.team_id);
            console.log(`[GM_CMD] command=${data.command} team=${tid} args=`, data);

            if (data.command === 'move_mission') {
                const mission = data.mission;
                const progress = (mission - 1) * 20; // mission 1 clear = 20%, mission 2 clear = 40% ...
                gameState[tid] = { mission: mission, progress: progress };
                // savedNodes 업데이트
                for (const ip of Object.keys(savedNodes)) {
                    if (String(savedNodes[ip].team_id) === tid) {
                        savedNodes[ip].mission = mission;
                        savedNodes[ip].progress = progress;
                    }
                }
                // 해당 팀 노드에 강제 미션 이동 명령
                sendToTeam(tid, { type: 'force_move_mission', mission: mission, progress: progress });
                // GM에게 진행도 업데이트 브로드캐스트
                broadcastToRole('gm', { type: 'update_progress', team_id: tid, mission: mission, progress: progress });
                broadcastProgressSync();
            }
            else if (data.command === 'skip_mission') {
                const cur = gameState[tid] ? gameState[tid].mission : 1;
                const nextMission = Math.min(cur + 1, 5);
                const progress = (nextMission - 1) * 20;
                gameState[tid] = { mission: nextMission, progress: progress };
                for (const ip of Object.keys(savedNodes)) {
                    if (String(savedNodes[ip].team_id) === tid) {
                        savedNodes[ip].mission = nextMission;
                        savedNodes[ip].progress = progress;
                    }
                }
                sendToTeam(tid, { type: 'force_move_mission', mission: nextMission, progress: progress });
                broadcastToRole('gm', { type: 'update_progress', team_id: tid, mission: nextMission, progress: progress });
                broadcastProgressSync();
            }
            else if (data.command === 'set_progress') {
                const pct = data.progress;
                const mission = Math.min(5, Math.floor(pct / 20) + 1);
                gameState[tid] = { mission: mission, progress: pct };
                for (const ip of Object.keys(savedNodes)) {
                    if (String(savedNodes[ip].team_id) === tid) {
                        savedNodes[ip].mission = mission;
                        savedNodes[ip].progress = pct;
                    }
                }
                sendToTeam(tid, { type: 'force_set_progress', mission: mission, progress: pct });
                broadcastToRole('gm', { type: 'update_progress', team_id: tid, mission: mission, progress: pct });
                broadcastProgressSync();
            }
            else if (data.command === 'send_hint') {
                sendToTeam(tid, { type: 'chat_message', sender: 'gm', text: '[HINT] ' + (data.hint || '') });
                broadcastToRole('gm', { type: 'chat_message', sender: 'gm', team_id: tid, text: '[HINT→T' + tid + '] ' + (data.hint || '') });
            }
            else if (data.command === 'pause_team') {
                sendToTeam(tid, { type: 'team_pause', paused: true });
            }
            else if (data.command === 'resume_team') {
                sendToTeam(tid, { type: 'team_pause', paused: false });
            }
            return;
        }

        // F. GM이 특정 팀 리셋
        if (data.type === 'force_reset_team' && clientInfo.role === 'gm') {
            const targetTeamId = data.team_id;
            console.log(`[RESET] GM이 팀 ${targetTeamId} 리셋 요청`);
            // savedNodes에서 해당 팀의 IP 저장 데이터 삭제
            for (const ip of Object.keys(savedNodes)) {
                if (String(savedNodes[ip].team_id) === String(targetTeamId)) {
                    delete savedNodes[ip];
                    console.log(`[RESET] IP ${ip} 저장 데이터 삭제됨`);
                }
            }
            // gameState에서도 삭제
            delete gameState[targetTeamId];
            // 해당 팀 노드에 force_reset 전송
            sendToTeam(targetTeamId, { type: 'force_reset' });
            // GM에 알림
            broadcastToRole('gm', { type: 'node_disconnected', team_id: targetTeamId });
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
        try { if (!client.destroyed) client.write(payload + '\n'); } catch (e) {}
    }
}

function broadcastToRole(role, data) {
    const payload = JSON.stringify(data);
    for (let [client, info] of clients.entries()) {
        if (info.role === role && client.readyState === WebSocket.OPEN) {
            client.send(payload);
        }
    }
    for (let [client, info] of tcpClients.entries()) {
        if (info.role === role) {
            try { if (!client.destroyed) client.write(payload + '\n'); } catch (e) {}
        }
    }
}

function sendToTeam(teamId, data) {
    const payload = JSON.stringify(data);
    for (let [client, info] of clients.entries()) {
        if (info.role === 'node' && String(info.team_id) === String(teamId) && client.readyState === WebSocket.OPEN) {
            client.send(payload);
        }
    }
    for (let [client, info] of tcpClients.entries()) {
        if (info.role === 'node' && String(info.team_id) === String(teamId)) {
            try { if (!client.destroyed) client.write(payload + '\n'); } catch (e) {}
        }
    }
}

// --- TCP 소켓 서버 (Qt C++ 내장 소켓용) ---
const tcpClients = new Map();
const tcpServer = net.createServer((socket) => {
    const clientIp = socket.remoteAddress || '';
    console.log('[SYSTEM] 새로운 TCP 클라이언트 연결됨, IP:', clientIp);
    tcpClients.set(socket, { role: 'unknown', team_id: null, ip: clientIp });

    let buffer = '';

    socket.on('data', async (data) => {
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
                    console.log(`[TCP REGISTER] Role: ${msg.role}, Team: ${msg.team_id}, Name: ${msg.team_name || '(미정)'}, IP: ${clientIp}`);
                    
                    if (msg.role === 'node') {
                        const nodeIp = clientInfo.ip;
                        // IP 기반 저장 데이터 확인 (재연결 복원)
                        if (nodeIp && savedNodes[nodeIp]) {
                            // 팀명이 새로 전달되면 업데이트 (재등록 시)
                            if (msg.team_name) {
                                savedNodes[nodeIp].team_name = msg.team_name;
                                clientInfo.team_name = msg.team_name;
                            }
                            const saved = savedNodes[nodeIp];
                            clientInfo.team_id = saved.team_id;
                            clientInfo.team_name = saved.team_name;
                            gameState[saved.team_id] = { mission: saved.mission, progress: saved.progress };
                            console.log(`[TCP RESTORE] IP ${nodeIp} → Team ${saved.team_id} (${saved.team_name}), M${saved.mission}, ${saved.progress}%`);
                            socket.write(JSON.stringify({
                                type: 'restore_state',
                                team_id: saved.team_id,
                                team_name: saved.team_name,
                                mission: saved.mission,
                                progress: saved.progress
                            }) + '\n');
                            // 복원 후 평균 진행도 동기화
                            setTimeout(() => broadcastProgressSync(), 500);
                        } else {
                            // 새 노드 — 서버가 team_id 할당
                            const assignedId = nextTeamId++;
                            clientInfo.team_id = assignedId;
                            gameState[assignedId] = { mission: 1, progress: 0 };
                            if (nodeIp) {
                                savedNodes[nodeIp] = { team_id: assignedId, team_name: msg.team_name || null, mission: 1, progress: 0 };
                            }
                            console.log(`[TCP ASSIGN] IP ${nodeIp} → Team ${assignedId}`);
                            // 노드에게 할당된 team_id 알려줌
                            socket.write(JSON.stringify({ type: 'assign_team_id', team_id: assignedId }) + '\n');
                        }
                        broadcastToRole('gm', { type: 'node_connected', team_id: clientInfo.team_id, team_name: clientInfo.team_name || null });
                        // 새로 연결된 노드에게 현재 타이머 동기화
                        socket.write(JSON.stringify({ type: 'timer_sync', timeRemaining: timerState.timeRemaining, running: timerState.running }) + '\n');
                    }
                    continue;
                }

                if (msg.type === 'update_progress' && clientInfo.role === 'node') {
                    gameState[msg.team_id] = { mission: msg.mission, progress: msg.progress };
                    if (clientInfo.ip && savedNodes[clientInfo.ip]) {
                        savedNodes[clientInfo.ip].mission = msg.mission;
                        savedNodes[clientInfo.ip].progress = msg.progress;
                    }
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

                if (msg.type === 'qr_decode_request') {
                    // 팀별 QR 중복 요청 방지 (2초 쿨다운)
                    const qrKey = `qr_${clientInfo.team_id}`;
                    const now = Date.now();
                    if (global[qrKey] && now - global[qrKey] < 2000) {
                        console.log(`[QR] Team ${clientInfo.team_id} 중복 요청 무시 (${now - global[qrKey]}ms)`);
                        continue;
                    }
                    global[qrKey] = now;

                    const imageBase64 = msg.image;
                    const QR_CORRECT_ANSWER = 'https://m.site.naver.com/263Ew';
                    let qrResult = '';
                    let qrStatus = 'invalid'; // 'invalid' | 'wrong' | 'correct'
                    try {
                        const imgBuf = Buffer.from(imageBase64, 'base64');
                        const image = await Jimp.read(imgBuf);
                        const { width, height, data } = image.bitmap;
                        const code = jsQR(new Uint8ClampedArray(data), width, height);
                        if (code && code.data) {
                            qrResult = code.data;
                            if (qrResult === QR_CORRECT_ANSWER) {
                                qrStatus = 'correct';
                            } else {
                                qrStatus = 'wrong';
                            }
                        } else {
                            qrStatus = 'invalid';
                            qrResult = '';
                        }
                    } catch (e) {
                        console.error('[QR] 디코드 에러:', e.message);
                        qrStatus = 'invalid';
                    }

                    // 결과를 노드에 반환
                    socket.write(JSON.stringify({ type: 'qr_decode_result', status: qrStatus, result: qrResult }) + '\n');

                    // GM 로그 메시지 생성
                    const teamLabel = `T${msg.team_id || '?'}`;
                    let logText = '';
                    if (qrStatus === 'correct') {
                        logText = `[${teamLabel}] ✅ QR 정답! → ${qrResult}`;
                    } else if (qrStatus === 'wrong') {
                        logText = `[${teamLabel}] ❌ QR 오답 → ${qrResult}`;
                    } else {
                        logText = `[${teamLabel}] ⚠ 유효하지 않은 QR (디코드 실패)`;
                    }
                    console.log('[QR]', logText);
                    broadcastToRole('gm', {
                        type: 'chat_message', sender: 'system',
                        team_id: msg.team_id, team_name: msg.team_name || '',
                        text: logText
                    });
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
            try {
                if (!client.destroyed) {
                    client.write(JSON.stringify({ type: 'progress_sync', my_progress: myProgress, avg_progress: avgProgress }) + '\n');
                }
            } catch (e) { /* socket already closed */ }
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
