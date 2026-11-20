// ============================================================================
//  net_w6300.c - síťová vrstva: Ethernet čip W6300, DHCP, webový, Modbus a servisní server
//
//  Jak to funguje (stavový automat):
//
//    NET_NO_CHIP  čip neodpověděl na SPI (špatné zapojení/pájení)  -> síť se nepoužije
//    NET_NO_LINK  čip je v pořádku, ale není zapojený síťový kabel  -> čekáme na kabel
//    NET_DHCP     kabel je zapojený, čekáme na IP adresu od routeru
//    NET_UP       síť běží, obsluhujeme web (port 80), Modbus TCP (502) a servisní kanál
//
//  Funkce net_poll() se volá pořád dokola z hlavní smyčky. Žádná funkce tady
//  nečeká (neblokuje), takže senzor a displej fungují i bez připojené sítě.
// ============================================================================
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
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
#include "settings.h"
#include "diag.h"
#include "service.h"
#include "net_w6300.h"

// Sdílená data z main.c: poslední naměřené hodnoty a Modbus registry.
extern sensor_data_t g_data;
extern uint16_t g_regs[MODBUS_NREGS];
extern int g_regs_write(uint16_t addr, uint16_t value, bool commit);   // zápis do registrů Modbus (main.c)

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
static uint8_t s_mb_rx[300];   // přijatý Modbus rámec (největší možný Modbus TCP rámec má 260 bajtů)
static uint8_t s_tx[16384];    // odpověď (webová stránka a JSON s historií mají několik kB)

// HTTP požadavek se může přijít na několik částí (hlavičky zvlášť, tělo POST zvlášť), proto má
// každý HTTP socket vlastní buffer, do kterého se data skládají, dokud požadavek není kompletní.
#define HTTP_RX_MAX 1536
typedef struct { uint8_t buf[HTTP_RX_MAX]; size_t len; uint32_t first_ms; } http_rx_t;
static http_rx_t s_http_rx[2];                       // [0] pro SOCK_HTTP_A, [1] pro SOCK_HTTP_B

// Servisní kanál: jeden klient, řádek se skládá po znacích.
static svc_session_t s_svc;
static char s_svc_line[96];
static size_t s_svc_len;
static uint32_t s_svc_last_ms;
static bool s_svc_skip_telnet;                       // právě přeskakujeme zbytek telnetového povelu (0xFF + 2 bajty)
static int s_svc_telnet_left;
static uint32_t s_reboot_at_ms;                      // plánovaný restart (0 = žádný)

// Plánované přenastavení sítě po změně nastavení (0 = žádné) a příznak, zda běží DHCP klient.
static uint32_t s_reconfig_at_ms;
static bool s_dhcp_active;
static uint32_t s_last_tick_ms;

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
    const uint8_t socks[] = {SOCK_HTTP_A, SOCK_HTTP_B, SOCK_MODBUS_A, SOCK_MODBUS_B, SOCK_SERVICE};
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

// Spustí získání IP adresy (DHCP) nebo nastaví statickou, podle nastavení (g_set).
static void start_ip_config(void) {
    if (g_set.dhcp) {
        // MAC musí být v čipu nastavená dřív, než se spustí DHCP (klient si ji z čipu přečte).
        memset(s_net.ip, 0, 4);
        network_initialize(s_net);
        DHCP_init(0, s_dhcp_buf);                   // DHCP používá socket 0
        reg_dhcp_cbfunc(on_dhcp_assign, on_dhcp_assign, on_dhcp_conflict);
        s_dhcp_active = true;
        s_state = NET_DHCP;
        printf("Kabel pripojen, zadam o IP adresu pres DHCP...\n");
    } else {
        memcpy(s_net.ip, g_set.ip, 4);  memcpy(s_net.sn, g_set.mask, 4);
        memcpy(s_net.gw, g_set.gw, 4);  memcpy(s_net.dns, g_set.dns, 4);
        s_net.dhcp = NETINFO_STATIC;
        s_net.ipmode = NETINFO_STATIC_V4;
        s_dhcp_active = false;
        apply_network();
        s_state = NET_UP;
    }
}

