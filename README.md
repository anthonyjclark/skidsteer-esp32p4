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

Switch over to container development? [IDF commands (including dev container)](https://github.com/espressif/vscode-esp-idf-extension/blob/master/README.md#all-available-commands)

Move helper code into components with `idf.py create-component -C components NAME`
