// ============================================================================
//  net_w6300.c - síťová vrstva: Ethernet čip W6300, DHCP, webový a Modbus server
//
//  Jak to funguje (stavový automat):
//
//    NET_NO_CHIP  čip neodpověděl na SPI (špatné zapojení/pájení)  -> síť se nepoužije
//    NET_NO_LINK  čip je v pořádku, ale není zapojený síťový kabel  -> čekáme na kabel
//    NET_DHCP     kabel je zapojený, čekáme na IP adresu od routeru
//    NET_UP       síť běží, obsluhujeme web (port 80) a Modbus TCP (port 502)
//
//  Funkce net_poll() se volá pořád dokola z hlavní smyčky. Žádná funkce tady
//  nečeká (neblokuje), takže senzor a displej fungují i bez připojené sítě.
// ============================================================================
#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pico/unique_id.h"

#include "ethchip_conf.h"   // knihovna čipu: eth_NetInfo, ctlethchip(), getCIDR(), ...
#include "ethchip_spi.h"    // inicializace SPI/GPIO a síťových informací (z ukázky Waveshare)
#include "socket.h"         // socket(), listen(), recv(), send(), ...
#include "dhcp.h"           // DHCP klient

#include "config.h"
#include "sensor_data.h"
#include "modbus.h"
#include "web.h"
#include "net_w6300.h"

// Sdílená data z main.c: poslední naměřené hodnoty a Modbus registry.
extern sensor_data_t g_data;
extern uint16_t g_regs[MODBUS_NREGS];

// --- Stavový automat ---------------------------------------------------------
typedef enum { NET_NO_CHIP, NET_NO_LINK, NET_DHCP, NET_UP } net_state_t;
static net_state_t s_state = NET_NO_CHIP;

// Síťové informace (MAC, IP, maska, brána, DNS...). U DHCP se IP vyplní až po přidělení.
static eth_NetInfo s_net = {
    .mac    = {0x02, 0x08, 0xDC, 0x00, 0x00, 0x00},   // poslední 3 bajty se doplní z ID čipu RP2350
    .ip     = {0, 0, 0, 0},
    .sn     = {0, 0, 0, 0},
    .gw     = {0, 0, 0, 0},
    .dns    = {0, 0, 0, 0},
    .lla    = {0},                                    // IPv6 nepoužíváme, necháme nuly
    .gua    = {0},
    .sn6    = {0},
    .gw6    = {0},
    .dns6   = {0},
    .dhcp   = NETINFO_STATIC,
    .ipmode = NETINFO_STATIC_V4,
};

// Pomocné vyrovnávací paměti. DHCP knihovna potřebuje větší buffer (2 kB).
static uint8_t s_dhcp_buf[2048];
static uint8_t s_rx[1024];     // přijatý požadavek (HTTP nebo Modbus)
static uint8_t s_tx[1280];     // odpověď

// ----------------------------------------------------------------------------
//  Pomocné funkce
// ----------------------------------------------------------------------------

// Je zapojený síťový kabel (PHY link)? Vrací true, pokud ano.
static bool link_is_up(void) {
    uint8_t link = PHY_LINK_OFF;
    if (ctlethchip(CW_GET_PHYLINK, (void *)&link) == -1) return false;
    return link == PHY_LINK_ON;
}

// Zavře všechny sokety našich serverů (při odpojení kabelu nebo změně IP).
static void close_servers(void) {
    const uint8_t socks[] = {SOCK_HTTP_A, SOCK_HTTP_B, SOCK_MODBUS_A, SOCK_MODBUS_B};
    for (size_t i = 0; i < sizeof socks; i++) close(socks[i]);
}

// Zapíše do čipu nastavení sítě (MAC, IP, maska, brána) a vypíše ho na sériový výstup.
static void apply_network(void) {
    network_initialize(s_net);
    print_network_information(s_net);
}

// DHCP: zavolá se, když router přidělí (nebo změní) IP adresu.
static void on_dhcp_assign(void) {
    // Vyzvedneme přidělené hodnoty z DHCP klienta.
    getIPfromDHCP(s_net.ip);
    getGWfromDHCP(s_net.gw);
    getSNfromDHCP(s_net.sn);
    getDNSfromDHCP(s_net.dns);
    s_net.dhcp = NETINFO_DHCP;
    s_net.ipmode = NETINFO_DHCP_V4;

    close_servers();            // staré spojení by s novou adresou nefungovalo
    apply_network();
    printf("DHCP: pridelena adresa %d.%d.%d.%d\n", s_net.ip[0], s_net.ip[1], s_net.ip[2], s_net.ip[3]);
    s_state = NET_UP;
}

