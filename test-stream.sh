#!/bin/bash
export PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin:$HOME/.local/bin
# Stream a tiny WAV via RAOP
exec atvremote --id 00155D629ADD --protocol raop --debug \
    stream_file=/usr/share/sounds/alsa/Front_Center.wav
