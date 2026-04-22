const express = require('express');
const http = require('http');
const WebSocket = require('ws');
const net = require('net');
const fs = require('fs');
const path = require('path');
const jsQR = require('jsqr');
const Jimp = require('jimp');
const { exec, spawn } = require('child_process');

const app = express();
const server = http.createServer(app);
const wss = new WebSocket.Server({ server });

// 1. GM HTML 화면을 띄워주기 위한 정적 파일 서빙
app.use(express.static('public'));
app.use('/ending', express.static(path.resolve(__dirname, '..', 'ending')));

// ── BGM 재생 시스템 ──────────────────────────────────────────────────────
let bgmProcess = null;
let bgmPlaying = false;
const bgmFile = path.resolve(__dirname, '..', 'songs', 'title.wav');

function startBgm() {
    if (bgmPlaying && bgmProcess) return;
    if (!fs.existsSync(bgmFile)) {
        console.log('[BGM] 파일 없음:', bgmFile);
        return;
    }
    const platform = process.platform;
    const startLoop = () => {
        if (!bgmPlaying) return;
        let proc;
        if (platform === 'win32') {
            // Windows: PowerShell로 Media.SoundPlayer 사용
            proc = spawn('powershell', ['-Command',
                `$p = New-Object System.Media.SoundPlayer '${bgmFile.replace(/'/g, "''")}'; $p.PlaySync()`
            ], { stdio: 'ignore' });
        } else {
            // Linux: aplay 사용
            proc = spawn('aplay', [bgmFile], { stdio: 'ignore' });
        }
        bgmProcess = proc;
        proc.on('close', () => {
            if (bgmPlaying) startLoop(); // 루프 재생
        });
        proc.on('error', (err) => {
            console.error('[BGM] 재생 에러:', err.message);
            bgmPlaying = false;
        });
    };
    bgmPlaying = true;
    startLoop();
    console.log('[BGM] 재생 시작:', bgmFile);
}

function stopBgm() {
    bgmPlaying = false;
    if (bgmProcess) {
        bgmProcess.kill();
        bgmProcess = null;
    }
    console.log('[BGM] 재생 중지');
}

// BGM 상태 API
app.get('/api/bgm/status', (req, res) => {
    res.json({ playing: bgmPlaying });
});
app.post('/api/bgm/toggle', (req, res) => {
    if (bgmPlaying) { stopBgm(); } else { startBgm(); }
    res.json({ playing: bgmPlaying });
});
// ─────────────────────────────────────────────────────────────────────────

// 2. 서버 메모리에 현재 게임 상태 저장
let gameState = {};

// 팀별 타이머 상태: key=team_id, value={ timeRemaining, running }
const teamTimers = {};
const TIMER_INITIAL = 20 * 60;

// IP 주소 기반 노드 상태 저장 (재연결 시 복원용)
// key: IP주소, value: { team_id, team_name, mission, progress }
const savedNodes = {};

// 클리어 기록: key=team_id, value={ elapsed, clearTime, rank }
const clearRecords = {};
// 복구 코드: key=team_id, value=code (4자리 랜덤)
const recoveryCodes = {};

function generateRecoveryCode() {
    return String(Math.floor(1000 + Math.random() * 9000));
}

function getTotalTeamCount() {
    return Object.keys(teamTimers).length;
}