// DHCP: kolize adresy (stejnou IP už používá jiné zařízení).
static void on_dhcp_conflict(void) {
    printf("DHCP: kolize IP adresy, zkusim znovu\n");
    s_state = NET_DHCP;
}

// Spustí získání IP adresy (DHCP) nebo nastaví statickou, podle config.h.
static void start_ip_config(void) {
#if NET_USE_DHCP
    // MAC musí být v čipu nastavená dřív, než se spustí DHCP (klient si ji z čipu přečte).
    memset(s_net.ip, 0, 4);
    network_initialize(s_net);
    DHCP_init(0, s_dhcp_buf);                       // DHCP používá socket 0
    reg_dhcp_cbfunc(on_dhcp_assign, on_dhcp_assign, on_dhcp_conflict);
    s_state = NET_DHCP;
    printf("Kabel pripojen, zadam o IP adresu pres DHCP...\n");
#else
    const uint8_t ip[4] = NET_STATIC_IP, mask[4] = NET_STATIC_MASK;
    const uint8_t gw[4] = NET_STATIC_GW, dns[4] = NET_STATIC_DNS;
    memcpy(s_net.ip, ip, 4);  memcpy(s_net.sn, mask, 4);
    memcpy(s_net.gw, gw, 4);  memcpy(s_net.dns, dns, 4);
    s_net.dhcp = NETINFO_STATIC;
    apply_network();
    s_state = NET_UP;
#endif
}

// ----------------------------------------------------------------------------
//  Inicializace
// ----------------------------------------------------------------------------
void w6300_init(void) {
    // MAC adresa: pevný prefix 02:08:DC (lokálně spravovaná adresa) + 3 bajty z unikátního
    // ID čipu RP2350. Každá deska tak má vlastní, stále stejnou MAC adresu.
    pico_unique_board_id_t id;
    pico_get_unique_board_id(&id);
    s_net.mac[3] = id.id[5];
    s_net.mac[4] = id.id[6];
    s_net.mac[5] = id.id[7];

    ethchip_spi_initialize();      // SPI0 a vývody CS
    ethchip_cris_initialize();     // zámek proti souběžnému přístupu k čipu
    ethchip_reset();               // hardwarový reset čipu (vývod RST)
    ethchip_initialize();          // nastavení velikostí bufferů socketů (neblokuje)

    if (!ethchip_check()) {        // ověření, že čip odpovídá (ID 0x6300)
        printf("W6300: cip neodpovida, siet nebude fungovat (zkontroluj desku)\n");
        s_state = NET_NO_CHIP;
        return;
    }
    printf("W6300: cip OK, MAC %02X:%02X:%02X:%02X:%02X:%02X\n",
           s_net.mac[0], s_net.mac[1], s_net.mac[2], s_net.mac[3], s_net.mac[4], s_net.mac[5]);
    s_state = NET_NO_LINK;         // dál čekáme na kabel (kontroluje net_poll)
}

// ----------------------------------------------------------------------------
//  Obsluha jednoho TCP serveru na jednom socketu
// ----------------------------------------------------------------------------

// Odešle celý blok dat. Čip může odeslat jen část nebo být chvíli zaneprázdněn,
// proto opakujeme, nejdéle 300 ms. Vrací true při úspěchu.
static bool send_all(uint8_t sn, const uint8_t *data, size_t len) {
    size_t sent = 0;
    absolute_time_t deadline = make_timeout_time_ms(300);
    while (sent < len) {
        int32_t r = send(sn, (uint8_t *)(data + sent), (uint16_t)(len - sent));
        if (r < 0) { close(sn); return false; }        // chyba spojení
        sent += (size_t)r;                               // r může být 0 = čip je zaneprázdněn
        if (sent < len && absolute_time_diff_us(get_absolute_time(), deadline) < 0) return false;
    }
    return true;
}

// Zpracuje přijatá data na socketu: buď HTTP požadavek, nebo Modbus TCP rámec(e).
static void handle_data(uint8_t sn, bool is_http) {
    uint16_t avail = 0;
    getsockopt(sn, SO_RECVBUF, &avail);                  // kolik bajtů čeká v čipu
    if (avail == 0) return;
    if (avail > sizeof s_rx - 1) avail = sizeof s_rx - 1;

    int32_t n = recv(sn, s_rx, avail);                   // přečteme data z čipu
    if (n <= 0) return;
    s_rx[n] = 0;                                         // ukončíme jako text (pro HTTP)

    if (is_http) {
        // Sestavíme odpověď (HTML nebo JSON) a odešleme ji. Po odeslání spojení zavřeme
        // (HTTP/1.1 s "Connection: close"), čímž se socket uvolní pro dalšího klienta.
        size_t m = http_handle((const char *)s_rx, (char *)s_tx, sizeof s_tx, &g_data);
        if (m > 0) send_all(sn, s_tx, m);
        disconnect(sn);
    } else {
        // Modbus TCP: v jednom TCP segmentu může být i víc rámců za sebou.
        // Délka rámce = 6 bajtů hlavičky MBAP + hodnota v bajtech 4 a 5.
        size_t pos = 0;
        while ((size_t)n - pos >= 8) {
            size_t flen = 6u + (((size_t)s_rx[pos + 4] << 8) | s_rx[pos + 5]);
            if (flen > (size_t)n - pos) break;           // neúplný rámec, zahodíme zbytek
            size_t m = modbus_handle(&s_rx[pos], flen, s_tx, g_regs, MODBUS_NREGS);
            if (m > 0 && !send_all(sn, s_tx, m)) break;
            pos += flen;
        }
    }
}

