# Modbus TCP – kompletní mapa registrů

Verze firmwaru 0.4 (`FW_VERSION_NUM 4`).

## Připojení

| Položka | Hodnota |
|---|---|
| Port | 502 (TCP), dvě souběžná spojení |
| Funkce | 03 Read Holding Registers, 04 Read Input Registers (obě čtou stejnou tabulku), 06 Write Single Register, 16 Write Multiple Registers |
| Počet registrů | 31 (adresy 0 až 30), adresování od nuly |
| Unit ID | výchozí 0 = odpovídá na jakékoli Unit ID; jinak jen na nastavené (1 až 247), na ostatní neodpovídá |
| Výjimky | 01 neplatná funkce, 02 neplatná adresa (mimo tabulku nebo zápis do registru jen pro čtení), 03 neplatná hodnota (jas nad 100) |

**Zabezpečení:** Modbus nemá žádné ověření. Kdo se dostane na port 502, může měnit jas displeje a LED. Heslo nastavení chrání jen web a servisní kanál. Zařízení patří do důvěryhodné sítě.

## Měřené hodnoty (jen čtení)

| Adresa | Hodnota | Typ | Jednotka |
|---|---|---|---|
| 0 | Teplota | int16 | 0,1 °C |
| 1 | Vlhkost | uint16 | 0,1 % |
| 2 | Tlak | uint16 | hPa |
| 3 | Stav senzoru: 0 = OK, 1 = chyba | uint16 | – |
| 4–5 | Teplota | float32 | °C |
| 6–7 | Vlhkost | float32 | % |
| 8–9 | Tlak | float32 | hPa |

Floaty jsou IEEE 754. Pořadí slov je standardní (vyšší slovo první, ABCD); v nastavení (web, volba „Float: prohozená slova“) jde přepnout na CDAB. Při chybě senzoru jsou floaty NaN (0x7FC00000). Hodnoty zahrnují kalibrační posun z nastavení.

## Nastavení (čtení i zápis)

| Adresa | Hodnota | Rozsah | Poznámka |
|---|---|---|---|
| 10 | Jas displeje | 0 až 100 % | 0 = displej vypnutý |
| 11 | Jas LED | 0 až 100 % | 0 = LED zhasnutá |

Změna se projeví ihned. Do flash se uloží až po 5 s klidu bez další změny (nejpozději po 60 s), aby se při častém zápisu neopotřebovávala flash. Hodnoty zapsané přes Modbus se v nastavení na webu objeví také. Registry 12 až 15 jsou rezerva (čtou se jako 0, zápis dává výjimku 02).

## Diagnostika (jen čtení)

| Adresa | Hodnota | Typ | Poznámka |
|---|---|---|---|
| 16–17 | Doba běhu | uint32 | sekundy, vyšší slovo první |
| 18 | Verze firmwaru | uint16 | desetinásobek (0.4 → 4) |
| 19 | Příznaky | uint16 | bitová maska, viz níže |
| 20 | Důvod posledního restartu | uint16 | 0 = zapnutí nebo reset, 1 = hlídací obvod (zařízení se zaseklo) |
| 21 | Teplota čipu RP2350 | int16 | 0,1 °C, orientační |
| 22–23 | Počet chyb senzoru od startu | uint32 | vyšší slovo první |
| 24–25 | Počet Modbus požadavků od startu | uint32 | vyšší slovo první |
| 26–27 | Počet HTTP požadavků od startu | uint32 | vyšší slovo první |
| 28 | Stav sítě | uint16 | 0 = žádný čip, 1 = bez kabelu, 2 = čeká na IP (DHCP), 3 = běží |
| 29–30 | IP adresa | 2× uint16 | např. 192.168.1.50 = 0xC0A8, 0x0132; mimo stav 3 samé nuly |

### Příznaky (registr 19)

| Bit | Význam, když je nastavený |
|---|---|
| 0 | chyba senzoru |
| 1 | Ethernetový čip nenalezen |
| 2 | bez kabelu |
| 3 | čeká na IP adresu |
| 4 | poslední restart způsobil hlídací obvod |
| 5 | nastavení ve flash chybělo nebo bylo poškozené, platí výchozí |

## Příklady

Čtení teploty, vlhkosti a tlaku jako float (funkce 03, adresa 4, 6 registrů):

```
00 01 00 00 00 06 01 03 00 04 00 06
```

Nastavení jasu displeje na 40 % (funkce 06, adresa 10):

```
00 02 00 00 00 06 01 06 00 0A 00 28
```

Nastavení jasu displeje i LED jedním příkazem (funkce 16, adresa 10, 2 registry, 40 % a 10 %):

```
00 03 00 00 00 0B 01 10 00 0A 00 02 04 00 28 00 0A
```

U funkce 16 se nejdřív zkontrolují všechny hodnoty; je-li některá neplatná, nezapíše se žádná.
