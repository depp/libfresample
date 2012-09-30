#!/bin/sh
# The purpose of this test is to compare the output of scalar and
# vector algorithms.  The integer versions should produce output
# that is identical, bit-for-bit, even with dithering.
FR=../build/product/fresample
SOX=sox
set -e

if test ! -f test_stereo.wav ; then
    # Generate some really messy signal with SoX
    $SOX -n -b 16 -r 48k test_stereo.wav \
        synth 4 triangle 10k:100 triangle 11k:110 \
        synth 4 triangle amod 400:1k triangle amod 900:300 \
        synth 4 pluck mix 256 pluck mix 384
fi

if test ! -f test_mono.wav ; then
    $SOX test_stereo.wav test_mono.wav remix 1
fi

# Test 1: Convert to 44.1 kHz
for q in 0 1 2 3 ; do
    for c in stereo mono ; do
        IN=test_$c.wav
        $FR -b 4 -q $q -r 44.1k -c none $IN test_${c}_q${q}_0.wav
        $FR -b 20 -q $q -r 44.1k -c all  $IN test_${c}_q${q}_1.wav
        diff test_${c}_q${q}_0.wav test_${c}_q${q}_1.wav
    done
done

md5sum test_*.wav

echo
echo '==== SUCCESS ===='
echo 'Vector output matches scalar output'
