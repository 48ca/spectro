#!/bin/bash -e

function getBaseName {
    python3 -c "print('.'.join('$1'.split('.')[:-1]))"
}

for f in "$@"
do
    o="$(getBaseName "$f").wav"
    echo "Converting $f -> $o"
    ffmpeg -i "$f" -acodec pcm_u8 -ar 44100 "$o"
done