// Přenastaví síť podle aktuálního nastavení: zavře sockety, zastaví DHCP a nechá síť znovu naběhnout.
static void do_reconfig(void) {
    close_servers();
    if (s_dhcp_active) DHCP_stop();
    s_dhcp_active = false;
    if (s_state != NET_NO_CHIP) {
        s_state = NET_NO_LINK;                      // další sekundový tik zjistí kabel a spustí start_ip_config()
        s_last_tick_ms = 0;
    }
    printf("Sit se prenastavuje podle noveho nastaveni\n");
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
// proto opakujeme, nejdéle 1 s (větší odpověď se do čipu vejde po částech, jak se uvolňuje jeho vyrovnávací paměť). Vrací true při úspěchu.
static bool send_all(uint8_t sn, const uint8_t *data, size_t len) {
    size_t sent = 0;
    absolute_time_t deadline = make_timeout_time_ms(1000);
    while (sent < len) {
        int32_t r = send(sn, (uint8_t *)(data + sent), (uint16_t)(len - sent));
        if (r < 0) { close(sn); return false; }        // chyba spojení
        sent += (size_t)r;                               // r může být 0 = čip je zaneprázdněn
        if (sent < len && absolute_time_diff_us(get_absolute_time(), deadline) < 0) return false;
    }
    return true;
}

// Najde v hlavičkách HTTP požadavku hodnotu Content-Length (0, když chybí).
static size_t http_content_length(const char *hdrs) {
    for (const char *p = hdrs; *p; p++) {
        if ((p == hdrs || p[-1] == '\n') && strncasecmp(p, "Content-Length:", 15) == 0)
            return (size_t)strtoul(p + 15, NULL, 10);
    }
    return 0;
}

// HTTP: přijatá data se skládají do bufferu socketu, dokud není požadavek kompletní (hlavičky a u POST
// i celé tělo). Pak se odpoví a spojení se zavře ("Connection: close"), čímž se socket uvolní.
// force = true: klient už spojení ukončil, zpracujeme co máme.
static void handle_http(uint8_t sn, bool force) {
    http_rx_t *c = &s_http_rx[sn == SOCK_HTTP_A ? 0 : 1];
    uint32_t now = to_ms_since_boot(get_absolute_time());

    uint16_t avail = 0;
    getsockopt(sn, SO_RECVBUF, &avail);                  // kolik bajtů čeká v čipu
    size_t room = HTTP_RX_MAX - 1 - c->len;
    if (avail > room) avail = (uint16_t)room;
    if (avail > 0) {
        int32_t n = recv(sn, c->buf + c->len, avail);
        if (n > 0) { if (c->len == 0) c->first_ms = now; c->len += (size_t)n; }
    }
    if (c->len == 0) { if (force) disconnect(sn); return; }
    c->buf[c->len] = 0;                                  // ukončíme jako text

    // Je požadavek kompletní? Hlavičky končí prázdným řádkem, u POST musí dorazit i celé tělo.
    char *hdr_end = strstr((char *)c->buf, "\r\n\r\n");
    bool complete = false;
    if (hdr_end) {
        size_t body_off = (size_t)(hdr_end - (char *)c->buf) + 4;
        size_t need = body_off + http_content_length((char *)c->buf);
        complete = c->len >= need || need > HTTP_RX_MAX - 1;   // příliš velký požadavek odbavíme tím, co se vešlo
    }
    if (!complete && !force && c->len < HTTP_RX_MAX - 1) {
        if (now - c->first_ms > 3000) { c->len = 0; disconnect(sn); }   // klient se nedočkal: zrušíme
        return;                                          // čekáme na další data
    }

    web_info_t info = {
        .d = &g_data,
        .ip = net_ip_text(),
        .mac = net_mac_text(),
        .uptime_s = now / 1000,
    };
    size_t m;
    const char *out = http_handle((const char *)c->buf, (char *)s_tx, sizeof s_tx, &info, &m);
    g_diag.http_req++;
    if (m > 0) send_all(sn, (const uint8_t *)out, m);
    c->len = 0;
    disconnect(sn);
}

// Modbus TCP: v jednom TCP segmentu může být i víc rámců za sebou.
static void handle_modbus(uint8_t sn) {
    uint16_t avail = 0;
    getsockopt(sn, SO_RECVBUF, &avail);
    if (avail == 0) return;
    if (avail > sizeof s_mb_rx) avail = sizeof s_mb_rx;
    int32_t n = recv(sn, s_mb_rx, avail);
    if (n <= 0) return;

    // Délka rámce = 6 bajtů hlavičky MBAP + hodnota v bajtech 4 a 5.
    size_t pos = 0;
    while ((size_t)n - pos >= 8) {
        size_t flen = 6u + (((size_t)s_mb_rx[pos + 4] << 8) | s_mb_rx[pos + 5]);
        if (flen < 8 || flen > 260 || flen > (size_t)n - pos) break;   // neplatný nebo neúplný rámec, zbytek zahodíme
        size_t m = modbus_handle(&s_mb_rx[pos], flen, s_tx, g_regs, MODBUS_NREGS, g_regs_write, g_set.mb_unit);
        if (m > 0) {
            g_diag.modbus_req++;
            if (s_tx[7] & 0x80) g_diag.modbus_exc++;     // odpověď s chybou
            if (!send_all(sn, s_tx, m)) break;
        }
        pos += flen;
    }
}

// Servisní kanál: textové příkazy po řádcích (viz service.h). Jeden klient, po 2 minutách ticha se odpojí.
static void svc_send(uint8_t sn, const char *text) { send_all(sn, (const uint8_t *)text, strlen(text)); }

static void handle_service(uint8_t sn, bool new_connection) {
    uint32_t now = to_ms_since_boot(get_absolute_time());
    if (new_connection) {
        s_svc.authed = false; s_svc_len = 0; s_svc_telnet_left = 0; s_svc_last_ms = now;
        svc_send(sn, service_banner());
        svc_send(sn, "> ");
        return;
    }
    if (now - s_svc_last_ms > 120000) { svc_send(sn, "\r\nTimeout\r\n"); disconnect(sn); return; }

    uint16_t avail = 0;
    getsockopt(sn, SO_RECVBUF, &avail);
    if (avail == 0) return;
    uint8_t buf[64];
    if (avail > sizeof buf) avail = sizeof buf;
    int32_t n = recv(sn, buf, avail);
    if (n <= 0) return;
    s_svc_last_ms = now;

    for (int32_t i = 0; i < n; i++) {
        uint8_t ch = buf[i];
        if (s_svc_telnet_left > 0) { s_svc_telnet_left--; continue; }      // zbytek telnetového povelu
        if (ch == 0xFF) { s_svc_telnet_left = 2; continue; }                // začátek telnetového povelu: přeskočíme
        if (ch == '\r' || ch == '\n') {
            if (s_svc_len == 0) continue;
            s_svc_line[s_svc_len] = 0;
            s_svc_len = 0;
            // Výstup se sestaví do s_tx (servisní kanál se obsluhuje mimo HTTP, takže se buffery nepřekrývají).
            svc_action_t act = service_exec(&s_svc, s_svc_line, (char *)s_tx, sizeof s_tx);
            svc_send(sn, (const char *)s_tx);
            if (act == SVC_CLOSE) { disconnect(sn); return; }
            if (act == SVC_REBOOT) { s_reboot_at_ms = now + 500; return; }   // restart po chvíli, aby odešla odpověď
            svc_send(sn, "> ");
        } else if (s_svc_len < sizeof s_svc_line - 1 && ch >= 0x20 && ch < 0x7F) {
            s_svc_line[s_svc_len++] = (char)ch;
        }
    }
}

// Jeden průchod "životním cyklem" serverového socketu. Volá se pořád dokola.
//   CLOSED       -> otevřeme socket na daném portu
//   INIT         -> přepneme do režimu poslouchání (listen)
//   ESTABLISHED  -> klient je připojený, zpracujeme jeho data
//   CLOSE_WAIT   -> klient spojení ukončil, přečteme zbytek dat a uzavřeme
typedef enum { KIND_HTTP, KIND_MODBUS, KIND_SERVICE } sock_kind_t;

static void serve_socket(uint8_t sn, uint16_t port, sock_kind_t kind) {
    uint8_t status = 0;
    getsockopt(sn, SO_STATUS, &status);

    switch (status) {
    case SOCK_CLOSED:
        if (kind == KIND_HTTP) s_http_rx[sn == SOCK_HTTP_A ? 0 : 1].len = 0;   // nové spojení začne s prázdným bufferem
        socket(sn, Sn_MR_TCP4, port, SOCK_IO_NONBLOCK);  // TCP přes IPv4, neblokující režim
        break;
    case SOCK_INIT:
        listen(sn);
        break;
    case SOCK_ESTABLISHED: {
        uint8_t inter = 0;
        ctlsocket(sn, CS_GET_INTERRUPT, &inter);
        bool is_new = (inter & Sn_IR_CON) != 0;
        if (is_new) {                                    // nové spojení: potvrdíme příznak
            uint8_t clr = Sn_IR_CON;
            ctlsocket(sn, CS_CLR_INTERRUPT, &clr);
            if (kind == KIND_HTTP) s_http_rx[sn == SOCK_HTTP_A ? 0 : 1].len = 0;
        }
        if (kind == KIND_HTTP)         handle_http(sn, false);
        else if (kind == KIND_MODBUS)  handle_modbus(sn);
        else                           handle_service(sn, is_new);
        break;
    }
    case SOCK_CLOSE_WAIT:
        if (kind == KIND_HTTP)         handle_http(sn, true);   // klient mohl poslat dotaz a hned zavřít
        else if (kind == KIND_MODBUS)  handle_modbus(sn);
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
    uint32_t now = to_ms_since_boot(get_absolute_time());

    // Plánovaný restart (z servisního kanálu): čekáme chvíli, aby se stihla odeslat odpověď.
    if (s_reboot_at_ms && (int32_t)(now - s_reboot_at_ms) >= 0) diag_reboot();

    // Plánované přenastavení sítě po změně nastavení (čeká se, aby odešla odpověď na požadavek, který ho vyvolal).
    if (s_reconfig_at_ms && (int32_t)(now - s_reconfig_at_ms) >= 0) { s_reconfig_at_ms = 0; do_reconfig(); }

    if (s_state == NET_NO_CHIP) return;                  // bez čipu není co dělat

    // Jednou za sekundu: kontrola kabelu a "tikání" DHCP klienta.
    if (now - s_last_tick_ms >= 1000) {
        s_last_tick_ms = now;
        if (s_dhcp_active) DHCP_time_handler();          // DHCP knihovna potřebuje sekundový tik
        bool link = link_is_up();
        if (!link && s_state != NET_NO_LINK) {           // kabel byl odpojen
            printf("Kabel odpojen\n");
            g_diag.link_changes++;
            close_servers();
            if (s_dhcp_active) DHCP_stop();
            s_dhcp_active = false;
            s_state = NET_NO_LINK;
        } else if (link && s_state == NET_NO_LINK) {     // kabel byl připojen
            g_diag.link_changes++;
            start_ip_config();
        }
    }

    // DHCP klienta voláme pořád (při získávání adresy i při pozdějším obnovování adresy).
    if (s_dhcp_active && (s_state == NET_DHCP || s_state == NET_UP)) {
        uint8_t r = DHCP_run();
        if (r == DHCP_FAILED && s_state == NET_DHCP) {
            printf("DHCP: server neodpovida, zkusim znovu\n");
        }
    }

    if (s_state == NET_UP) {
        serve_socket(SOCK_HTTP_A,   HTTP_PORT,    KIND_HTTP);
        serve_socket(SOCK_HTTP_B,   HTTP_PORT,    KIND_HTTP);
        serve_socket(SOCK_MODBUS_A, MODBUS_PORT,  KIND_MODBUS);
        serve_socket(SOCK_MODBUS_B, MODBUS_PORT,  KIND_MODBUS);
        serve_socket(SOCK_SERVICE,  SERVICE_PORT, KIND_SERVICE);
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

// IP adresa jako text (např. "192.168.1.50"); mimo stav NET_UP je to "-".
const char *net_ip_text(void) {
    static char buf[20];
    if (s_state != NET_UP) return "-";
    snprintf(buf, sizeof buf, "%d.%d.%d.%d", s_net.ip[0], s_net.ip[1], s_net.ip[2], s_net.ip[3]);
    return buf;
}

// MAC adresa jako text (např. "02:08:DC:AA:BB:CC").
const char *net_mac_text(void) {
    static char buf[20];
    snprintf(buf, sizeof buf, "%02X:%02X:%02X:%02X:%02X:%02X",
             s_net.mac[0], s_net.mac[1], s_net.mac[2], s_net.mac[3], s_net.mac[4], s_net.mac[5]);
    return buf;
}

// Stav sítě jako číslo (pro diagnostiku): 0 žádný čip, 1 bez kabelu, 2 čeká na IP, 3 běží.
int net_state_code(void) { return (int)s_state; }

// IP adresa jako 4 bajty; mimo stav NET_UP samé nuly.
void net_ip_bytes(uint8_t out[4]) {
    if (s_state == NET_UP) memcpy(out, s_net.ip, 4); else memset(out, 0, 4);
}

// Naplánuje přenastavení sítě asi za 1,5 s.
void net_request_reconfig(void) {
    s_reconfig_at_ms = to_ms_since_boot(get_absolute_time()) + 1500;
    if (s_reconfig_at_ms == 0) s_reconfig_at_ms = 1;
}
