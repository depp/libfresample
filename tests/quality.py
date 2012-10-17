#!/usr/bin/env python
import tempfile
import math
import sys
import re
import shutil
import atexit
import os
import subprocess

try:
    import numpy
    import scipy.io.wavfile
except ImportError:
    sys.stderr.write('error: SciPy is required to run this test.\n')
    sys.exit(0)

def to_dB(x):
    return 20 * math.log10(x)

class ProcFailure(Exception):
    def __init__(self, cmd, returncode):
        self.cmd = cmd
        self.returncode = returncode

class TempGroup(object):
    def __init__(self, prefix):
        self._files = set()
        self._prefix = prefix
    def __enter__(self):
        return self
    def __exit__(self, exc_type, exc_value, traceback):
        for f in self._files:
            try:
                os.unlink(f)
            except OSError:
                pass
    def new(self, name):
        name = '%s_%s' % (self._prefix, name)
        self._files.add(name)
        return name

class TempFiles(object):
    def __init__(self):
        self._files = set()
        self._counter = 0
        self._dir = None

    def getdir(self):
        d = self._dir
        if d is None:
            atexit.register(self.destroy)
            d = tempfile.mkdtemp()
            self._dir = d
        return d

    def destroy(self):
        d = self._dir
        if d is not None:
            shutil.rmtree(d)
            self._dir = None

    def unique(self):
        c = self._counter
        self._counter = c + 1
        return 'tmp%6x' % c

    def group(self):
        return TempGroup(self.unique())

temp_files = TempFiles()

def pexc(info):
    if info is not None:
        sys.stderr.write('%s: %s\n' % (info[0].__name__, str(info[1])))

def warning(why, info=None):
    sys.stdout.write('warning: ' + why + '\n')
    pexc(info)

def error(why, info=None):
    sys.stdout.write('error: ' + why + '\n')
    pexc(info)
    sys.exit(1)

class Param(object):
    def __init__(self, name, doc, default, parse, check):
        self.name = name
        self.doc = doc
        self.default = default
        self.parse = parse
        self.check = check
        assert check(default)

    def __get__(self, instance, owner):
        if instance is None:
            return self
        obj = instance
        while obj is not None:
            try:
                value = obj._dict[self.name]
            except KeyError:
                pass
            else:
                obj._unread.discard(self.name)
                return value
            obj = obj._parent
        return self.default

    def __set__(self, instance, value):
        if not self.check(value):
            raise ValueError(
                'value for %s is out of range: %r' % (self.name, value))
        instance._dict[self.name] = value

PARAM = re.compile('^[A-Za-z_][A-Za-z0-9_]*$')
class ParamSet(object):
    @classmethod
    def from_args(klass, args=None):
        if args is None:
            args = sys.argv[1:]
        d = {}
        for arg in args:
            idx = arg.find('=')
            if idx < 0:
                error('invalid argument, must be NAME=VALUE: %r' % arg)
            argname = arg[:idx]
            argval = arg[idx+1:]
            if not PARAM.match(argname):
                error('invalid argument name: %r' % argname)
            argname = argname.upper()
            try:
                parm = getattr(klass, argname)
            except AttributeError:
                parm = None
            if parm is None or not isinstance(parm, Param):
                warning('unknown parameter: %s' % argname)
                continue
            try:
                argval = parm.parse(argval)
            except ValueError:
                error('cannot parse value for %s: %r' % (argname, argval))
            d[argname] = argval
        return klass(**d)

    def __init__(self, **params):
        self._dict = {}
        self._parent = None
        klass = self.__class__
        for k, v in params.items():
            try:
                parm = getattr(klass, k)
            except AttributeError:
                parm = None
            if parm is None or not isinstance(parm, Param):
                raise ValueError('unknown parameter: %s' % k)
            parm.__set__(self, v)
        self._unread = set(self._dict)

    def warn_unread(self):
        if not self._unread:
            return
        warning('unused parameters: %s\n' % ', '.join(sorted(self._unread)))

    def override(self, **kw):
        obj = self.__class__(self, **kw)
        obj._parent = self
        return obj

    def __repr__(self):
        a = []
        obj = self
        while obj is not None:
            a.append(obj._dict)
            obj = obj._parent
        d = {}
        a.reverse()
        for x in a:
            d.update(x)
        return '%s(%s)' % (
            self.__class__.__name__,
            ', '.join('%s=%r' % x for x in sorted(d.items()))
        )

RATE = re.compile(r'^\s*([0-9.]+)\s*(k?)(?:hz)?\s*$', re.I)
def parse_rate(x):
    m = RATE.match(x)
    if not m:
        raise ValueError('invalid frequency: %r' % x)
    num, prefix = m.groups()
    if prefix.lower() == 'k':
        scale = 1000
    else:
        scale = 1
    return round(scale * float(num))
def valid_rate(x):
    return 8000 <= x <= 192000
def parse_format(x):
    return x.lower()
FORMATS = set(['s16', 'f32'])
def valid_format(x):
    return x in FORMATS

class TestParamSet(ParamSet):
    RATE_IN = Param(
        'RATE_IN', 'input sampling rate',
        48000, parse_rate, valid_rate)
    RATE_OUT = Param(
        'RATE_OUT', 'output sample rate',
        44100, parse_rate, valid_rate)
    QUALITY = Param(
        'QUALITY', 'resampler quality setting',
        5, int, lambda x: 0 <= x <= 10)
    FORMAT = Param(
        'FORMAT', 'sample format',
        's16', parse_format, valid_format)

    TEST_BETA = Param(
        'TEST_BETA', 'beta parameter for windowed sine waves',
        14, float, lambda x: 0 <= x <= 50)
    TEST_LENGTH = Param(
        'TEST_LENGTH', 'length of test input files',
        2**16, int, lambda x: 10**2 <= x <= 10**6)

