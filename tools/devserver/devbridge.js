// The desktop stand-in for window.GB (MainActivity.Bridge): every call goes to the test server (server.py).
(function () {
  function get(path) {
    const x = new XMLHttpRequest();
    x.open('GET', path, false);
    x.send(null);
    return x.responseText;
  }
  const q = encodeURIComponent;
  window.GB = {
    meta: () => get('/api/meta'),
    state: k => get('/api/state?k=' + k),
    fast: () => get('/api/fast'),
    cmd: (op, a, b, f) => { get(`/api/cmd?op=${op}&a=${a}&b=${b}&f=${f}`); },
    savePreset: (t, n) => { get(`/api/savePreset?t=${t}&name=${q(n)}`); },
    save: () => { get('/api/save'); },
    audioInfo: () => 'DESKTOP TEST SERVER (NO SOUND)',
    version: () => 'dev',
    exportMidi: (w, i) => get(`/api/export?w=${w}&i=${i}`),
    gridState: (t, f, n) => get(`/api/gridState?t=${t}&f=${f}&n=${n}`),
    gridWrite: (t, js) => { get(`/api/gridWrite?t=${t}&js=${q(js)}`); },
    projects: () => get('/api/projects'),
    project: (w, a) => { get(`/api/project?w=${w}&a=${q(a)}`); },
  };
})();
