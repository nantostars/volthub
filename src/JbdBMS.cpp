#include "JbdBMS.h"
#include <string.h>

// DD A5 <fn> 00 <checksum BE> 77 — the checksum is the two's complement of <fn> + 0x00.
const uint8_t JbdBMS::REQ_BASIC[7] = {0xDD, 0xA5, JBD_FN_BASIC, 0x00, 0xFF, 0xFD, 0x77};
const uint8_t JbdBMS::REQ_CELLS[7] = {0xDD, 0xA5, JBD_FN_CELLS, 0x00, 0xFF, 0xFC, 0x77};

// Alternate the two requests: basic info on even polls, cell voltages on odd ones. At the 2 s
// cadence that is a full refresh every 4 s, which is plenty for a battery and keeps one
// request in flight at a time — this BMS answers one question at a time.
void JbdBMS::onPoll(uint32_t seq) {
    _rxLen = 0;                                   // a new request voids any half-received answer
    if (seq & 1) writeCmd(REQ_CELLS, sizeof(REQ_CELLS));
    else         writeCmd(REQ_BASIC, sizeof(REQ_BASIC));
}

void JbdBMS::onDisconnected() {
    _rxLen = 0;
    _haveBasic = false;
}

void JbdBMS::onNotify(const uint8_t* data, size_t len) {
    feed(data, len);
}

// Reassemble a frame across notifications: 0xDD starts one, the header's length byte says how
// long it will be. Anything that does not fit or does not start with 0xDD is dropped rather
// than parsed half-way — a wrong frame here would show a wrong SOC.
void JbdBMS::feed(const uint8_t* data, size_t len) {
    if (!data || len == 0) return;
    if (data[0] == JBD_START) _rxLen = 0;         // resync on every start marker
    if (_rxLen == 0 && data[0] != JBD_START) return;

    if (_rxLen + len > sizeof(_rx)) { _rxLen = 0; return; }
    memcpy(_rx + _rxLen, data, len);
    _rxLen += len;

    const size_t want = jbdFrameLen(_rx, _rxLen);
    if (want == 0 || _rxLen < want) return;       // header or body still incomplete

    handleFrame(_rx, want);
    _rxLen = 0;
}

void JbdBMS::handleFrame(const uint8_t* f, size_t n) {
    const uint8_t fn = jbdDecodeFrame(f, n, _stage);
    if (fn == JBD_FN_BASIC) _haveBasic = true;

    // Cells alone would publish a snapshot with no voltage or SOC, so wait for the first basic
    // frame. After that every frame refreshes the part it carries and republishes the whole.
    if (fn != 0 && _haveBasic) {
        BmsData copy = _stage;
        publish(copy);        // fills power, avgCurrent (EMA), lastSeen, valid
    }
}
