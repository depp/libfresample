#!/usr/bin/env python
import os, re, sys, platform
import cStringIO

def fail(s):
    sys.stderr.write(s + '\n')
    sys.exit(1)

def parsebool(x):
    if isinstance(x, bool):
        return x
    x = x.lower()
    if x in ('1', 'yes', 'on', 'true'):
        return True
    if x in ('0', 'no', 'off', 'false'):
        return False
    fail('invalid boolean: %s' % (x,))

def listsource(path):
    a = []
    for name in os.listdir(path):
        if name.startswith('.'):
            continue
        if os.path.splitext(name)[1] in ('.h', '.c'):
            a.append(os.path.join(path, name))
    return a

def get_special(name):
    """Get the feature used by the given source file path.

    Returns None if the source file path indicates no special
    features.  For example, given "resample_sse2" this will return
    "sse2".  This will only return features that actually exist, so
    "resample_stereo" will return None, since "stereo" is not a
    processor feature.

    """
    i = name.rfind('/')
    if i >= 0:
        name = name[i+1:]
    i = name.find('.')
    if i >= 0:
        name = name[:i]
    i = name.rfind('_')
    if i >= 0:
        special = name[i+1:]
        if special in ALL_SPECIAL:
            return special
    return None

VARSUBST = re.compile(r'@(\w+)@')

class Builder(object):
    def __init__(self, base):
        self.vars = {}
        self.base = base
        self.arch = current_arch()

    def __setitem__(self, var, val):
        self.vars[var] = val

    def __getitem__(self, var):
        try:
            return self.vars[var]
        except KeyError:
            if self.base is not None:
                return self.base[var]
            raise

    def __contains__(self, var):
        if var in self.vars:
            return True
        if self.base is not None:
            return var in self.base
        return False

    def defmakevar(self, var, val):
        if self.base is None:
            raise Exception('invalid base')
        self.base.defmakevar(var, val)

    def build(self, *arg, **kw):
        if self.base is None:
            raise Exception('invalid base')
        if 'base' not in kw:
            kw = dict(kw)
            kw['base'] = self
        self.base.build(*arg, **kw)

    def expand(self, val):
        def repl(match):
            var = match.group(1)
            return self[var]
        return VARSUBST.sub(repl, val)

    def compile(self, sources):
        allowed_special = set(SPECIAL.get(self.arch, []))
        objs = []
        objdir = '@builddir@/obj'
        for src in sources:
            dirpath, fname = os.path.split(src)
            base, ext = os.path.splitext(fname)
            special = get_special(base)
            if special and special not in allowed_special:
                continue
            if ext == '.c':
                objpath = os.path.join(objdir, base + '.o')
                cflags = '@cflags@'
                if special:
                    cflags = '$(%s_CFLAGS) %s' % (special.upper(), cflags)
                self.build(
                    objpath, [src],
                    '$(CC) $< -c -o $@ ' + cflags)
            elif ext in ('.o', '.a'):
                objpath = src
            else:
                continue
            objs.append(objpath)
        return objs

    def staticlib(self, target, srcs):
        objs = self.compile(srcs)
        libpath = os.path.join('@builddir@/product', 'lib' + target + '.a')
        self.build(
            libpath, objs,
            'rm -f $@',
            'ar rc $@ $(filter %.o,$^)',
            'ranlib $@')
        return [libpath]

    def executable(self, target, srcs):
        objs = self.compile(srcs)
        exepath = os.path.join('@builddir@/product', target)
        self.build(exepath, objs,
            '$(CC) -o $@ @ldflags@ $(filter %.o,$^) $(filter %.a,$^)')
        return [exepath]

class RootBuilder(Builder):
    def __init__(self):
        super(RootBuilder, self).__init__(None)
        self._fp = cStringIO.StringIO()
        self._dirs = set()
        self._phony = set(['all'])
        self._defaults = set()

    def write(self):
        fp = open('Makefile', 'w')
        fp.write('all: %s\n' % ' '.join(sorted(self._defaults)))
        fp.write('.PHONY: %s\n' % ' '.join(sorted(self._phony)))
        fp.write('all_dirs := %s\n' % ' '.join(sorted(self._dirs)))
        fp.write('missing_dirs := $(filter-out $(wildcard '
                 '$(all_dirs)),$(all_dirs))\n')
        fp.write('$(all_dirs):\n')
        fp.write('\tmkdir -p $@\n')
        fp.write(self._fp.getvalue())
        fp.close()

    def defmakevar(self, var, val):
        self._fp.write('%s := %s\n' % (var, val))

    def getfp():
        return self._fp

    def build(self, target, deps, *cmds, **kw):
        base = kw.get('base', self)
        phony = kw.get('phony', False)
        extra = set(kw) - set(['base', 'phony'])
        if extra:
            raise Exception('extra keyword arg')
        if base is None:
            base = self
        target = base.expand(target)
        deps = [base.expand(dep) for dep in deps]
        dirpath = os.path.dirname(target)
        if dirpath:
            deps = list(deps) + ['$(missing_dirs)']
            self._dirs.add(dirpath)
        f = self._fp
        f.write(target + ':')
        if deps:
            f.write(' ' + ' '.join(deps))
        f.write('\n')
        for cmd in cmds:
            f.write('\t' + base.expand(cmd) + '\n')
        if phony:
            self._phony.add(target)

    def default_targets(self, targets):
        self._defaults.update([self.expand(target) for target in targets])

