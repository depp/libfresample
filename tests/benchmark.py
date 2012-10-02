#!/usr/bin/env python
import subprocess
import sys

def benchmark(depth, nchan, rate1, rate2, length):
    cmd = ['sox', '-b', str(depth), '-r', str(rate1),
           '-n', 'bench_in.wav',
           'synth', str(length), 'pinknoise']
    if nchan == 2:
        cmd.append('pinknoise')
    subprocess.check_call(cmd)
    for q in range(11):
        speeds = []
        for c in ('none', 'all'):
            sys.stdout.write(
                '\rdepth=%d nchan=%d in=%d out=%d Q=%-2d c=%-4s' %
                (depth, nchan, rate1, rate2, q, c))
            sys.stdout.flush()
            proc = subprocess.Popen(
                ['../build/product/fresample',
                 '-q', str(q), '-r', str(rate2), '-c', c, '-b', '20',
                 'bench_in.wav', 'bench_out.wav'],
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE)
            out, err = proc.communicate()
            if proc.returncode:
                sys.stderr.write('process failed\n')
                sys.exit(1)
            speeds.append(out.strip())
        log.write('%d\t%d\t%s\t%s\n' % (nchan, q, speeds[0], speeds[1]))
    sys.stdout.write('\n')

log = open('benchmark.txt', 'w')
try:
    benchmark(16, 1, 48000, 44100, 120)
    benchmark(16, 2, 48000, 44100, 120)
finally:
    log.close()

