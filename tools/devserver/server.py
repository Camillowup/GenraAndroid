#!/usr/bin/env python3
"""The panel of GENRA BOX in a desktop browser, with the real engine behind it (no sound): for working on the UI.
   ./build.sh && python3 server.py  ->  http://localhost:8765/ui/index.html"""
import ctypes, os, sys, threading, time, urllib.parse
from http.server import ThreadingHTTPServer, BaseHTTPRequestHandler

HERE = os.path.dirname(os.path.abspath(__file__))
UI = os.path.join(HERE, '..', '..', 'app', 'src', 'main', 'assets', 'ui')
lib = ctypes.CDLL(os.path.join(HERE, 'build', 'libgenrabox-dev.so'))
for n in ('gb_meta', 'gb_state', 'gb_fast'):
    getattr(lib, n).restype = ctypes.c_void_p
lib.gb_state.argtypes = [ctypes.c_int]
lib.gb_cmd.argtypes = [ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.c_float]
lib.gb_render.restype = ctypes.c_float
lib.gb_free.argtypes = [ctypes.c_void_p]
lock = threading.Lock()
files = os.path.join(HERE, 'build', 'files')
os.makedirs(files, exist_ok=True)
lib.gb_init(files.encode())

def text(p):
    s = ctypes.string_at(p).decode('utf-8')
    lib.gb_free(p)
    return s

def audio():  # the engine runs in real time, without a sound card
    while True:
        with lock:
            lib.gb_render(960)
        time.sleep(0.02)

class H(BaseHTTPRequestHandler):
    def log_message(self, *a): pass
    def send(self, body, ctype='text/plain; charset=utf-8'):
        b = body.encode('utf-8') if isinstance(body, str) else body
        self.send_response(200); self.send_header('Content-Type', ctype); self.send_header('Content-Length', str(len(b)))
        self.send_header('Cache-Control', 'no-store'); self.end_headers(); self.wfile.write(b)
    def do_GET(self):
        u = urllib.parse.urlparse(self.path); q = dict(urllib.parse.parse_qsl(u.query)); p = u.path
        if p.startswith('/api/'):
            with lock:
                if p == '/api/meta': return self.send(text(lib.gb_meta()))
                if p == '/api/state': return self.send(text(lib.gb_state(int(q.get('k', 0)))))
                if p == '/api/fast': return self.send(text(lib.gb_fast()))
                if p == '/api/cmd':
                    lib.gb_cmd(int(q['op']), int(q['a']), int(q['b']), float(q['f'])); return self.send('')
                if p == '/api/savePreset':
                    lib.gb_save_preset(int(q['t']), q.get('name', '').encode()); return self.send('')
                if p == '/api/save': lib.gb_save(); return self.send('')
                if p == '/api/export':
                    buf = ctypes.create_string_buffer(1 << 20)
                    n = lib.gb_export(int(q['w']), int(q['i']), buf, 1 << 20)
                    return self.send('Exported %d bytes (desktop: not saved)' % n if n else 'Nothing to export')
            return self.send('?')
        if p == '/dev/devbridge.js':
            return self.send(open(os.path.join(HERE, 'devbridge.js'), 'rb').read(), 'text/javascript')
        if p.startswith('/ui/'):
            f = os.path.normpath(os.path.join(UI, p[4:]))
            if f.startswith(os.path.normpath(UI)) and os.path.isfile(f):
                ct = {'.html': 'text/html', '.js': 'text/javascript', '.css': 'text/css', '.woff': 'font/woff'}.get(os.path.splitext(f)[1], 'application/octet-stream')
                return self.send(open(f, 'rb').read(), ct)
        self.send_response(404); self.end_headers()

if __name__ == '__main__':
    threading.Thread(target=audio, daemon=True).start()
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 8765
    print('http://localhost:%d/ui/index.html' % port)
    ThreadingHTTPServer(('127.0.0.1', port), H).serve_forever()