function getClearRank(tid) {
    // 현재까지 클리어한 팀 수 = 이 팀의 등수
    return Object.keys(clearRecords).length;
}

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
                // 팀명이 설정되어 있고 타이머가 아직 안 돌고 있으면 자동 시작
                const wsTid = String(clientInfo.team_id);
                const wstt = getTeamTimer(wsTid);
                if (data.team_name && !wstt.running) {
                    wstt.running = true;
                    ensureTimerLoop();
                    sendToTeam(wsTid, { type: 'timer_control', action: 'start' });
                    sendToTeam(wsTid, { type: 'timer_sync', timeRemaining: wstt.timeRemaining, running: true });
                    broadcastToRole('gm', { type: 'all_timers', timers: teamTimers });
                    console.log(`[TIMER] Team ${wsTid} timer auto-started (WS, team_name: ${data.team_name})`);
                }
            }

            // GM이 접속하면 현재까지의 상태를 한 번 보내줌
            if (data.role === 'gm') {
                ws.send(JSON.stringify({ type: 'init_state', state: gameState }));    
                ws.send(JSON.stringify({ type: 'all_timers', timers: teamTimers }));
                
                // 현재 접속 중인 노드 목록도 같이 보내주기
                const activeTeams = getActiveTeams();
                ws.send(JSON.stringify({ type: 'active_nodes', teams: activeTeams }));
            }
            return;
        }

        // B. 노드 상태 업데이트 수신
        if (data.type === 'update_progress' && clientInfo.role === 'node') {
            const tid = String(data.team_id);
            gameState[tid] = { mission: data.mission, progress: data.progress };
            // IP 기반 저장 업데이트
            if (clientInfo.ip && savedNodes[clientInfo.ip]) {
                savedNodes[clientInfo.ip].mission = data.mission;
                savedNodes[clientInfo.ip].progress = data.progress;
            }
            // 미션 1 시작 시 해당 팀 타이머 자동 시작
            if (data.mission === 1 && data.progress === 0) {
                const t = getTeamTimer(tid);
                if (!t.running) {
                    t.running = true;
                    ensureTimerLoop();
                    sendToTeam(tid, { type: 'timer_control', action: 'start' });
                    sendToTeam(tid, { type: 'timer_sync', timeRemaining: t.timeRemaining, running: true });
                    broadcastToRole('gm', { type: 'all_timers', timers: teamTimers });
                    console.log(`[TIMER] Team ${tid} timer auto-started (mission 1)`);
                }
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

        // C-2. 이벤트 상태 (노드→GM)
        if (data.type === 'event_status') {
            const tid = String(data.team_id || clientInfo.team_id);
            console.log(`[EVENT] Team ${tid}: ${data.event} → ${data.status}`);
            broadcastToRole('gm', { type: 'event_status', team_id: tid, team_name: data.team_name || '', event: data.event, status: data.status });
            return;
        }

        // C-3. 타이머 감소 패널티 (노드→서버)
        if (data.type === 'timer_penalty') {
            const tid = String(data.team_id || clientInfo.team_id);
            const penalty = data.seconds || 60;
            const t = getTeamTimer(tid);
            t.timeRemaining = Math.max(0, t.timeRemaining - penalty);
            console.log(`[TIMER] Team ${tid} penalty -${penalty}s → ${t.timeRemaining}s remaining`);
            sendToTeam(tid, { type: 'timer_sync', timeRemaining: t.timeRemaining, running: t.running });
            broadcastToRole('gm', { type: 'all_timers', timers: teamTimers });
            return;
        }

        // C-4. 게임 클리어 (노드→서버) — 미션5 완료 시 복구 코드 발급
        if (data.type === 'game_clear') {
            const tid = String(data.team_id || clientInfo.team_id);
            const code = generateRecoveryCode();
            recoveryCodes[tid] = code;
            // 클리어 정보 미리 계산 (타이머 정지 + 기록)
            const t = getTeamTimer(tid);
            t.running = false;
            const elapsed = TIMER_INITIAL - t.timeRemaining;
            const clearMin = Math.floor(elapsed / 60).toString().padStart(2, '0');
            const clearSec = (elapsed % 60).toString().padStart(2, '0');
            const clearTime = `${clearMin}:${clearSec}`;
            clearRecords[tid] = { elapsed, clearTime };
            const rank = getClearRank(tid);
            const totalTeams = getTotalTeamCount();
            const isLast = (rank >= totalTeams && totalTeams > 1);
            console.log(`[CLEAR] Team ${tid} mission 5 done, recovery code: ${code}, time: ${clearTime}, rank: ${rank}/${totalTeams}`);
            sendToTeam(tid, { type: 'timer_control', action: 'pause' });
            sendToTeam(tid, { type: 'timer_sync', timeRemaining: t.timeRemaining, running: false });
            // 노드에 복구 코드 + 클리어 정보 함께 전송
            sendToTeam(tid, { type: 'recovery_code', code: code, clear_time: clearTime, rank, total_teams: totalTeams, is_last: isLast });
            broadcastToRole('gm', { type: 'all_timers', timers: teamTimers });
            broadcastToRole('gm', {
                type: 'chat_message', sender: 'system',
                team_id: tid, text: `🔑 [T${tid}] 복구 코드 발급: ${code}`
            });
            return;
        }

        // C-4b. 복구 코드 확인 (노드→서버, 기록용)
        if (data.type === 'verify_recovery_code') {
            const tid = String(data.team_id || clientInfo.team_id);
            const inputCode = String(data.code || '');
            const expectedCode = recoveryCodes[tid];
            if (inputCode === expectedCode) {
                console.log(`[CLEAR] 🎉 Team ${tid} FINAL CLEAR verified!`);
                const rec = clearRecords[tid] || {};
                broadcastToRole('gm', {
                    type: 'game_clear', team_id: tid,
                    team_name: data.team_name || clientInfo.team_name || '',
                    clear_time: rec.clearTime || '??:??', elapsed: rec.elapsed || 0,
                    rank: getClearRank(tid), total_teams: getTotalTeamCount()
                });
                broadcastToRole('gm', {
                    type: 'chat_message', sender: 'system',
                    team_id: tid, text: `🎉 [T${tid}] GAME CLEAR! 클리어 타임: ${rec.clearTime || '??:??'} (${getClearRank(tid)}/${getTotalTeamCount()}등)`
                });
                delete recoveryCodes[tid];
            } else {
                console.log(`[CLEAR] Team ${tid} wrong recovery code: ${inputCode} (expected ${expectedCode})`);
            }
            return;
        }

        // D. GM 강제 제어 명령
        if (data.type === 'force_clear' && clientInfo.role === 'gm') {
            sendToTeam(data.target_team, data);
            return;
        }

        // E. 팀별 타이머 제어
        if (data.type === 'timer_control' && clientInfo.role === 'gm') {
            const targetTid = data.team_id ? String(data.team_id) : null;
            // team_id가 'ALL'이면 모든 팀에 적용
            const tids = (targetTid === 'ALL' || !targetTid) ? Object.keys(teamTimers) : [targetTid];
            for (const tid of tids) {
                const t = getTeamTimer(tid);
                if (data.action === 'start') {
                    t.running = true;
                    ensureTimerLoop();
                    sendToTeam(tid, { type: 'timer_control', action: 'start' });
                    sendToTeam(tid, { type: 'timer_sync', timeRemaining: t.timeRemaining, running: true });
                } else if (data.action === 'pause') {
                    t.running = false;
                    sendToTeam(tid, { type: 'timer_control', action: 'pause' });
                    sendToTeam(tid, { type: 'timer_sync', timeRemaining: t.timeRemaining, running: false });
                } else if (data.action === 'reset') {
                    t.running = false;
                    t.timeRemaining = TIMER_INITIAL;
                    sendToTeam(tid, { type: 'timer_control', action: 'reset' });
                    sendToTeam(tid, { type: 'timer_sync', timeRemaining: t.timeRemaining, running: false });
                }
            }
            // GM에게 모든 타이머 동기화
            broadcastToRole('gm', { type: 'all_timers', timers: teamTimers });
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
                if (cur >= 5) {
                    // 미션 5에서 skip → 최종 클리어 처리
                    gameState[tid] = { mission: 5, progress: 100 };
                    for (const ip of Object.keys(savedNodes)) {
                        if (String(savedNodes[ip].team_id) === tid) {
                            savedNodes[ip].mission = 5;
                            savedNodes[ip].progress = 100;
                        }
                    }
                    const t = getTeamTimer(tid);
                    t.running = false;
                    const elapsed = TIMER_INITIAL - t.timeRemaining;
                    const clearMin = Math.floor(elapsed / 60).toString().padStart(2, '0');
                    const clearSec = (elapsed % 60).toString().padStart(2, '0');
                    const clearTime = `${clearMin}:${clearSec}`;
                    clearRecords[tid] = { elapsed, clearTime };
                    const rank = getClearRank(tid);
                    const totalTeams = getTotalTeamCount();
                    const isLast = (rank >= totalTeams && totalTeams > 1);
                    console.log(`[CLEAR] 🎉 Team ${tid} GAME CLEAR (skip)! Time: ${clearTime} Rank: ${rank}/${totalTeams}`);
                    const code = generateRecoveryCode();
                    recoveryCodes[tid] = code;
                    sendToTeam(tid, { type: 'timer_control', action: 'pause' });
                    sendToTeam(tid, { type: 'timer_sync', timeRemaining: t.timeRemaining, running: false });
                    sendToTeam(tid, { type: 'recovery_code', code: code, clear_time: clearTime, rank, total_teams: totalTeams, is_last: isLast });
                    broadcastToRole('gm', { type: 'all_timers', timers: teamTimers });
                    broadcastToRole('gm', { type: 'game_clear', team_id: tid, clear_time: clearTime, elapsed, rank, total_teams: totalTeams });
                    broadcastToRole('gm', { type: 'chat_message', sender: 'system', team_id: tid, text: `🎉 [T${tid}] GAME CLEAR (skip)! ${clearTime} (${rank}/${totalTeams}등)` });
                    broadcastToRole('gm', { type: 'update_progress', team_id: tid, mission: 5, progress: 100 });
                    broadcastProgressSync();
                } else {
                    const nextMission = cur + 1;
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
            else if (data.command === 'force_ending') {
                // GM이 엔딩으로 강제 이동 — 복구 코드 입력부터 시작
                const code = generateRecoveryCode();
                recoveryCodes[tid] = code;
                // 클리어 정보 계산
                const t = getTeamTimer(tid);
                t.running = false;
                const elapsed = TIMER_INITIAL - t.timeRemaining;
                const clearMin = Math.floor(elapsed / 60).toString().padStart(2, '0');
                const clearSec = (elapsed % 60).toString().padStart(2, '0');
                const clearTime = `${clearMin}:${clearSec}`;
                clearRecords[tid] = { elapsed, clearTime };
                const rank = getClearRank(tid);
                const totalTeams = getTotalTeamCount();
                const isLast = (rank >= totalTeams && totalTeams > 1);
                console.log(`[CLEAR] Team ${tid} force ending, recovery code: ${code}, time: ${clearTime}`);
                gameState[tid] = { mission: 5, progress: 100 };
                sendToTeam(tid, { type: 'timer_control', action: 'pause' });
                sendToTeam(tid, { type: 'timer_sync', timeRemaining: t.timeRemaining, running: false });
                sendToTeam(tid, { type: 'recovery_code', code: code, clear_time: clearTime, rank, total_teams: totalTeams, is_last: isLast });
                broadcastToRole('gm', { type: 'all_timers', timers: teamTimers });
                broadcastToRole('gm', {
                    type: 'chat_message', sender: 'system',
                    team_id: tid, text: `🔑 [T${tid}] 강제 엔딩 → 복구 코드 발급: ${code}`
                });
                broadcastToRole('gm', { type: 'update_progress', team_id: tid, mission: 5, progress: 100 });
                broadcastProgressSync();
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
                        // 새로 연결된 노드에게 해당 팀 타이머 동기화
                        const tid = String(clientInfo.team_id);
                        const tt = getTeamTimer(tid);
                        // 팀명이 설정되어 있고 타이머가 아직 안 돌고 있으면 자동 시작
                        if (msg.team_name && !tt.running) {
                            tt.running = true;
                            ensureTimerLoop();
                            sendToTeam(tid, { type: 'timer_control', action: 'start' });
                            broadcastToRole('gm', { type: 'all_timers', timers: teamTimers });
                            console.log(`[TIMER] Team ${tid} timer auto-started (team_name set: ${msg.team_name})`);
                        }
                        socket.write(JSON.stringify({ type: 'timer_sync', timeRemaining: tt.timeRemaining, running: tt.running }) + '\n');
                    }
                    continue;
                }

                if (msg.type === 'update_progress' && clientInfo.role === 'node') {
                    const tid = String(msg.team_id);
                    gameState[tid] = { mission: msg.mission, progress: msg.progress };
                    if (clientInfo.ip && savedNodes[clientInfo.ip]) {
                        savedNodes[clientInfo.ip].mission = msg.mission;
                        savedNodes[clientInfo.ip].progress = msg.progress;
                    }
                    // 미션 1 시작 시 해당 팀 타이머 자동 시작
                    if (msg.mission === 1 && msg.progress === 0) {
                        const t = getTeamTimer(tid);
                        if (!t.running) {
                            t.running = true;
                            ensureTimerLoop();
                            sendToTeam(tid, { type: 'timer_control', action: 'start' });
                            sendToTeam(tid, { type: 'timer_sync', timeRemaining: t.timeRemaining, running: true });
                            broadcastToRole('gm', { type: 'all_timers', timers: teamTimers });
                            console.log(`[TIMER] Team ${tid} timer auto-started (mission 1, TCP)`);
                        }
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

                // 이벤트 상태 (GM에 전달)
                if (msg.type === 'event_status') {
                    const tid = String(msg.team_id || clientInfo.team_id);
                    console.log(`[EVENT] Team ${tid}: ${msg.event} → ${msg.status}`);
                    broadcastToRole('gm', {
                        type: 'event_status',
                        team_id: tid,
                        team_name: msg.team_name || '',
                        event: msg.event,
                        status: msg.status
                    });
                    continue;
                }

                // 타이머 감소 패널티
                if (msg.type === 'timer_penalty') {
                    const tid = String(msg.team_id || clientInfo.team_id);
                    const penalty = msg.seconds || 60;
                    const t = getTeamTimer(tid);
                    t.timeRemaining = Math.max(0, t.timeRemaining - penalty);
                    console.log(`[TIMER] Team ${tid} penalty -${penalty}s → ${t.timeRemaining}s remaining`);
                    sendToTeam(tid, { type: 'timer_sync', timeRemaining: t.timeRemaining, running: t.running });
                    broadcastToRole('gm', { type: 'all_timers', timers: teamTimers });
                    continue;
                }

                // 게임 클리어
                if (msg.type === 'game_clear') {
                    const tid = String(msg.team_id || clientInfo.team_id);
                    const t = getTeamTimer(tid);
                    t.running = false;
                    const elapsed = TIMER_INITIAL - t.timeRemaining;
                    const clearMin = Math.floor(elapsed / 60).toString().padStart(2, '0');
                    const clearSec = (elapsed % 60).toString().padStart(2, '0');
                    const clearTime = `${clearMin}:${clearSec}`;
                    console.log(`[CLEAR] 🎉 Team ${tid} GAME CLEAR! Time: ${clearTime} (${elapsed}s)`);
                    sendToTeam(tid, { type: 'timer_control', action: 'pause' });
                    sendToTeam(tid, { type: 'timer_sync', timeRemaining: t.timeRemaining, running: false });
                    broadcastToRole('gm', { type: 'all_timers', timers: teamTimers });
                    broadcastToRole('gm', {
                        type: 'game_clear', team_id: tid,
                        team_name: msg.team_name || clientInfo.team_name || '',
                        clear_time: clearTime, elapsed: elapsed
                    });
                    broadcastToRole('gm', {
                        type: 'chat_message', sender: 'system',
                        team_id: tid, text: `🎉 [T${tid}] GAME CLEAR! 클리어 타임: ${clearTime}`
                    });
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

// --- 팀별 타이머 ---
let serverTimerInterval = null;
function ensureTimerLoop() {
    if (serverTimerInterval) return;
    serverTimerInterval = setInterval(() => {
        let anyRunning = false;
        for (const tid of Object.keys(teamTimers)) {
            const t = teamTimers[tid];
            if (t.running && t.timeRemaining > 0) {
                t.timeRemaining--;
                anyRunning = true;
                // 10초마다 해당 팀 노드에 동기화
                if (t.timeRemaining % 10 === 0) {
                    sendToTeam(tid, { type: 'timer_sync', timeRemaining: t.timeRemaining, running: true });
                }
                if (t.timeRemaining <= 0) {
                    t.running = false;
                    sendToTeam(tid, { type: 'timer_sync', timeRemaining: 0, running: false });
                }
            }
        }
        // 매 초 GM에게 모든 팀 타이머 동기화
        broadcastToRole('gm', { type: 'all_timers', timers: teamTimers });
        if (!anyRunning) {
            clearInterval(serverTimerInterval);
            serverTimerInterval = null;
        }
    }, 1000);
}
function getTeamTimer(tid) {
    if (!teamTimers[tid]) teamTimers[tid] = { timeRemaining: TIMER_INITIAL, running: false };
    return teamTimers[tid];
}

// 3. 서버 실행
const PORT = 8080;
const TCP_PORT = 5000;

server.listen(PORT, '0.0.0.0', () => {
    console.log(`[SYSTEM] 방탈출 백엔드 서버(WebSocket/HTTP)가 http://127.0.0.1:${PORT} 에서 실행 중입니다.`);
    // 서버 시작 시 BGM 자동 재생
    startBgm();
});

tcpServer.listen(TCP_PORT, '0.0.0.0', () => {
    console.log(`[SYSTEM] 방탈출 TCP 서버(C++ Node용)가 포트 ${TCP_PORT} 에서 대기 중입니다.`);
});
