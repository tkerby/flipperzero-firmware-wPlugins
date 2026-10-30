#include "master_sim.h"

#include <string.h>

#define SERIAL_NO_DATA_BYTE        0xFE
#define SERIAL_PREAMBLE_BYTE       0xFD
#define USING_EXTERNAL_CLOCK       0x01
#define USING_INTERNAL_CLOCK       0x02
#define CONNECTION_NOT_ESTABLISHED 0xFF
#define LINK_NULL                  0
#define LINK_TIMECAPSULE           1
#define LINK_TRADECENTER           2
#define LINK_COLOSSEUM             3
#define SERIAL_TIMECAPSULE         0x60
#define SERIAL_TRADECENTER         0x70
#define SERIAL_BATTLE              0x80
#define CABLECLUBROOM_COLOSSEUM    2

void ms_init(MasterSim* m, PblSession* slave) {
    memset(m, 0, sizeof(*m));
    m->slave = slave;
    m->hSerialConnectionStatus = CONNECTION_NOT_ESTABLISHED;
    m->hSerialSend = SERIAL_NO_DATA_BYTE;
    m->slave_preload = 0x00;
}

static uint32_t now_ms(const MasterSim* m) {
    return (uint32_t)(m->now_us / 1000u);
}

/* One 8-bit transfer clocked by the master: both shift registers swap. */
static uint8_t clock_byte(MasterSim* m, uint8_t out) {
    uint8_t in = m->slave_preload;
    m->now_us += 977; /* 8 bits at 8192 Hz */
    m->slave_preload = pbl_on_byte(m->slave, out, now_ms(m));
    if(m->trace && m->trace_len + 2 <= m->trace_cap) {
        m->trace[m->trace_len++] = out;
        m->trace[m->trace_len++] = in;
    }
    m->transfers++;
    return in;
}

/* Serial:: (connection established path) */
static void serial_isr(MasterSim* m, uint8_t in) {
    m->hSerialReceive = in;
    m->rSB = m->hSerialSend;
    m->hSerialReceivedNewData = true;
    m->hSerialSend = SERIAL_NO_DATA_BYTE;
}

static void complete_transfer(MasterSim* m) {
    if(!m->transfer_pending) return;
    m->transfer_pending = false;
    serial_isr(m, clock_byte(m, m->rSB));
}

/* ld a, SC_START | SC_INTERNAL ; ldh [rSC], a */
static void start_transfer(MasterSim* m) {
    complete_transfer(m);
    m->transfer_pending = true;
}

void ms_delay_frames(MasterSim* m, uint32_t n) {
    while(n--) {
        complete_transfer(m);
        m->now_us += MS_FRAME_US;
        m->frames++;
        if(m->on_frame) m->on_frame(m, m->hook_ctx);
    }
}

static void delay_frame(MasterSim* m) {
    ms_delay_frames(m, 1);
}

/* ---------------- home/serial.asm ---------------- */

static uint8_t serial_exchange_byte(MasterSim* m, uint8_t send_on_retry) {
    for(;;) {
        /* .timeout_loop */
        m->hSerialReceivedNewData = false;
        start_transfer(m);
        complete_transfer(m); /* .loop busy-waits for the interrupt */
        /* .await_new_data */
        m->hSerialReceivedNewData = false;
        if(m->hSerialReceive != SERIAL_NO_DATA_BYTE) return m->hSerialReceive;
        /* wLinkTimeoutFrames is 0 here: .timed_out */
        if(m->ie_serial_only) return SERIAL_NO_DATA_BYTE;
        m->hSerialSend = send_on_retry;
        delay_frame(m);
    }
}

static void serial_exchange_bytes(MasterSim* m, const uint8_t* hl, uint8_t* de, uint16_t bc) {
    m->hSerialIgnoringInitialData = true;
    while(bc) {
        m->hSerialSend = *hl;
        uint8_t a = serial_exchange_byte(m, *hl);
        hl++;
        if(m->hSerialIgnoringInitialData) {
            hl--;
            if(a == SERIAL_PREAMBLE_BYTE) m->hSerialIgnoringInitialData = false;
            continue;
        }
        *de++ = a;
        bc--;
    }
}

static void link_transfer_receive(MasterSim* m, uint8_t b) {
    /* LinkTransfer.Receive */
    m->wOtherPlayerLinkMode = m->hSerialReceive;
    if((m->hSerialReceive & 0xF0) != b) return;
    m->hSerialReceive = 0;
    m->wOtherPlayerLinkAction = m->wOtherPlayerLinkMode & 0x0F;
}

static void link_transfer(MasterSim* m) {
    uint8_t b;
    /* Every caller reaches here at least a frame (or a text box) after the
     * previous transfer was started, so its interrupt has long fired. */
    complete_transfer(m);
    if(m->wLinkMode == LINK_TIMECAPSULE || m->wLinkMode < LINK_TIMECAPSULE)
        b = SERIAL_TIMECAPSULE;
    else if(m->wLinkMode == LINK_TRADECENTER)
        b = SERIAL_TRADECENTER;
    else
        b = SERIAL_BATTLE;
    link_transfer_receive(m, b);
    m->hSerialSend = (uint8_t)(m->wPlayerLinkAction + b);
    start_transfer(m);
    link_transfer_receive(m, b);
}

