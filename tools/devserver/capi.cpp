// SPDX-License-Identifier: GPL-3.0-only
// A C interface to the engine for the desktop test server (tools/devserver/server.py): the panel in a browser,
// with the real engine behind it (no sound).
#include <cstdlib>
#include <cstring>
#include "../../app/src/main/cpp/box/BoxEngine.hpp"

static gbox::Engine* E() {
	static gbox::Engine* e = new gbox::Engine();
	return e;
}
static char* dup(const std::string& s) {
	char* p = (char*) std::malloc(s.size() + 1);
	std::memcpy(p, s.c_str(), s.size() + 1);
	return p;
}
extern "C" {
void gb_init(const char* dir) {
	E()->setSampleRate(48000.f);
	E()->init(dir);
}
char* gb_meta() { return dup(E()->meta()); }
char* gb_state(int k) { return dup(E()->state(k)); }
char* gb_fast() { return dup(E()->fast()); }
void gb_cmd(int op, int a, int b, float f) { E()->cmd(op, a, b, f); }
void gb_save_preset(int t, const char* name) { E()->savePreset(t, name); }
void gb_save() { E()->saveSession(); }
void gb_free(char* p) { std::free(p); }
float gb_render(int n) {
	static float L[4096], R[4096];
	float peak = 0.f;
	while (n > 0) {
		int k = n > 4096 ? 4096 : n;
		E()->render(L, R, k);
		for (int i = 0; i < k; i++)
			peak = peak > std::abs(L[i]) ? peak : std::abs(L[i]);
		n -= k;
	}
	return peak;
}
int gb_export(int what, int idx, unsigned char* out, int cap) {
	std::string name;
	std::vector<uint8_t> d = E()->exportMidi(what, idx, name);
	int n = (int) d.size() < cap ? (int) d.size() : cap;
	std::memcpy(out, d.data(), n);
	return (int) d.size();
}
}
