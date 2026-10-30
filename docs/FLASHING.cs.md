# Nahrání ARose WaveShare

[Zpět](../README.cs.md) · [English](FLASHING.md)

Firmware je určen pro **Waveshare ESP32-S3-Touch-LCD-2.1** a **ESP32-S3-Touch-LCD-2.8C**, s 16 MB flash a 8 MB OPI PSRAM. Oba modely rozpozná automaticky. Aktuální XIP sestavení je fyzicky ověřeno na 2.1; závěrečné ověření XIP a nastavení na 2.8C zatím chybí.

1. Z [release v0.3.0](https://github.com/CooLajz/ARose-WaveShare/releases/tag/v0.3.0) stáhněte **`ARose-WaveShare-0.3.0.factory.bin`**.
2. Připojte displej datovým USB kabelem a zavřete aplikace, které používají jeho sériový port.
3. V desktopovém Chrome nebo Edge otevřete [webový flasher Espressif](https://espressif.github.io/esptool-js/) a připojte se k USB portu zařízení.
4. Vyberte stažený soubor a adresu **`0x0`**. Pokud flasher požaduje parametry flash, zvolte **DIO, 80 MHz, 16 MB**. Pro přenos lze začít rychlostí 460800 baud.
5. Nechte **Erase all** vypnuté, pokud nechcete smazat veškerá dosavadní data. Nahrajte soubor a zařízení restartujte nebo znovu připojte napájení.

Pokud automatický vstup do bootloaderu nefunguje, podržte **BOOT**, krátce stiskněte **RESET**, uvolněte **BOOT** a znovu se připojte. Při nestabilním přenosu zkuste rychlost 115200 baud nebo jiný datový kabel.

Animace se spustí automaticky. Podržením prstu přibližně 0,7 sekundy otevřete nastavení. Čistá konfigurace má jas **100 %**, svítivost **50 %**, glow **80 %** a ukládání pozice po **2 minutách**. Existující kompatibilní nastavení má přednost před výchozími hodnotami.

Factory soubor obsahuje bootloader, partition table, výběr OTA slotu a aplikaci. **Patří na adresu `0x0`, nikoli `0x10000`, a není určen pro OTA aktualizátor hodin.** Není doplněn na celých 16 MB. Mezery uvnitř souboru včetně běžného NVS oddílu od `0x9000` přepíše hodnotami `0xff`; samostatný oddíl `clockcfg` na `0xfe0000` zůstává mimo jeho rozsah. Volba úplného smazání flash by odstranila i tento oddíl.

Release obsahuje také archiv jednotlivých částí s adresami a kontrolní součty SHA-256. Podrobnosti a příkaz pro esptool naleznete v [anglickém návodu](FLASHING.md). Složený balíček je ověřen proti kompilovaným částem; samotné nahrání přes prohlížeč v této relaci ověřeno nebylo. Fyzické testy používaly komponenty nahrané přes esptool.
