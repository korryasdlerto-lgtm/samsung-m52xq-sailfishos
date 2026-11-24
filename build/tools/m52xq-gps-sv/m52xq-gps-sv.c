/* m52xq: GNSS signal check through QMI LOC (QRTR): number of satellites and signal strength only, no position data is read. */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <time.h>
#include <poll.h>
#include <sys/socket.h>

#define AF_QIPCRTR 42
struct sockaddr_qrtr { unsigned short sq_family; uint32_t sq_node, sq_port; };
#define PORT_CTRL 0xfffffffeu

static int s;
static struct sockaddr_qrtr loc;
static uint16_t txn;

static void put(uint8_t *b, int *n, uint8_t t, const void *v, uint16_t l)
{
	b[(*n)++] = t; memcpy(b + *n, &l, 2); *n += 2; memcpy(b + *n, v, l); *n += l;
}
static void req(uint16_t msg, const uint8_t *tlv, int l)
{
	uint8_t p[512]; uint16_t len = l; int n = 0;
	p[n++] = 0; txn++; memcpy(p + n, &txn, 2); n += 2; memcpy(p + n, &msg, 2); n += 2; memcpy(p + n, &len, 2); n += 2;
	memcpy(p + n, tlv, l); n += l;
	if (sendto(s, p, n, 0, (void *)&loc, sizeof(loc)) < 0) perror("sendto");
}
static const uint8_t *tl(const uint8_t *b, int len, uint8_t want, int *l)
{
	for (int i = 0; i + 3 <= len; ) {
		uint16_t n; memcpy(&n, b + i + 1, 2);
		if (b[i] == want && i + 3 + n <= len) { *l = n; return b + i + 3; }
		i += 3 + n;
	}
	return NULL;
}
int main(int argc, char **argv)
{
	int secs = argc > 1 ? atoi(argv[1]) : 120, lastkey = -1, fixes = 0, best_all = 0, reports = 0;
	struct sockaddr_qrtr me; socklen_t ml = sizeof(me);
	uint8_t buf[65536], b[256]; int n;

	s = socket(AF_QIPCRTR, SOCK_DGRAM, 0);
	if (s < 0) { perror("socket"); return 1; }
	getsockname(s, (void *)&me, &ml);
	uint32_t lk[5] = { 10, 16, 0, 0, 0 };
	struct sockaddr_qrtr ctrl = { AF_QIPCRTR, me.sq_node, PORT_CTRL };
	sendto(s, lk, sizeof(lk), 0, (void *)&ctrl, sizeof(ctrl));
	time_t t0 = time(NULL);
	while (!loc.sq_port && time(NULL) - t0 < 6) {
		struct pollfd p = { s, POLLIN, 0 }; if (poll(&p, 1, 1000) <= 0) continue;
		if (recv(s, buf, sizeof(buf), 0) >= 20) { uint32_t c[5]; memcpy(c, buf, 20);
			if (c[0] == 4 && c[1] == 16) { loc.sq_family = AF_QIPCRTR; loc.sq_node = c[3]; loc.sq_port = c[4]; } }
	}
	if (!loc.sq_port) { puts("no LOC service"); return 1; }
	uint64_t ev = 0x3 | 0x80 | 0x100; n = 0; put(b, &n, 1, &ev, 8); req(0x21, b, n);
	uint8_t sid = 1; uint32_t per = 1, off = 1, iv = 1000; n = 0; put(b, &n, 1, &sid, 1); put(b, &n, 0x10, &per, 4); put(b, &n, 0x12, &off, 4); put(b, &n, 0x13, &iv, 4); req(0x22, b, n);
	t0 = time(NULL);
	while (time(NULL) - t0 < secs) {
		struct pollfd p = { s, POLLIN, 0 }; if (poll(&p, 1, 1000) <= 0) continue;
		int r = recv(s, buf, sizeof(buf), 0); uint16_t mid, len; int l;
		if (r < 7) continue;
		memcpy(&mid, buf + 3, 2); memcpy(&len, buf + 5, 2);
		if (buf[0] == 2) { const uint8_t *e = tl(buf + 7, r - 7, 2, &l); uint16_t res[2] = {0}; if (e && l >= 4) memcpy(res, e, 4); printf("response 0x%04x result=%u error=%u\n", mid, res[0], res[1]); fflush(stdout); continue; }
		if (buf[0] != 4) continue;
		{ if (mid != 0x24 && mid != 0x25) { printf("%ld s indication 0x%04x:", (long)(time(NULL) - t0), mid); for (int i = 7; i + 3 <= r; ) { uint16_t q; memcpy(&q, buf + i + 1, 2); printf(" 0x%02x=", buf[i]); for (int k = 0; k < q && k < 8; k++) printf("%02x", buf[i + 3 + k]); i += 3 + q; } puts(""); fflush(stdout); } }
		if (mid == 0x24) { const uint8_t *st = tl(buf + 7, r - 7, 1, &l); uint32_t v = 99; if (st && l >= 4) memcpy(&v, st, 4); if (v == 0) { if (fixes++ < 3) printf("%ld s FIX\n", (long)(time(NULL) - t0)); } else { static int f; if (f++ < 12) printf("%ld s position report status=%u\n", (long)(time(NULL) - t0), v); } fflush(stdout); }
		if (mid == 0x25) {
			/* 0x11 = expanded list: valid u32, system u32, id u16, health u8, status u32, mask u8, elevation, azimuth, snr float, glonass frequency u8 */
			const uint8_t *sv = tl(buf + 7, r - 7, 0x11, &l); int cnt = sv && l ? sv[0] : 0, track = 0, best = 0, st3 = 0, used = 0, sum = 0; int sysn[8] = {0};
			for (int i = 0; i < cnt && 1 + (i + 1) * 29 <= l; i++) {
				const uint8_t *e = sv + 1 + i * 29; uint32_t sys, stt; float snr = 0; memcpy(&sys, e + 4, 4); memcpy(&stt, e + 11, 4); memcpy(&snr, e + 24, 4);
				if (snr > 1 && snr < 99) { track++; if ((int)snr > best) best = snr; if (snr >= 25) used++; if (sys < 8) sysn[sys]++; sum += snr; }
				if (stt == 3) st3++;
			}
			(void)sum;
			if (best > best_all) best_all = best;
			if (cnt * 1000 + track * 10 + best / 5 != lastkey) { lastkey = cnt * 1000 + track * 10 + best / 5; printf("%ld s listed=%d tracking=%d with signal=%d strong(>=25)=%d best C/N0=%d (by system gps=%d gal=%d sbas=%d cmps=%d glo=%d)\n", (long)(time(NULL) - t0), cnt, st3, track, used, best, sysn[1], sysn[2], sysn[3], sysn[4], sysn[5]); fflush(stdout); }
		}
	}
	n = 0; put(b, &n, 1, &sid, 1); req(0x23, b, n); usleep(300000);
	printf("END reports=%d best=%d dBHz fixes=%d\n", reports, best_all, fixes);
	return 0;
}
