#pragma once
#include <atomic>

// ─── BleConnectGate ──────────────────────────────────────────────────────────
//
// NimBLE can open only ONE GATT connection at a time. The BMS and the IMU run in separate tasks
// with their own backoff timers, so their attempts drifted into each other: the second connect()
// failed at once (BLE_HS_EALREADY) and often dragged the first one down with it, each failure
// doubling both backoffs. With the IMU switched off and retrying, the battery stayed offline for
// minutes while it was in range and answering fine on its own.
//
// Whoever wants to connect takes the gate first; if it is busy, it simply tries again on its
// next tick (100 ms later) — a skipped turn is not a failure and must not count toward backoff.
//
//   BleConnectGate gate;
//   if (!gate) return;          // someone else is connecting
//   … connect, discover, subscribe … (released on every return path)

class BleConnectGate {
public:
    BleConnectGate() { bool f = false; _held = busy().compare_exchange_strong(f, true); }
    ~BleConnectGate() { if (_held) busy().store(false); }
    explicit operator bool() const { return _held; }

    BleConnectGate(const BleConnectGate&) = delete;
    BleConnectGate& operator=(const BleConnectGate&) = delete;

private:
    static std::atomic<bool>& busy() { static std::atomic<bool> b(false); return b; }
    bool _held;
};
