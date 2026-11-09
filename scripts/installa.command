#!/bin/bash
# Installa Collateral Effects CARTILAGINE nelle cartelle di sistema dei plugin
# (/Library/Audio/Plug-Ins) e rimuove la quarantena. Chiede la password di amministratore.
cd "$(dirname "$0")"
set -e
VST3="/Library/Audio/Plug-Ins/VST3"
AU="/Library/Audio/Plug-Ins/Components"
echo "Installazione di CARTILAGINE (serve la password di amministratore)..."
sudo mkdir -p "$VST3" "$AU"
sudo rm -rf "$VST3/CARTILAGINE.vst3" "$AU/CARTILAGINE.component" "/Applications/CARTILAGINE.app"
sudo cp -R "CARTILAGINE.vst3" "$VST3/"
sudo cp -R "CARTILAGINE.component" "$AU/"
sudo cp -R "CARTILAGINE.app" "/Applications/"
sudo xattr -dr com.apple.quarantine "$VST3/CARTILAGINE.vst3" "$AU/CARTILAGINE.component" "/Applications/CARTILAGINE.app" 2>/dev/null || true
killall -9 AudioComponentRegistrar 2>/dev/null || true
echo ""
echo "Collateral Effects CARTILAGINE installato:"
echo "  VST3: $VST3/CARTILAGINE.vst3"
echo "  AU:   $AU/CARTILAGINE.component"
echo "  App:  /Applications/CARTILAGINE.app"
echo "Riavvia la DAW per vederlo."
