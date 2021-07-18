project('squashfuse', ['c'],
  version:       '0.1.104',
  meson_version: '>=0.58.0',
  default_options: [
    'warning_level=3',
  ],
)

cc = meson.get_compiler('c')
pkg = import('pkgconfig')

#
#   Generate the config.h
#   =====================
#

conf_data = configuration_data()
system_headers = [
  'asm/byteorder.h',
  'attr/xattr.h',
  'dlfcn.h',
  'endian.h',
  'inttypes.h',
  'machine/endian.h',
  'sys/mkdev.h',
  'sys/stat.h',
  'sys/sysmacros.h',
  'sys/types.h',
]


conf_data.set_quoted('VERSION',           meson.project_version())
conf_data.set_quoted('PACKAGE',           meson.project_name())
conf_data.set_quoted('PACKAGE_NAME',      meson.project_name())
conf_data.set_quoted('PACKAGE_VERSION',   meson.project_version())
conf_data.set_quoted('PACKAGE_BUGREPORT', 'dave@vasilevsky.ca')
conf_data.set_quoted('PACKAGE_STRING',    meson.project_name() + ' ' + meson.project_version())
conf_data.set_quoted('PACKAGE_TARNAME',   meson.project_name())
conf_data.set_quoted('PACKAGE_URL',       '')

conf_data.set('_POSIX_C_SOURCE', '200112L')

foreach h : system_headers
  define = 'HAVE_' + h.to_upper().underscorify()
  if cc.check_header(h)
    conf_data.set(define, 1)
  endif
endforeach

if cc.has_type('__le16', prefix: '#include <linux/types.h>')
  conf_data.set('HAVE_LINUX_TYPES_LE16', 1)
endif


# Determine what _*_SORUCE flags must be defined to enable the features
NONSTD_CHECKS = {
  'makedev':  { 'body': 'makedev(0,0)',        'includes': ['sys/types.h', 'sys/mkdev.h', 'sys/sysmacros.h']           },
  'S_IFSOCK': { 'body': 'mode_t m = S_IFSOCK', 'includes': ['sys/stat.h']                                              },
  'ENOATTR':  { 'body': 'int e = ENOATTR',     'includes': ['sys/types.h', 'attr/xattr.h', 'errno.h'],  'fatal': false },
  'pread':    { 'body': '(void)pread',         'includes': ['unistd.h']                                                },
  'daemon':   { 'body': '(void)daemon',        'includes': ['unistd.h']                                                },
  'symlink':  { 'body': '(void)symlink',       'includes': ['unistd.h']                                                },
}

NONSTD_DEFINES = [
  ['', '', ''],
  ['#define',  '_DARWIN_C_SOURCE', 1   ],
  ['#define',  '_NETBSD_SOURCE',   1   ],
  ['#define',  '_XOPEN_SOURCE',    500 ],
  ['#define',  '_BSD_SOURCE',      1   ],
  ['#define',  '_GNU_SOURCE',      1   ],
  ['#undef',   '_POSIX_C_SOURCE',  ''  ],
  ['__FAIL__', '', ''], # List end marker
]

foreach name, data : NONSTD_CHECKS
  body = data['body']
  fatal = data.get('fatal', true)

  # Check the required includes
  include_str = ''
  foreach i : data['includes']
    if cc.check_header(i)
      include_str += f'#include <@i@>\n'
    endif
  endforeach

  foreach def : NONSTD_DEFINES
    # Build the define prefix
    macro  = def[0]
    define = def[1]
    value  = def[2]
    prefix = f'@macro@ @define@ @value@'.strip()

    # Abort on last element since non of the others have worked
    if prefix == '__FAIL__' and fatal
      error(f'Failed to determine how to use @name@')
    elif prefix == '__FAIL__'
      break
    endif

    # Try to compile and link the sample source
    src_str = '''
#define _POSIX_C_SOURCE 1
@0@
@1@

int main(void) {
  @2@;
  return 0;
}
    '''.format(prefix, include_str, body)

    links = cc.links(src_str)
    if links and define == ''
      message(f'Using @name@ by changing nothing')
      break
    elif links
      message(f'Using @name@ by changing the @define@ define')
      name = f'NONSTD_@name@_DEF'.to_upper()
      value = f'CHANGE@define@'
      conf_data.set(name, value)
      break
    endif
  endforeach
endforeach


# SQ_CHECK_DECL_MAKEDEV_QNX
qnx_makedev_src = '''
#include <sys/types.h>
#include <sys/sysmacros.h>
#include <sys/netmgr.h>
int main(void) { makedev(ND_LOCAL_NODE,0,0); return 0; }
'''