def write_wav(path, rate, data, format):
    """Write a signal as a wave file.

    The 'dtype' corresponds to LibFResample data types, either s16 for
    signed 16-bit integer or f32 for IEEE single precision floating
    point.  The input should be scaled so that 1.0 is 0 dBFS.
    """
    if format == 's16':
        scale = 2**15 * math.sqrt(2)
        data = numpy.asarray(
            numpy.floor(scale * data +
                        numpy.random.random_sample(len(data))),
            dtype='int16')
    elif format == 'f32':
        data = numpy.asarray(data, dtype='float32')
    else:
        raise ValueError('unknown format')
    scipy.io.wavfile.write(path, rate, data)

def read_wav(path):
    rate, data = scipy.io.wavfile.read(path)
    n = data.dtype.name
    if n == 'int16':
        data = numpy.asarray(data, 'float32') * 2**-15
    return data

class Resampler(object):
    def __init__(self, params):
        self.rate1 = params.RATE_IN
        self.rate2 = params.RATE_OUT
        self.quality = params.QUALITY
        self.format = params.FORMAT

    def resample(self, arr):
        with temp_files.group() as temp:
            inpath = temp.new('in.wav')
            outpath = temp.new('out.wav')
            write_wav(inpath, self.rate1, arr, self.format)
            proc = subprocess.Popen([
                '../build/product/fresample',
                '-q', str(self.quality),
                '-r', str(self.rate2),
                inpath, outpath])
            proc.wait()
            if proc.returncode != 0:
                raise ProcFailure(cmd)
            return read_wav(outpath)

class BandwidthSNRTest(object):
    """Test the bandwidth and SNR of a resampler."""
    def __init__(self, params):
        self.resampler = Resampler(params)
        self.beta = params.TEST_BETA
        self.length = params.TEST_LENGTH
        self.nbins = 10
        self.snr_ntest = 40

    def test_atten(self, freq):
        """Test the attenuation of the resampler at the given frequency.

        Produces a windowed sine wave, resamples it, and compares the
        signal power of the output to the signal power of the input.
        Returns the ratio.
        """
        # Note that using float32 for generating sine waves will
        # result in noticeable harmonics as the input to the sine
        # function is subject to rounding, harmonics that can clearly
        # be heard if a 'pure' sine wave is notch filtered out.
        #
        # We also use a -6 dBFS signal so we don't get bothered by
        # clipping.
        w = 2 * math.pi * freq / self.resampler.rate1
        indata = (
            0.5 *
            numpy.sin(w * numpy.arange(self.length, dtype='float64')) *
            numpy.kaiser(self.length, self.beta))
        outdata = self.resampler.resample(indata)
        return numpy.std(outdata) / numpy.std(indata)

    def test_snr(self, freq):
        """Test the SNR using a test tone at the given frequency.

        This works by computing the FFT of the result, zeroing the FFT
        corresponding to the test tone, and comparing the signal power
        with the signal power of the output signal.  Returns the SNR
        ratio (as a ratio, not dB).
        """
        w = 2 * math.pi * freq / self.resampler.rate1
        indata = (
            0.5 * numpy.sin(w * numpy.arange(self.length, dtype='float64')))
        outdata = self.resampler.resample(indata)
        window = numpy.kaiser(len(outdata), self.beta)
        fft = numpy.fft.rfft(outdata * window)

        nbins = self.nbins
        fbin = round(freq * len(outdata) / self.resampler.rate2)
        fft[max(fbin-nbins/2, 0):min(fbin-nbins/2+nbins, len(fft))] = 0
        noise = numpy.std(fft) / math.sqrt(len(outdata))
        signal = numpy.std(outdata)

        return signal / noise

    def test_bw(self, target):
        rate = min(self.resampler.rate1, self.resampler.rate2)
        f1 = rate * 0.1
        f2 = rate * 0.499
        epsilon = 0.1
        f3 = (f1 + f2) * 0.5
        while f2 - f1 > epsilon:
            f3 = (f1 + f2) * 0.5
            atten = self.test_atten(f3)
            if atten < target:
                f2 = f3
            else:
                f1 = f3
        return f3

    def run(self):
        freq = self.test_bw(math.sqrt(0.5))
        minrate = min(self.resampler.rate1, self.resampler.rate2)
        sys.stdout.write(
            'Bandwidth: %.2f Hz (%.2f%% of Nyquist frequency)\n' %
            (freq, 100 * 2 * freq / minrate))

        test_freqs = numpy.linspace(freq*0.05, freq*0.95, self.snr_ntest)
        test_freqs += numpy.random.uniform(
            -freq*0.05, freq*0.05, len(test_freqs))

        test_snr = numpy.vectorize(self.test_snr, otypes=['float32'])
        snrs = test_snr(test_freqs)
        snr_avg = numpy.average(snrs)
        snr_min = numpy.amin(snrs)
        sys.stdout.write(
            'Worst-case SNR: %.2f dB (avg %.2f dB)\n' %
            (to_dB(snr_min), to_dB(snr_avg)))

NOESCAPE = re.compile('^[-A-Za-z0-9_./=:,+=]+$')
def mkparam(x):
    if NOESCAPE.match(x):
        return x
    return repr(x)

def run():
    try:
        param = TestParamSet.from_args()

        tests = [BandwidthSNRTest(param)]
        param.warn_unread()
        for t in tests:
            t.run()
    except ProcFailure:
        exc_type, exc_value, traceback
        sys.stderr.write('error: command failed with code %d\n' %
                         exc_value.returncode)
        sys.stderr.write('  command: %s\n' %
                         ' '.join([mkparam(x) for x in exc_value.cmd]))

run()
