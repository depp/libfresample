#!/usr/bin/env python
import os, re

def listsource(path):
    a = []
    for name in os.listdir(path):
        if name.startswith('.'):
            continue
        if os.path.splitext(name)[1] in ('.h', '.c'):
            a.append(os.path.join(path, name))
    return a

VARSUBST = re.compile(r'@(\w+)@')

class Builder(object):
    def __init__(self, base):
        self.vars = {}
        self.base = base

    def __setitem__(self, var, val):
        self.vars[var] = val

    def __getitem__(self, var):
        try:
            return self.vars[var]
        except KeyError:
            if self.base is not None:
                return self.base[var]
            raise

    def defmakevar(self, var, val):
        if self.base is None:
            raise Exception('invalid base')
        self.base.defmakevar(var, val)

    def build(self, *arg, base=None, **kw):
        if self.base is None:
            raise Exception('invalid base')
        if base is None:
            base = self
        self.base.build(*arg, base=base, **kw)

    def expand(self, val):
        def repl(match):
            var = match.group(1)
            return self[var]
        return VARSUBST.sub(repl, val)

    def compile(self, sources):
        objs = []
        objdir = '@builddir@/obj'
        for src in sources:
            dirpath, fname = os.path.split(src)
            base, ext = os.path.splitext(fname)
            if ext == '.c':
                objpath = os.path.join(objdir, base + '.o')
                self.build(objpath, [src],
                    '$(CC) $< -c -o $@ @cflags@')
            elif ext in ('.o', '.a'):
                objpath = src
            else:
                continue
            objs.append(objpath)
        return objs

    def default_targets(self, targets):
        self.build('all', targets)

class RootBuilder(Builder):
    def __init__(self):
        super(RootBuilder, self).__init__(None)
        self._fp = open('Makefile', 'w')
        self._dirs = set()
        self._phony = set(['all'])
        self._fp.write('all:\n')

    def close(self):
        f = self._fp
        if not f:
            return
        f.write(' '.join(sorted(self._dirs)))
        f.write(':\n\tmkdir -p $@\n')
        f.write('.PHONY: ' + ' '.join(sorted(self._phony)))
        f.close()
        self._fp = None

    def defmakevar(self, var, val):
        self._fp.write('%s := %s\n' % (var, val))

    def getfp():
        return self._fp

    def build(self, target, deps, *cmds, base=None, phony=False):
        if base is None:
            base = self
        target = base.expand(target)
        deps = [base.expand(dep) for dep in deps]
        dirpath = os.path.dirname(target)
        if dirpath:
            deps = list(deps) + [dirpath]
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

SPECIAL = {
    'ppc': ['altivec'],
    'ppc64': ['altivec'],
    'i386': ['mmx', 'sse'],
    'x86_64': ['mmx', 'sse']
}
ALL_SPECIAL = set([y for x in SPECIAL.values() for y in x])

class ArchBuilder(Builder):
    def __init__(self, base, arch):
        super(ArchBuilder, self).__init__(base)
        self._arch = arch
        self['builddir'] = os.path.join(base['builddir'], arch)
        self['cflags'] = '-arch %s %s' % (arch, base['cflags'])
        self['ldflags'] = '-arch %s %s' % (arch, base['ldflags'])
        self['arch'] = arch

    def compile(self, srcs):
        exclude = ALL_SPECIAL - set(SPECIAL[self._arch])
        nsrcs = []
        for src in srcs:
            for e in exclude:
                if e in src:
                    break
            else:
                nsrcs.append(src)
        return super(ArchBuilder, self).compile(nsrcs)

    def staticlib(self, target, srcs):
        objs = self.compile(srcs)
        libpath = os.path.join('@builddir@/product', 'lib' + target + '.a')
        self.build(libpath, objs,
            'libtool -arch_only @arch@ -static -o $@ $(filter %.o,$^)')
        return [libpath]

    def executable(self, target, srcs):
        objs = self.compile(srcs)
        exepath = os.path.join('@builddir@/product', target)
        self.build(exepath, objs,
            '$(CC) -o $@ @ldflags@ $(filter %.o,$^) $(filter %.a,$^)')
        return [exepath]

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

def run():
    libsrc = listsource('lib')
    incsrc = listsource('include')
    srcsrc = listsource('src')
    p = RootBuilder()
    p['builddir'] = 'build'
    p['cflags'] = '$(PROJ_CFLAGS) $(CFLAGS)'
    p['ldflags'] = '$(PROJ_LDFLAGS) $(LDFLAGS)'
    try:
        p.defmakevar('CFLAGS', '-O2 -g')
        p.defmakevar('PROJ_CFLAGS', '-Iinclude')
        archs = ['i386', 'ppc', 'x86_64', 'ppc64']
        a = MultiArchBuilder(p, archs)
        lib = a.staticlib('fresample', libsrc)
        exe = a.executable('fresample', lib + srcsrc)
        p.default_targets(lib + exe)
        p.build('clean', [], 'rm -rf build', phony=True)
    except:
        p.close()
        try:
            os.unlink('Makefile')
        except:
            pass
        raise
    finally:
        p.close()

run()
