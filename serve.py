#!/usr/bin/env python3
"""test.html 을 localhost 로 띄우고, 타이핑 로그를 logs/ 밑에 직접 받아 적는 서버.

브라우저는 임의의 경로에 파일을 쓸 수 없다. 그래서 페이지가 CSV 를 만들어
POST 로 넘기고, 이 서버가 받아서 파일에 append 한다. 문장을 하나 끝낼 때마다
디스크에 바로 붙기 때문에 중간에 브라우저가 죽어도 그때까지는 남는다.

    python3 serve.py            # http://localhost:8000/test.html
    python3 serve.py 8080       # 포트 바꾸기

Web Serial 은 file:// 에서 막히므로 어차피 localhost 로 띄워야 한다 — 그 역할도 겸한다.

logs/
  position-1/
    position-1-phrases.csv    문장 하나 = 한 줄
    position-1-keys.csv       키 하나 = 한 줄
    position-1-presses.csv    타건(버클링) 하나 = 한 줄
    position-1-misses.csv     눌렀는데 글자가 안 나온 누름 하나 = 한 줄
"""
import http.server, json, os, re, socketserver, sys

ROOT = os.path.dirname(os.path.abspath(__file__))
LOGS = os.path.join(ROOT, 'logs')
KINDS = ('phrases', 'keys', 'presses', 'misses')
DIR_RE = re.compile(r'^[a-z0-9]+-\d+$')       # 경로 탈출 방지. 서버가 만든 이름만 통과한다
MODE_RE = re.compile(r'^[a-z0-9]+$')
BOM = '﻿'                                # 엑셀이 UTF-8 로 열도록


def csv_path(d, kind):
    return os.path.join(LOGS, d, '%s-%s.csv' % (d, kind))


def next_dir(mode):
    """logs/ 를 훑어 {mode}-N 중 안 쓴 가장 작은 N 을 잡는다."""
    os.makedirs(LOGS, exist_ok=True)
    used = set()
    for name in os.listdir(LOGS):
        m = re.match(r'^' + re.escape(mode) + r'-(\d+)$', name)
        if m and os.path.isdir(os.path.join(LOGS, name)):
            used.add(int(m.group(1)))
    n = 1
    while n in used:
        n += 1
    return '%s-%d' % (mode, n)


class Handler(http.server.SimpleHTTPRequestHandler):
    def __init__(self, *a, **kw):
        super().__init__(*a, directory=ROOT, **kw)

    def log_message(self, fmt, *args):        # 정적 파일 요청까지 찍히면 시끄럽다
        if not self.path.startswith('/api/'):
            return
        sys.stderr.write('  %s\n' % (fmt % args))

    # ── POST /api/session : 새 세션 폴더 + 헤더 줄 ────────────────────────────
    # ── POST /api/append  : 방금 만들어진 줄들을 이어 붙인다 ──────────────────
    def do_POST(self):
        try:
            body = self.rfile.read(int(self.headers.get('Content-Length', 0)))
            req = json.loads(body.decode('utf-8'))
            if self.path == '/api/session':
                self.reply(self.start_session(req))
            elif self.path == '/api/append':
                self.reply(self.append(req))
            else:
                self.reply({'error': 'unknown endpoint'}, 404)
        except Exception as e:
            self.reply({'error': '%s: %s' % (type(e).__name__, e)}, 500)

    def start_session(self, req):
        mode = str(req.get('mode', '')).strip().lower()
        if not MODE_RE.match(mode):
            raise ValueError('bad mode %r' % mode)
        d = next_dir(mode)
        os.makedirs(os.path.join(LOGS, d))
        headers = req.get('headers') or {}
        for kind in KINDS:
            with open(csv_path(d, kind), 'w', encoding='utf-8', newline='') as f:
                f.write(BOM + headers.get(kind, '') + '\n')
        sys.stderr.write('  → logs/%s/ 열림\n' % d)
        return {'dir': d}

    def append(self, req):
        d = str(req.get('dir', ''))
        if not DIR_RE.match(d) or not os.path.isdir(os.path.join(LOGS, d)):
            raise ValueError('bad dir %r' % d)
        wrote = {}
        for kind in KINDS:
            text = req.get(kind) or ''
            if not text:
                continue
            with open(csv_path(d, kind), 'a', encoding='utf-8', newline='') as f:
                f.write(text + '\n')
            wrote[kind] = text.count('\n') + 1
        return {'ok': True, 'wrote': wrote}

    def reply(self, obj, code=200):
        raw = json.dumps(obj).encode('utf-8')
        self.send_response(code)
        self.send_header('Content-Type', 'application/json; charset=utf-8')
        self.send_header('Content-Length', str(len(raw)))
        self.end_headers()
        self.wfile.write(raw)

    def end_headers(self):
        # 편집하고 새로고침했는데 옛 test.html 이 뜨는 일이 없게
        self.send_header('Cache-Control', 'no-store')
        super().end_headers()


class Server(socketserver.ThreadingTCPServer):
    allow_reuse_address = True
    daemon_threads = True


if __name__ == '__main__':
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 8000
    os.makedirs(LOGS, exist_ok=True)
    print('  http://localhost:%d/test.html' % port)
    print('  로그 → %s/' % LOGS)
    with Server(('127.0.0.1', port), Handler) as httpd:
        try:
            httpd.serve_forever()
        except KeyboardInterrupt:
            print('\n  종료')
