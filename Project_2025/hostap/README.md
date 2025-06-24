# Custom hostapd Build from Source

This repository contains the source code of `hostapd`, cloned from the official upstream, compiled manually on Ubuntu.

## 🛠️ Features

- Version: hostapd v2.12-devel
- Built and installed from source
- Configured to run as a daemon

## 📦 Build Instructions

```bash
cd hostapd
cp defconfig .config
make
