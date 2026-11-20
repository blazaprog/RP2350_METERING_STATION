// ============================================================================
//  net_w6300.h - síťová vrstva (Ethernet čip W6300)
// ============================================================================
#pragma once
#include <stddef.h>

// Inicializuje SPI, resetuje a nastaví čip W6300. Nikdy neblokuje (funguje i bez kabelu).
void w6300_init(void);

// Zavolej co nejčastěji z hlavní smyčky. Stará se o kontrolu kabelu, DHCP
// a obsluhu webového serveru a Modbus TCP serveru.
void net_poll(void);

// Krátký text o stavu sítě pro displej: "NO LINK", "DHCP" nebo IP adresa (např. 192.168.1.50).
// Vrácený ukazatel platí do dalšího volání.
const char *net_status_text(void);

// Adresy jako text pro web. IP adresa platí, jakmile síť běží (stav NET_UP).
const char *net_ip_text(void);
const char *net_mac_text(void);