static void link_data_received(MasterSim* m) {
    complete_transfer(m);
    m->hSerialSend = 0;
    start_transfer(m);
}

/* Returns false if wLinkTimeoutFrames ran out (SerialDisconnected). */
static bool wait_link_transfer(MasterSim* m) {
    m->wOtherPlayerLinkAction = 0xFF;
    for(;;) {
        link_transfer(m);
        delay_frame(m);
        if(m->wLinkTimeoutFrames) {
            if(--m->wLinkTimeoutFrames == 0) {
                m->wLinkTimeoutFrames = 0xFFFF;
                return false;
            }
        }
        if(m->wOtherPlayerLinkAction != 0xFF) break;
    }
    for(int b = 10; b; b--) {
        delay_frame(m);
        link_transfer(m);
    }
    for(int b = 10; b; b--) {
        delay_frame(m);
        link_data_received(m);
    }
    m->wOtherPlayerLinkMode = m->wOtherPlayerLinkAction;
    return true;
}

/* ---------------- engine/link/link.asm ---------------- */

static bool link_check_communication_error(MasterSim* m) {
    uint16_t saved = m->wLinkTimeoutFrames;
    bool ok = wait_link_transfer(m) && m->wLinkTimeoutFrames != 0xFFFF;
    if(!ok) {
        for(int b = 10; b; b--) {
            delay_frame(m);
            link_data_received(m);
        }
        /* .ConvertDW */
        m->wLinkTimeoutFrames = (uint16_t)(((saved - 0x100) >> 2) + 0x100);
        ok = wait_link_transfer(m) && m->wLinkTimeoutFrames != 0xFFFF;
        if(!ok) {
            for(int b = 10; b; b--) {
                delay_frame(m);
                link_data_received(m);
            }
        }
    }
    m->wLinkTimeoutFrames = 0;
    return ok;
}

static bool wait_for_linked_friend(MasterSim* m) {
    /* wPlayerLinkAction != 0: listen with rSB = $02 for 3 frames (we never
     * clock, so nothing happens) */
    ms_delay_frames(m, 3);
    for(int tries = 0; tries < 0x2FF; tries++) {
        if(m->hSerialConnectionStatus == USING_INTERNAL_CLOCK) {
            link_data_received(m);
            delay_frame(m);
            link_data_received(m);
            ms_delay_frames(m, 50);
            return true;
        }
        m->hSerialConnectionStatus = CONNECTION_NOT_ESTABLISHED;
        /* ld a, USING_EXTERNAL_CLOCK ; ldh [rSB], a ; internal start */
        m->rSB = USING_EXTERNAL_CLOCK;
        uint8_t in = clock_byte(m, m->rSB);
        /* Serial .establish_connection */
        if(in == USING_INTERNAL_CLOCK) {
            m->hSerialReceive = in;
            m->hSerialConnectionStatus = in;
            m->rSB = 0;
        } else if(in == USING_EXTERNAL_CLOCK) {
            m->error = "slave answered $01: master would become the slave";
            return false;
        }
        m->hSerialReceivedNewData = true;
        m->hSerialSend = SERIAL_NO_DATA_BYTE;
        delay_frame(m);
    }
    m->error = "WaitForLinkedFriend timed out";
    return false;
}

static uint8_t link_ensure_sync(MasterSim* m, uint8_t a) {
    uint8_t player[5] = {(uint8_t)(a + 0xD0), (uint8_t)(a + 0xD0), 0, 0, 0};
    uint8_t received[2];
    ms_delay_frames(m, 2);
    for(int attempt = 0; attempt < 1000; attempt++) {
        /* Serial_ExchangeSyncBytes */
        const uint8_t* hl = player;
        uint8_t* de = received;
        int c = 2;
        m->hSerialIgnoringInitialData = true;
        while(c) {
            delay_frame(m);
            m->hSerialSend = *hl;
            uint8_t b = serial_exchange_byte(m, *hl);
            hl++;
            bool ignoring = m->hSerialIgnoringInitialData;
            m->hSerialIgnoringInitialData = false;
            if(ignoring) continue;
            *de++ = b;
            c--;
        }
        if((received[0] & 0xF0) == 0xD0) return received[0] & 0x0F;
        if((received[1] & 0xF0) == 0xD0) return received[1] & 0x0F;
    }
    m->error = "Link_EnsureSync never synced";
    return 0xFF;
}

