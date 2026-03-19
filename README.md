# Skid Steer ESP32P4

## Initial Setup

```bash
source ~/.espressif/tools/activate_idf_v5.5.3.fish
cp -r ~/.espressif/v5.5.3/esp-idf/tools/templates/sample_project skidsteer-esp32p4
idf.py set-target esp32p4
idf.py menuconfig  # Component config → Hardware Settings → Chip revision → Select ESP32-P4 revisions <3.0
idf.py reconfigure
```

VSCode → ESP-IDF: Add VS Code Configuration Folder
VSCode → ESP-IDF: Configure project for ESP-Clang

```json5
  // Add to settings.json
  "C_Cpp.intelliSenseEngine": "disabled",
```

```bash
idf.py build
idf.py flash
idf.py monitor

# OR
idf.py build flash monitor
```

## Adding Wifi


1. Add component dependencies
2. Select the co-processor (the default is correct --> `esp32c6`)
3. Create `sdkconfig.defaults` (required for the chip-specific configuration, even if empty)
4. Create `sdkconfig.defaults.esp32p4` (with contents specified in the esp-hosted GitHub guide)

```bash
idf.py add-dependency "espressif/esp_wifi_remote"
idf.py add-dependency "espressif/esp_hosted"
```

Notes

- [Configuration Files Structure and Relationships](https://docs.espressif.com/projects/esp-idf/en/v5.5.3/esp32p4/api-guides/kconfig/configuration_structure.html#sdkconfig-defaults-and-sdkconfig-defaults-chip)
- [esp-hosted-mcu/docs/esp32_p4_function_ev_board.md](https://github.com/espressif/esp-hosted-mcu/blob/main/docs/esp32_p4_function_ev_board.md#3-building-host-for-the-p4)


## TODO

Switch over to container development? [IDF commands (including dev container)](https://github.com/espressif/vscode-esp-idf-extension/blob/master/README.md#all-available-commands)

Move helper code into components with `idf.py create-component -C components NAME`
