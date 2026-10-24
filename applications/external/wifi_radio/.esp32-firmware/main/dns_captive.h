#pragma once
// Minimal DNS server that answers every A query with 192.168.4.1, so joining
// phones trigger the OS captive-portal popup. Phase 1.
void dns_captive_start(void);
