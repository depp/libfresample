#!/usr/bin/env python
import subprocess
import sys, time

def error(why):
    sys.stderr.write('error: ' + why + '\n')
    sys.exit(1)

INPUTS = set()

def gen_input(depth, nchan, rate, length):
    path = 'bench_r%dn%ds%d_%d.wav' % (depth, nchan, rate // 1000, length)
    k = depth, nchan, rate, length
    if k not in INPUTS:
        INPUTS.add(k)
        cmd = ['sox', '-b', str(depth), '-r', str(rate), '-n', path,
               'synth', str(length), 'pinknoise']
        if nchan == 2:
            cmd.append('pinknoise')
        subprocess.check_call(cmd)
    return path

def benchmark(depth, nchan, rate1, rate2, length, q, itercount):
    infile = gen_input(depth, nchan, rate1, length)
    speeds = []
    for c in ('none', 'all'):
        sys.stdout.write(
            'depth=%d nchan=%d in=%d out=%d Q=%-2d c=%-4s\n' %
            (depth, nchan, rate1, rate2, q, c))
        proc = subprocess.Popen(
            ['../build/product/fresample',
             '-q', str(q), '-r', str(rate2), '-c', c, '-b', str(itercount),
             infile, 'bench_out.wav'],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE)
        out, err = proc.communicate()
        if proc.returncode:
            sys.stderr.write('process failed\n')
            sys.exit(1)
        speeds.append(out.strip())
    log.write('%d\t%d\t%s\t%s\n' % (nchan, q, speeds[0], speeds[1]))
    log.flush()

def parse_range(x, minv, maxv):
    i = x.find('-')
    if i < 0:
        v0 = int(x)
        v1 = v0
    else:
        v0 = int(x[:i])
        v1 = int(x[i+1:])
    if v1 < v0:
        raise ValueError('invalid range')
    if v0 < minv or v1 > maxv:
        raise ValueError('range too large')
    return v0, v1

def run():
    global log
    log = open('benchmark.txt', 'w')
    nchan_range = (1, 2)
    q_range = (0, 10)
    itercount = 20
    for arg in sys.argv[1:]:
        i = arg.find('=')
        if i < 0:
            error('invalid argument syntax: %s' % repr(arg))
        vn = arg[:i].lower()
        vv = arg[i+1:]
        try:
            if vn == 'q':
                q_range = parse_range(vv, 0, 10)
            elif vn == 'nchan':
                nchan_range = parse_range(vv, 1, 2)
            elif vn == 'iter':
                itercount = int(vv)
                if itercount < 1:
                    error('invalid iteration count')
            else:
                error('unknown variable: %s' % repr(vn))
        except ValueError:
            error('invalid value: %s' % repr(vv))
    sys.stdout.write('NCHAN: %d-%d\n' % nchan_range)
    sys.stdout.write('Q: %d-%d\n' % q_range)
    sys.stdout.write('ITER: %d\n' % itercount)
    t0 = time.time()
    try:
        for nchan in range(nchan_range[0], nchan_range[1]+1):
            for q in range(q_range[0], q_range[1]+1):
                benchmark(16, nchan, 48000, 44100, 120, q, itercount)
    finally:
        t1 = time.time()
        sys.stdout.write('Time: %f\n' % (t1 - t0))
        log.close()

run()