if cc.links(qnx_makedev_src)
  conf_data.set('QNX_MAKEDEV', 1)
endif


#
#   Compression libraries
#   =====================
#

COMP_DEPS = [
  {'id': 'zlib',    'name': 'zlib', 'fallback': 'zlib',    'define': 'HAVE_ZLIB_H'      },
  {'id': 'liblzma', 'name': 'xz',   'fallback': 'liblzma', 'define': 'HAVE_LZMA_H'      },
  {'id': 'lzo2',    'name': 'lzo',                         'define': 'HAVE_LZO_LZO1X_H' },
  {'id': 'liblz4',  'name': 'lz4',  'fallback': 'lz4',     'define': 'HAVE_LZ4_H'       },
  {'id': 'libzstd', 'name': 'zstd', 'fallback': 'zstd',    'define': 'HAVE_ZSTD_H'      },
]

compression_deps = []

foreach data : COMP_DEPS
  if 'fallback' in data
    # Use a WrapDB fallback if availiable
    dep = dependency(data['id'], required: get_option('use_' + data['name']), fallback: data['fallback'])
  else
    dep = dependency(data['id'], required: get_option('use_' + data['name']))
  endif
  if dep.found()
    conf_data.set(data['define'], 1)
    compression_deps += [dep]
  endif
  summary(data['name'], dep.found(), section: 'Compression')
endforeach


#
#   Handle fuse 2 and 3
#   ===================
#

fuse_dep = disabler()
fuse_use_version = 0

if get_option('fuse_version') == '2'
  fuse_dep = dependency('fuse',  version: '>=2.6', fallback: 'fuse')
  fuse_use_version = 26
elif get_option('fuse_version') == '3'
  fuse_dep = dependency('fuse3', version: '>=3.2', fallback: 'fuse3')
  fuse_use_version = 32
endif

if fuse_dep.type_name() == 'internal'
  conf_data.set('FUSE_USE_VERSION', fuse_use_version)

  # Hardcode API compatibility functions since we have full control
  # over the subrpoject version
  if get_option('fuse_version') == '2'
    conf_data.set('HAVE_DECL_FUSE_ADD_DIRENT',          0)
    conf_data.set('HAVE_DECL_FUSE_ADD_DIRENTRY',        1)
    conf_data.set('HAVE_DECL_FUSE_DAEMONIZE',           1)
    conf_data.set('HAVE_DECL_FUSE_SESSION_REMOVE_CHAN', 1)
    conf_data.set('HAVE_NEW_FUSE_UNMOUNT',              1)
  elif get_option('fuse_version') == '3'
    conf_data.set('HAVE_DECL_FUSE_ADD_DIRENT',          0)
    conf_data.set('HAVE_DECL_FUSE_ADD_DIRENTRY',        1)
    conf_data.set('HAVE_DECL_FUSE_DAEMONIZE',           1)
    conf_data.set('HAVE_DECL_FUSE_SESSION_REMOVE_CHAN', 0)
  endif

elif fuse_dep.found()

  # Detect API compatibility since we don't know the exeact fuse version
  prefix = '''
    #define FUSE_USE_VERSION @0@
    #include <fuse_lowlevel.h>
    #include <fuse.h>
  '''.format(fuse_use_version)
  conf_data.set('FUSE_USE_VERSION', fuse_use_version)
  conf_data.set10('HAVE_DECL_FUSE_ADD_DIRENT',          cc.has_function('fuse_add_dirent',          prefix: prefix, dependencies: fuse_dep))
  conf_data.set10('HAVE_DECL_FUSE_ADD_DIRENTRY',        cc.has_function('fuse_add_direntry',        prefix: prefix, dependencies: fuse_dep))
  conf_data.set10('HAVE_DECL_FUSE_DAEMONIZE',           cc.has_function('fuse_daemonize',           prefix: prefix, dependencies: fuse_dep))
  conf_data.set10('HAVE_DECL_FUSE_SESSION_REMOVE_CHAN', cc.has_function('fuse_session_remove_chan', prefix: prefix, dependencies: fuse_dep))

  xattr_pos_src = '''
    #include <fuse.h>

    int main(void) {
      struct fuse_operations ops;
      ops.getxattr(0, 0, 0, 0, 0);
      return 0;
    }
  '''
  if cc.links(xattr_pos_src, dependencies: fuse_dep)
    conf_data.set('FUSE_XATTR_POSITION', 1)
  endif

  new_fuse_umount_src = '''
    @0@
    int main(void) {
      fuse_unmount(0, 0);
      return 0;
    }
  '''.format(prefix)
  if cc.links(new_fuse_umount_src, dependencies: fuse_dep)
    conf_data.set('HAVE_NEW_FUSE_UNMOUNT', 1)
  endif
