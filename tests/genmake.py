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

def test_sweep(depth, rate1, rate2):
    inpath = 'sweep_%dk%d.wav' % (rate1 // 1000, depth)
    make.build(
        inpath, ['Makefile'],
        '$(SOX) -b %d -r %d -n $@ synth 8 sine 0+%d vol 0.999' %
        (depth, rate1, rate1//2))
    sweeps = []
    for q in range(11):
        outpath = 'sweep_%dk%d_%dk%02dq' % \
                  (rate1 // 1000, depth, rate2/1000, q)
        make.build(
            outpath + '.wav', [inpath, '$(FR)', 'Makefile'],
            '$(FR) -q %d -r %d $< $@' % (q, rate2))
        make.build(
            outpath + '.png', [outpath + '.wav', 'Makefile'],
            'sox $< -n spectrogram -w kaiser -o $@')
        sweeps.append(outpath + '.png')
    make.phony('sweep', sweeps);

test_sweep(16, 96000, 44100)
test_sweep(16, 96000, 48000)
test_sweep(16, 48000, 44100)

def test_correct(depth, rate1, rate2):
    inpath = 'correct_%dk%d.wav' % (rate1 // 1000, depth)
    make.build(
        inpath, ['Makefile'],
        '$(SOX) -b %d -r %d -n $@ synth 16 whitenoise' % (depth, rate1))
    outputs = []
    for q in range(11):
        outpath = 'sweep_%dk%d_%dk%02dq' % \
                  (rate1 // 1000, depth, rate2 // 1000, q)
        out1 = outpath + '_1.wav'
        out2 = outpath + '_2.wav'
        make.build(
            out1, [inpath, '$(FR)', 'Makefile'],
            '$(FR) -c none -q %d -r %d $< $@' % (q, rate2))
        make.build(
            out2, [inpath, '$(FR)', 'Makefile'],
            '$(FR) -c all -q %d -r %d $< $@' % (q, rate2))
        outputs.append((out1, out2))
    make.build(
        'test', [x for y in outputs for x in y],
        *(['cmp %s %s' % x for x in outputs] +
          ['@echo === OUTPUT MATCHES ===']))
    make.add_default('test')

test_correct(16, 48000, 44100)            

make.write(
    'clean:',
    '\trm -f *.wav *.png')
make.save()
