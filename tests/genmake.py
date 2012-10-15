#!/usr/bin/env python
try:
    from cStringIO import StringIO
except ImportError:
    from io import StringIO

class Makefile(object):
    def __init__(self):
        self._fp = StringIO()
        self._all = set()
        self._targets = set()
        self._phony = set()
    def add_default(self, x):
        self._all.add(x)
    def _write_dep(self, target, deps):
        fp = self._fp
        fp.write(target + ':')
        for dep in deps:
            fp.write(' ' + dep)
        fp.write('\n')
    def build(self, target, deps, *cmds):
        if target in self._targets:
            return
        self._targets.add(target)
        self._write_dep(target, deps)
        for cmd in cmds:
            self._fp.write('\t' + cmd + '\n')
    def write(self, *line):
        for line in line:
            self._fp.write(line + '\n')
    def save(self):
        f = open('Makefile', 'w')
        self._write_dep('all', sorted(self._all))
        self._write_dep('.PHONY', sorted(self._phony))
        f.write('all:\n')
        f.write(self._fp.getvalue())
    def phony(self, target, deps):
        self._phony.add(target)
        self._write_dep(target, deps)

make = Makefile()
make.build('Makefile', ['genmake.py'], 'python genmake.py')
make.write(
    'FR := ../build/product/fresample',
    'SOX := sox')

def test_sweep(depth, nchan, rate1, rate2):
    name = 'sweep_r%ds%dn%d' % (rate1 // 1000, depth, nchan)
    if nchan == 1:
        cmd = 'synth 8 sine 0+%d' % (rate1 // 2)
    else:
        cmd = 'synth 8 sine 0+%d sine %d+0' % (rate1 // 2, rate1 // 2)
    cmd = '$(SOX) -b %d -r %d -n $@ %s vol 0.999' % (depth, rate1, cmd)
    make.build(name + '.wav', ['Makefile'], cmd)
    sweeps = []
    for q in range(11):
        name2 = '%s_r%dq%02d' % (name, rate2 // 1000, q);
        make.build(
            name2 + '.wav', [name + '.wav', '$(FR)', 'Makefile'],
            '$(FR) -q %d -r %d $< $@' % (q, rate2))
        make.build(
            name2 + '.png', [name2 + '.wav', 'Makefile'],
            'sox $< -n spectrogram -w kaiser -o $@')
        sweeps.append(name2 + '.png')
    make.phony('sweep-mono' if nchan == 1 else 'sweep-stereo', sweeps);

test_sweep(16, 1, 96000, 44100)
test_sweep(16, 1, 96000, 48000)
test_sweep(16, 1, 48000, 44100)
test_sweep(16, 2, 96000, 44100)
test_sweep(16, 2, 96000, 48000)
test_sweep(16, 2, 48000, 44100)
make.phony('sweep', ['sweep-mono', 'sweep-stereo'])

def test_correct(depth, nchan, rate1, rate2):
    name = 'correct_r%ds%dn%d' % (rate1 // 1000, depth, nchan)
    inpath = name + '.wav'
    cmd = '$(SOX) -b %d -r %d -n $@ synth 16 whitenoise' % (depth, rate1)
    if nchan == 2:
        cmd += ' whitenoise'
    make.build(inpath, ['Makefile'], cmd)
    outputs = []
    for q in range(6): # q6 and higher is floating point
        outpath = '%s_r%dq%02d' % (name, rate2 // 1000, q)
        out1 = outpath + '_1.wav'
        out2 = outpath + '_2.wav'
        make.build(
            out1, [inpath, '$(FR)', 'Makefile'],
            '$(FR) --cpu-features none -q %d -r %d $< $@' % (q, rate2))
        make.build(
            out2, [inpath, '$(FR)', 'Makefile'],
            '$(FR) --cpu-features all -q %d -r %d $< $@' % (q, rate2))
        outputs.append((out1, out2))
    make.build(
        name, [x for y in outputs for x in y],
        *(['$(FR) --test-bufsize --cpu-features %s -q %d -r %d %s /dev/null'
           % (f, q, rate2, inpath)
           for q in range(11) for f in ['none', 'all']] +
          ['cmp %s %s' % x for x in outputs] +
          ['@echo === OUTPUT MATCHES ===']))
    name2 = 'test-' + { 1: 'mono', 2: 'stereo' }.get(nchan, 'n%d' % nchan)
    make.phony(name2, [name])
    make.phony('test', [name2])
    make.add_default('test')

test_correct(16, 1, 48000, 44100)            
test_correct(16, 2, 48000, 44100)            

make.write(
    'clean:',
    '\trm -f *.wav *.png')
make.save()