endif


# Write the config file
configure_file(
  output: 'config.h',
  configuration: conf_data,
  install: true,
  install_dir: get_option('includedir') / 'squashfuse'
)


fuse_inc_only = declare_dependency()
if fuse_dep.found()
  fuse_inc_only = fuse_dep.partial_dependency(compile_args : true, includes: true)
endif

#
#   Compile the programs and  libraries
#   ===================================
#

# Generate `swap.h.incand` and `swap.c.inc`
swap_inc = custom_target(
  'gen-swap',
  input:    ['squashfs_fs.h'],
  output:   ['swap.h.inc', 'swap.c.inc'],
  command:  [find_program('gen_swap.sh'), '@INPUT@', '@OUTDIR@'],
)

nonstd_lib = static_library('nonstd', [
    'nonstd-daemon.c',
    'nonstd-enoattr.c',
    'nonstd-makedev.c',
    'nonstd-pread.c',
    'nonstd-stat.c',
    'nonstd-symlink.c'
  ],
  dependencies: [fuse_inc_only],
)

libsquashfuse = library(
  'libsquashfuse', [swap_inc] + [
    'cache.c',
    'decompress.c',
    'dir.c',
    'file.c',
    'fs.c',
    'hash.c',
    'stack.c',
    'swap.c',
    'table.c',
    'traverse.c',
    'util.c',
    'xattr.c',
  ],
  c_args:       ['-fno-strict-aliasing'],
  link_with:    [nonstd_lib],
  dependencies: [compression_deps, fuse_inc_only],
  install:      true,
)

install_headers([
    'cache.h',
    'common.h',
    'decompress.h',
    'dir.h',
    'file.h',
    'fs.h',
    'fuseprivate.h',
    'hash.h',
    'll.h',
    'nonstd-internal.h',
    'nonstd.h',
    'squashfs_fs.h',
    'squashfuse.h',
    'stack.h',
    'stat.h',
    'swap.h',
    'table.h',
    'traverse.h',
    'util.h',
    'xattr.h',
  ],
  subdir: 'squashfuse',
)

pkg.generate(
  libsquashfuse,
  name:        'squashfuse',
  description: 'squashfuse library to mount squashfs archives',
)

libsquashfuse_dep = declare_dependency(
    link_with:           libsquashfuse,
    include_directories: include_directories('.'),
    dependencies:        [compression_deps, fuse_inc_only],
  )

fuseprivate = static_library(
  'fuseprivate', ['fuseprivate.c', 'stat.c'],
  c_args:       ['-fno-strict-aliasing'],
  link_with:    [nonstd_lib],
  dependencies: [compression_deps, fuse_dep],
)

if get_option('enable_hl')
  squashfuse = executable(
    'squashfuse', ['hl.c'],
    c_args:       ['-fno-strict-aliasing'],
    link_with:    [libsquashfuse, fuseprivate],
    dependencies: [fuse_dep],
    install:      true,
  )
endif

if get_option('enable_ll')
  libsquashfuse_ll = library(
    'libsquashfuse_ll', ['ll.c', 'll_inode.c'],
    c_args:       ['-fno-strict-aliasing'],
    link_with:    [libsquashfuse, fuseprivate],
    dependencies: [fuse_dep],
    install:      true,
  )

  pkg.generate(
    libsquashfuse,
    name:        'squashfuse_ll',
    description: 'squashfuse low-level library to mount squashfs archives',
  )

  libsquashfuse_ll_dep = declare_dependency(
    link_with:           libsquashfuse_ll,
    include_directories: include_directories('.'),
    dependencies:        [compression_deps, fuse_dep],
  )

  squashfuse_ll = executable(
    'squashfuse_ll', ['ll_main.c'],
    c_args:       ['-fno-strict-aliasing'],
    link_with:    [libsquashfuse, libsquashfuse_ll, fuseprivate],
    dependencies: [fuse_dep],
    install:      true,
  )
endif

if get_option('enable_demo')
  executable('squashfuse_ls',      ['ls.c'],      link_with: [libsquashfuse, fuseprivate], c_args: ['-fno-strict-aliasing'])
  executable('squashfuse_extract', ['extract.c'], link_with: [libsquashfuse, fuseprivate], c_args: ['-fno-strict-aliasing'])
endif

summary(
  {
    'FUSE version': fuse_dep.found() ? fuse_dep.version() : 'DISABLED',
    'Build HL':   get_option('enable_hl'),
    'Build LL':   get_option('enable_ll'),
    'Build Demo': get_option('enable_demo'),
  },
  section: 'General',
)
