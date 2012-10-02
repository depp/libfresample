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
    def add_default(self, x):
        self._all.add(x)
    def build(self, target, deps, *cmds):
        if target in self._targets:
            return
        self._targets.add(target)
        fp = self._fp
        fp.write(target + ':')
        for dep in deps:
            fp.write(' ' + dep)
        fp.write('\n')
        for cmd in cmds:
            fp.write('\t' + cmd + '\n')
    def write(self, *line):
        for line in line:
            self._fp.write(line + '\n')
    def save(self):
        f = open('Makefile', 'w')
        f.write('all:')
        for t in sorted(self._all):
            f.write(' ' + t)
        f.write('\n')
        f.write(self._fp.getvalue())

make = Makefile()
make.write(
    'FR := ../build/product/fresample',
    'SOX := sox')

def test_sweep(depth, rate1, rate2):
    inpath = 'in_%dk%d.wav' % (rate1 // 1000, depth)
    make.build(
        inpath, ['Makefile'],
        '$(SOX) -b %d -r %d -n $@ synth 8 sine 0+%d vol 0.999' %
        (depth, rate1, rate1//2))
    for q in range(11):
        outpath = 'out_%dk%d_%dk%02dq' % \
                  (rate1 // 1000, depth, rate2/1000, q)
        make.build(
            outpath + '.wav', [inpath, '$(FR)', 'Makefile'],
            '$(FR) -q %d -r %d $< $@' % (q, rate2))
        make.build(
            outpath + '.png', [outpath + '.wav', 'Makefile'],
            'sox $< -n spectrogram -w kaiser -o $@')
        make.add_default(outpath + '.png')

test_sweep(16, 96000, 44100)
test_sweep(16, 96000, 48000)
test_sweep(16, 48000, 44100)
make.write(
    'clean:',
    '\trm -f *.wav *.png')
make.save()