bool ms_receptionist(MasterSim* m, uint32_t save_prompt_frames) {
    /* SetBitsForBattleRequest */
    m->wPlayerLinkAction = CABLECLUBROOM_COLOSSEUM;
    m->wLinkMode = LINK_NULL;
    if(!wait_for_linked_friend(m)) return false;
    /* "must save the game" + TryQuickSave */
    ms_delay_frames(m, save_prompt_frames + 30);
    /* CheckLinkTimeout_Receptionist */
    m->wPlayerLinkAction = 1;
    m->wLinkTimeoutFrames = 0x0300;
    ms_delay_frames(m, 2);
    if(!link_check_communication_error(m)) {
        m->error = "receptionist link check timed out";
        return false;
    }
    if(m->wOtherPlayerLinkMode == 0) {
        m->error = "LinkedToFirstGen";
        return false;
    }
    /* CheckBothSelectedSameRoom */
    uint8_t room = link_ensure_sync(m, CABLECLUBROOM_COLOSSEUM);
    link_data_received(m);
    delay_frame(m);
    link_data_received(m);
    if(room != CABLECLUBROOM_COLOSSEUM) {
        if(!m->error) m->error = "IncompatibleRooms";
        return false;
    }
    m->wLinkMode = CABLECLUBROOM_COLOSSEUM + 1;
    /* "Please come in" and walking to the console */
    ms_delay_frames(m, 120);
    return true;
}

static bool check_link_timeout_gen2(MasterSim* m) {
    m->wPlayerLinkAction = 5;
    m->wLinkTimeoutFrames = 0x0300;
    ms_delay_frames(m, 2);
    if(!link_check_communication_error(m)) {
        m->error = "console check 1 timed out";
        return false;
    }
    m->now_us += 110000; /* ld bc, $ffff busy loop */
    if(m->wOtherPlayerLinkMode != 5) {
        m->error = "console check 1: other player not ready ($85)";
        return false;
    }
    m->wPlayerLinkAction = 6;
    m->wLinkTimeoutFrames = 0x0132;
    if(!link_check_communication_error(m)) {
        m->error = "console check 2 timed out";
        return false;
    }
    if(m->wOtherPlayerLinkMode != 6) {
        m->error = "console check 2: other player not ready ($86)";
        return false;
    }
    return true;
}

bool ms_console(
    MasterSim* m,
    const uint8_t rn_tx[PKB_RN_BLOCK_LEN],
    const uint8_t party_tx[PKB_PARTY_BLOCK_LEN],
    const uint8_t patch_tx[PKB_PATCH_BLOCK_LEN],
    uint8_t rn_rx[PKB_RN_BLOCK_LEN],
    uint8_t party_rx[PKB_PARTY_BLOCK_LEN],
    uint8_t patch_rx[PKB_PATCH_BLOCK_LEN]) {
    /* special Colosseum -> LinkCommunications */
    m->wLinkMode = LINK_COLOSSEUM;
    ms_delay_frames(m, 160);
    /* Gen2ToGen2LinkComms */
    if(!check_link_timeout_gen2(m)) return false;
    /* USING_INTERNAL_CLOCK: two raw $00 transfers */
    ms_delay_frames(m, 3);
    m->hSerialSend = 0;
    start_transfer(m);
    delay_frame(m);
    m->hSerialSend = 0;
    start_transfer(m);
    ms_delay_frames(m, 3);
    complete_transfer(m);
    m->ie_serial_only = true;
    serial_exchange_bytes(m, rn_tx, rn_rx, PKB_RN_BLOCK_LEN);
    serial_exchange_bytes(m, party_tx, party_rx, PKB_PARTY_BLOCK_LEN);
    serial_exchange_bytes(m, patch_tx, patch_rx, PKB_PATCH_BLOCK_LEN);
    m->ie_serial_only = false;
    /* copy data, 66 frames (USING_INTERNAL_CLOCK), StartBattle intro */
    ms_delay_frames(m, 66 + 240);
    return true;
}

uint8_t ms_battle_exchange(MasterSim* m, uint8_t my_nibble) {
    /* .LinkBattle_SendReceiveAction: no timeout */
    m->wPlayerLinkAction = my_nibble & 0x0F;
    m->wOtherPlayerLinkAction = 0xFF;
    for(uint32_t guard = 0; m->wOtherPlayerLinkAction == 0xFF; guard++) {
        link_transfer(m);
        delay_frame(m);
        if(guard > 100000) {
            m->error = "battle exchange never completed";
            return 0xFF;
        }
    }
    for(int b = 10; b; b--) {
        delay_frame(m);
        link_transfer(m);
    }
    for(int b = 10; b; b--) {
        delay_frame(m);
        link_data_received(m);
    }
    return m->wOtherPlayerLinkAction;
}

void ms_exit_link_communications(MasterSim* m) {
    m->rSB = 0;
    m->hSerialSend = 0;
    start_transfer(m);
    delay_frame(m);
}

void ms_wait_for_other_player_to_exit(MasterSim* m) {
    ms_delay_frames(m, 3);
    m->hSerialConnectionStatus = CONNECTION_NOT_ESTABLISHED;
    m->rSB = 0;
    m->hSerialReceive = 0;
    start_transfer(m); /* internal clock: one transfer of $00 */
    ms_delay_frames(m, 3);
    /* external-clock listen, then idle: no transfers from our side */
    m->rSB = 0;
    m->hSerialReceive = 0;
    ms_delay_frames(m, 6);
    m->hSerialConnectionStatus = CONNECTION_NOT_ESTABLISHED;
    m->wLinkTimeoutFrames = 0;
    m->wLinkMode = LINK_NULL;
}