SPECIAL_PPC = ['altivec']
SPECIAL_X86 = 'mmx sse sse2 sse3 ssse3 sse41 sse42'.split()
SPECIAL = {
    'ppc': SPECIAL_PPC,
    'ppc64': SPECIAL_PPC,
    'i386': SPECIAL_X86,
    'x86_64': SPECIAL_X86,
}
ALL_SPECIAL = set([y for x in SPECIAL.values() for y in x])

class ArchBuilder(Builder):
    def __init__(self, base, arch):
        super(ArchBuilder, self).__init__(base)
        self.arch = arch
        self['builddir'] = os.path.join(base['builddir'], arch)
        self['cflags'] = '-arch %s %s' % (arch, base['cflags'])
        self['ldflags'] = '-arch %s %s' % (arch, base['ldflags'])
        self['arch'] = arch

    def staticlib(self, target, srcs):
        objs = self.compile(srcs)
        libpath = os.path.join('@builddir@/product', 'lib' + target + '.a')
        self.build(libpath, objs,
            'libtool -arch_only @arch@ -static -o $@ $(filter %.o,$^)')
        return [libpath]

class MultiArchBuilder(Builder):
    def __init__(self, base, archs):
        super(MultiArchBuilder, self).__init__(base)
        self._archs = archs
        self._builders = [ArchBuilder(base, arch) for arch in archs]

    def compile(self, *arg, **kw):
        raise Exception('do not directly compile for multi-arch')

    def staticlib(self, target, srcs):
        libs = []
        for b in self._builders:
            libs.extend([b.expand(lib) for lib in b.staticlib(target, srcs)])
        libpath = os.path.join('@builddir@/product', 'lib' + target + '.a')
        self.build(libpath, libs,
            'libtool -static $(filter %.a,$^) -o $@')
        return [libpath]

    def executable(self, target, srcs):
        exes = []
        for b in self._builders:
            exes.extend([b.expand(exe) for exe in b.executable(target, srcs)])
        exepath = os.path.join('@builddir@/product', target)
        self.build(exepath, exes,
            'lipo -create %s -output $@' % (' '.join(exes)))
        return [exepath]

def current_arch():
    p = platform.processor() or platform.machine()
    if p == 'powerpc':
        return 'ppc'
    if re.match(r'^i\d86$', p):
        m = platform.machine()
        if m == 'x86_64':
            return m
        return 'i386'
    if p == 'x86_64':
        return p
    raise Exception('unknown arch: %s' % (p,))

def run():
    args = getargs()
    config = args.get('CONFIG', 'release').lower()
    if config not in ('debug', 'release'):
        fail('unknown configuration: %s' % config)
    release = config == 'release'
    try:
        cflags = args['CFLAGS']
    except KeyError:
        cflags = '-O2 -g' if release else '-O0 -g'
    multiarch = platform.system() == 'Darwin'
    if multiarch:
        try:
            archs = args['ARCHS']
        except KeyError:
            archs = 'i386 ppc x86_64 ppc64' if release else current_arch()
        if not archs:
            fail('no architectures specified\n')
        archs = archs.split()
    else:
        if args.get('ARCHS', None):
            fail('multiarch only supported on Darwin/OS X\n')

    p = RootBuilder()
    p['builddir'] = 'build'
    p['cflags'] = '$(PROJ_CFLAGS) $(CFLAGS)'
    p['ldflags'] = '$(PROJ_LDFLAGS) $(LDFLAGS)'
    p.defmakevar('CFLAGS', cflags)
    if platform.system() == 'Linux':
        p.defmakevar('PROJ_LDFLAGS', '-lm')
    fl = ('-Iinclude -Wall -Wextra -Wpointer-arith '
          '-Wwrite-strings -Wmissing-prototypes -Wstrict-prototypes')
    if parsebool(args.get('WERROR', False)):
        fl += ' -Werror'
    p.defmakevar('PROJ_CFLAGS', fl)

    # For GCC, -mpim-altivec compiles quickly but -maltivec
    # takes forever to compile.
    p.defmakevar('ALTIVEC_CFLAGS', '-mpim-altivec')

    if multiarch:
        a = MultiArchBuilder(p, archs)
    else:
        a = p

    libsrc = listsource('lib')
    incsrc = listsource('include')
    srcsrc = listsource('src')
    lib = a.staticlib('fresample', libsrc)
    exe = a.executable('fresample', lib + srcsrc)
    p.default_targets(lib + exe)
    p.build('clean', [], 'rm -rf build', phony=True)
    p.write()

def usage():
    sys.stderr.write('usage: genmake.py [VAR=VALUE...]\n')
    sys.exit(1)

VARS = set('CONFIG CFLAGS ARCHS WERROR'.split())
USAGE = """\
usage: genmake.py [VAR=VALUE]...
variables: %s
""" % (' '.join(sorted(VARS)))

def getargs():
    d = {}
    for arg in sys.argv[1:]:
        eq = arg.find('=')
        if eq < 0:
            usage()
        var = arg[:eq].upper()
        val = arg[eq+1:]
        if var not in VARS:
            sys.stderr.write('unknown variable: %s\n' % var)
            sys.exit(1)
        d[var] = val
    return d

run()
