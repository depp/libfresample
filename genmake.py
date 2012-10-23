#!/usr/bin/env python
import os

ALL_SPECIAL = 'altivec mmx sse sse2 sse3 ssse3 sse4_1 sse4_2'.split()

def get_special(name):
    for s in ALL_SPECIAL:
        if name.endswith(s):
            return s
    return None

HEADER = """\
include config.mak

all_dirs := $(builddir)
missing_dirs := $(filter-out $(wildcard $(all_dirs)),$(all_dirs))
$(all_dirs):
\tmkdir -p $@

"""

FOOTER = """
$(builddir)/libfresample.a: $(lib_objs)
\trm -f $@
\tar rc $@ $^
\tranlib $@

# libtool -arch_only i386 -static -o $@ $^

$(builddir)/libfresample.so: $(lib_objs)
\t$(CC) -o $@ -shared -Wl,-soname,libfresample.so.1 $(LDFLAGS) $^ $(LIBS)

$(builddir)/fresample: $(lib_objs) $(src_objs)
\t$(CC) -o $@ $(LDFLAGS) $^ $(LIBS)
"""

def run():
    fp = open('arch.mak', 'w')
    fp.write(HEADER)
    includes = []
    for dirname in ('lib', 'src'):
        sources = {}
        for name in os.listdir(dirname):
            if name.startswith('.'):
                continue
            base, ext = os.path.splitext(name)
            path = dirname + '/' + name
            if ext == '.h':
                includes.append(path)
            elif ext == '.c':
                s = get_special(base)
                if s is None:
                    s = 'base'
                try:
                    sources[s].append(path)
                except KeyError:
                    sources[s] = [path]

        for s in ['base'] + ALL_SPECIAL:
            try:
                srclist = sources[s]
            except KeyError:
                continue

            cflags = '$(CFLAGS)'
            if s != 'base':
                cflags = '$(%s_CFLAGS) %s' % (s.upper(), cflags)
            rule = '\t$(CC) -c -o $@ $< ' \
                   '-I$(srcdir)/include $(CPPFLAGS) $(CWARN) %s\n' % (cflags)

            srclist.sort()
            objlist = []
            for src in srclist:
                base = os.path.splitext(os.path.split(src)[1])[0]
                obj = '$(builddir)/' + base + '.o'
                src = '$(srcdir)/' + src
                objlist.append(obj)
                fp.write('%s: %s $(missing_dirs)\n' % (obj, src))
                fp.write(rule)

            objlist = ' '.join(objlist)
            if s == 'base':
                fp.write('%s_objs := %s\n' % (dirname, objlist))
            else:
                fp.write(
                    'ifeq ($(%s_ENABLED),1)\n'
                    '%s_objs += %s\n'
                    'endif\n'
                    % (s.upper(), dirname, objlist))

    fp.write(FOOTER)
    fp.close()

run()
