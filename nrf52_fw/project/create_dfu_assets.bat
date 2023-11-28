Rem @echo off
set app_path="app\Output\Debug\Exe\ws2812b_nrf52.hex"
set bootloader_path="bootloader\Output\Release\Exe\secure_bootloader_ble_s140_pca10056.hex"
set softdevice_path="..\nRF5_SDK_17.1.0_ddde560\components\softdevice\s140\hex\s140_nrf52_7.2.0_softdevice.hex"

..\tools\nrfutil settings generate --family NRF52840 --application %app_path% --application-version 0x01 --bootloader-version 0x01 --bl-settings-version 0x01 settings_page.hex

..\tools\nrfjprog\mergehex -m %app_path% settings_page.hex -o app_plus_sp.hex
..\tools\nrfjprog\mergehex -m app_plus_sp.hex %softdevice_path% %bootloader_path% -o ws2812b_nrf52_merged.hex

..\tools\nrfutil  pkg generate --hw-version 52 --sd-req 0x0100 --application-version 0x01 --application %app_path% --key-file bootloader\src\priv.pem ws2812b_nrf52_dfu.zip

del app_plus_sp.hex
del settings_page.hex

Rem ..\tools\nrfjprog\nrfjprog -f NRF52 -e
Rem ..\tools\nrfjprog\nrfjprog -f NRF52 --program ws2812b_nrf52_merged.hex -r
