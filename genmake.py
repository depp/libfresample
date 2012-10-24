#!/usr/bin/env python
import os

ALL_SPECIAL = 'altivec mmx sse sse2 sse3 ssse3 sse4_1 sse4_2'.split()

def get_special(name):
    for s in ALL_SPECIAL:
        if name.endswith(s):
            return s
    return None

HEADER = """\
ifeq ($(arch),)
include config.mak
else
include config.$(arch).mak
endif

all_dirs := $(builddir)/shared $(builddir)/static $(builddir)/tool
missing_dirs := $(filter-out $(wildcard $(all_dirs)),$(all_dirs))
$(all_dirs):
\tmkdir -p $@

ifeq ($(UNAME_S),Darwin)
CCSHARED :=
else
CCSHARED := -fpic
endif

"""

FOOTER = """

ifeq ($(UNAME_S),Darwin)

$(builddir)/libfresample.a: $(static_obj)
\tlibtool -static -o $@ $^

$(builddir)/$(so_name): $(static_obj)
\t$(CC) -o $@ -dynamiclib -install_name /Library/Frameworks/fresample.framework/Versions/A/fresample -compatibility_version $(ver_maj).$(ver_min) -current_version $(ver_maj).$(ver_min) $(LDFLAGS) $^ $(LIBS)

else

$(builddir)/libfresample.a: $(static_obj)
\trm -f $@
\tar rc $@ $^
\tranlib $@

$(builddir)/$(so_name): $(shared_obj)
\t$(CC) -o $@ -shared -Wl,-soname,$(so_name1) $(LDFLAGS) $^ $(LIBS)

endif

$(builddir)/fresample: $(tool_obj) $(builddir)/libfresample.a
\t$(CC) -o $@ $(LDFLAGS) $^ $(LIBS)
"""

def get_sources(dirpath):
    sources = {}
    for name in os.listdir(dirpath):
        if name.startswith('.'):
            continue
        base, ext = os.path.splitext(name)
        path = dirpath + '/' + name
        if ext == '.h':
            s = 'include'
        elif ext == '.c':
            s = get_special(base)
            if s is None:
                s = 'base'
        else:
            continue
        try:
            sources[s].append(path)
        except KeyError:
            sources[s] = [path]
    return sources

def build_sources(fp, name, sources, **kw):
    cflags = kw.get('cflags', '')
    prefix = kw.get('prefix', '')

    for s in ['base'] + ALL_SPECIAL:
        try:
            srclist = sources[s]
        except KeyError:
            continue

        if s == 'base':
            s_cflags = ''
        else:
            s_cflags = '$(%s_CFLAGS)' % s.upper()

        rule = '\t$(CC) -c -o $@ $< -I$(srcdir)/include $(CPPFLAGS) ' \
            '%s %s $(CWARN) $(CFLAGS)\n' % (s_cflags, cflags)

        objlist = []
        for src in sorted(srclist):
            base = os.path.splitext(os.path.split(src)[1])[0]
            obj = '$(builddir)/' + prefix + base + '.o'
            src = '$(srcdir)/' + src
            objlist.append(obj)
            fp.write('%s: %s $(missing_dirs)\n' % (obj, src))
            fp.write(rule)

        objlist = ' '.join(objlist)
        if s == 'base':
            fp.write('%s := %s\n' % (name, objlist))
        else:
            fp.write(
                'ifeq ($(%s_ENABLED),1)\n'
                '%s += %s\n'
                'endif\n'
                % (s.upper(), name, objlist))

def run():
    fp = open('arch.mak', 'w')
    fp.write(HEADER)

    lib_src = get_sources('lib')
    tool_src = get_sources('src')
    build_sources(fp, 'shared_obj', lib_src,
                  prefix='shared/', cflags='$(CCSHARED)')
    fp.write('\n')
    build_sources(fp, 'static_obj', lib_src,
                  prefix='static/')
    fp.write('\n')
    build_sources(fp, 'tool_obj', tool_src, prefix='tool/')

    fp.write(FOOTER)
    fp.close()

run()
