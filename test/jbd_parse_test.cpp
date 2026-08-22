// Host-side test for the JBD/Xiaoxiang decoder. No battery, no board:
//
//   g++ -std=c++17 -I src test/jbd_parse_test.cpp -o /tmp/jbd_test && /tmp/jbd_test
//
// It cannot prove the firmware talks to a real BMS — nobody here owns one — but it does catch
// the mistakes this kind of work actually produces: endianness flipped (JBD is big-endian while
// LiTime is little-endian), an offset off by one, a checksum rule misread, a short frame parsed
// anyway. PlatformIO ignores test/ during `pio run`, so this file never reaches the device.

#include "JbdProtocol.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>

static int failures = 0;

static void check(bool ok, const char* what) {
    if (!ok) { printf("  FAIL  %s\n", what); failures++; }
    else     { printf("  ok    %s\n", what); }
}
static void near(float got, float want, float tol, const char* what) {
    if (std::fabs(got - want) > tol) {
        printf("  FAIL  %s: got %.4f, want %.4f\n", what, got, want);
        failures++;
    } else printf("  ok    %s = %.3f\n", what, got);
}

// Build a well-formed response frame: DD <fn> 00 <len> <payload> <ck-hi> <ck-lo> 77
static std::vector<uint8_t> frame(uint8_t fn, const std::vector<uint8_t>& payload) {
    std::vector<uint8_t> f{JBD_START, fn, 0x00, (uint8_t)payload.size()};
    f.insert(f.end(), payload.begin(), payload.end());
    const uint16_t ck = jbdChecksum(&f[2], payload.size() + 2);
    f.push_back((uint8_t)(ck >> 8));
    f.push_back((uint8_t)(ck & 0xFF));
    f.push_back(JBD_END);
    return f;
}

static uint8_t hi(int v) { return (uint8_t)((v >> 8) & 0xFF); }
static uint8_t lo(int v) { return (uint8_t)(v & 0xFF); }

int main() {
    // ── the requests the driver writes, checked against the documented constants ──
    printf("requests\n");
    check(jbdChecksum((const uint8_t[]){JBD_FN_BASIC, 0x00}, 2) == 0xFFFD, "0x03 request checksum");
    check(jbdChecksum((const uint8_t[]){JBD_FN_CELLS, 0x00}, 2) == 0xFFFC, "0x04 request checksum");

    // ── 0x03 basic info: 13.25 V, −8.6 A, 83.1/140 Ah, 42 cycles, 56 %, 4 cells, 2 probes ──
    printf("\nbasic info (0x03)\n");
    std::vector<uint8_t> basic(27, 0);
    basic[0]=hi(1325);  basic[1]=lo(1325);          // 13.25 V
    basic[2]=hi(-860);  basic[3]=lo(-860);          // −8.60 A (two's complement)
    basic[4]=hi(8310);  basic[5]=lo(8310);          // 83.10 Ah remaining
    basic[6]=hi(14000); basic[7]=lo(14000);         // 140.00 Ah nominal
    basic[8]=hi(42);    basic[9]=lo(42);            // 42 cycles
    basic[19]=56;                                    // 56 %
    basic[21]=4;                                     // 4 cells
    basic[22]=2;                                     // 2 temperature probes
    basic[23]=hi(2971); basic[24]=lo(2971);         // 24 °C
    basic[25]=hi(3031); basic[26]=lo(3031);         // 30 °C

    auto fb = frame(JBD_FN_BASIC, basic);
    BmsData d = {};
    check(jbdDecodeFrame(fb.data(), fb.size(), d) == JBD_FN_BASIC, "frame accepted");
    near(d.voltage,        13.25f, 0.001f, "voltage");
    near(d.current,        -8.60f, 0.001f, "current (negative = discharging)");
    near(d.remainingAh,    83.10f, 0.001f, "remainingAh");
    near(d.fullCapacityAh, 140.0f, 0.001f, "fullCapacityAh");
    check(d.soc == 56,              "soc");
    check(d.dischargesCount == 42,  "cycles");
    check(d.cellTemp == 24,         "cellTemp");
    check(d.mosfetTemp == 30,       "mosfetTemp");
    check(d.soh == 0,               "soh stays 0 (not in this protocol)");

    // ── 0x04 cell voltages ────────────────────────────────────────────────────
    printf("\ncell voltages (0x04)\n");
    std::vector<uint8_t> cells;
    for (int mv : {3312, 3309, 3315, 3311}) { cells.push_back(hi(mv)); cells.push_back(lo(mv)); }
    auto fc = frame(JBD_FN_CELLS, cells);
    check(jbdDecodeFrame(fc.data(), fc.size(), d) == JBD_FN_CELLS, "frame accepted");
    check(d.cellCount == 4, "cellCount");
    near(d.cellVoltages[0], 3.312f, 0.0005f, "cell 1");
    near(d.cellVoltages[3], 3.311f, 0.0005f, "cell 4");
    near(d.voltage, 13.25f, 0.001f, "basic-info fields survive a cell frame");

    // ── frames that must be refused ───────────────────────────────────────────
    printf("\nrejection\n");
    auto bad = fb; bad[10] ^= 0xFF;
    check(jbdDecodeFrame(bad.data(), bad.size(), d) == 0, "payload corrupted → checksum fails");
    bad = fb; bad[2] = 0x80;
    check(jbdDecodeFrame(bad.data(), bad.size(), d) == 0, "status 0x80 (request refused)");
    bad = fb; bad.back() = 0x00;
    check(jbdDecodeFrame(bad.data(), bad.size(), d) == 0, "missing 0x77 terminator");
    bad = fb; bad.pop_back();
    check(jbdDecodeFrame(bad.data(), bad.size(), d) == 0, "truncated frame");
    check(jbdFrameLen(fb.data(), 3) == 0, "length unknown until the header is in");
    check(jbdFrameLen(fb.data(), fb.size()) == fb.size(), "declared length matches");
    std::vector<uint8_t> shortBasic(20, 0);
    auto fs = frame(JBD_FN_BASIC, shortBasic);
    check(jbdDecodeFrame(fs.data(), fs.size(), d) == 0, "basic info shorter than 23 bytes");

    printf("\n%s\n", failures ? "FAILED" : "all good");
    return failures ? 1 : 0;
}