// Jeden průchod "životním cyklem" serverového socketu. Volá se pořád dokola.
//   CLOSED       -> otevřeme socket na daném portu
//   INIT         -> přepneme do režimu poslouchání (listen)
//   ESTABLISHED  -> klient je připojený, zpracujeme jeho data
//   CLOSE_WAIT   -> klient spojení ukončil, přečteme zbytek dat a uzavřeme
static void serve_socket(uint8_t sn, uint16_t port, bool is_http) {
    uint8_t status = 0;
    getsockopt(sn, SO_STATUS, &status);

    switch (status) {
    case SOCK_CLOSED:
        socket(sn, Sn_MR_TCP4, port, SOCK_IO_NONBLOCK);  // TCP přes IPv4, neblokující režim
        break;
    case SOCK_INIT:
        listen(sn);
        break;
    case SOCK_ESTABLISHED: {
        uint8_t inter = 0;
        ctlsocket(sn, CS_GET_INTERRUPT, &inter);
        if (inter & Sn_IR_CON) {                         // nové spojení: potvrdíme příznak
            uint8_t clr = Sn_IR_CON;
            ctlsocket(sn, CS_CLR_INTERRUPT, &clr);
        }
        handle_data(sn, is_http);
        break;
    }
    case SOCK_CLOSE_WAIT:
        handle_data(sn, is_http);                        // klient mohl poslat dotaz a hned zavřít
        disconnect(sn);
        break;
    default:
        break;
    }
}

// ----------------------------------------------------------------------------
//  Hlavní funkce sítě, volá se z main smyčky
// ----------------------------------------------------------------------------
void net_poll(void) {
    if (s_state == NET_NO_CHIP) return;                  // bez čipu není co dělat

    // Jednou za sekundu: kontrola kabelu a "tikání" DHCP klienta.
    static uint32_t last_ms = 0;
    uint32_t now = to_ms_since_boot(get_absolute_time());
    if (now - last_ms >= 1000) {
        last_ms = now;
#if NET_USE_DHCP
        DHCP_time_handler();                             // DHCP knihovna potřebuje sekundový tik
#endif
        bool link = link_is_up();
        if (!link && s_state != NET_NO_LINK) {           // kabel byl odpojen
            printf("Kabel odpojen\n");
            close_servers();
#if NET_USE_DHCP
            DHCP_stop();
#endif
            s_state = NET_NO_LINK;
        } else if (link && s_state == NET_NO_LINK) {     // kabel byl připojen
            start_ip_config();
        }
    }

#if NET_USE_DHCP
    // DHCP klienta voláme pořád (při získávání adresy i při pozdějším obnovování adresy).
    if (s_state == NET_DHCP || s_state == NET_UP) {
        uint8_t r = DHCP_run();
        if (r == DHCP_FAILED && s_state == NET_DHCP) {
            printf("DHCP: server neodpovida, zkusim znovu\n");
        }
    }
#endif

    if (s_state == NET_UP) {
        serve_socket(SOCK_HTTP_A,   HTTP_PORT,   true);
        serve_socket(SOCK_HTTP_B,   HTTP_PORT,   true);
        serve_socket(SOCK_MODBUS_A, MODBUS_PORT, false);
        serve_socket(SOCK_MODBUS_B, MODBUS_PORT, false);
    }
}

// ----------------------------------------------------------------------------
//  Text o stavu sítě pro displej
// ----------------------------------------------------------------------------
const char *net_status_text(void) {
    static char buf[20];
    switch (s_state) {
    case NET_NO_CHIP: return "NO ETH";
    case NET_NO_LINK: return "NO LINK";
    case NET_DHCP:    return "DHCP";
    default:
        snprintf(buf, sizeof buf, "%d.%d.%d.%d", s_net.ip[0], s_net.ip[1], s_net.ip[2], s_net.ip[3]);
        return buf;
    }
}
