from __future__ import print_function

import gdb
import re
import subprocess
import sys


class UpdateSymbols(gdb.Command):
  'Command to load symbol files directly from /proc/<pid>/maps.'
  

  def __init__(self):
    sup = super(UpdateSymbols, self)
    sup.__init__('update-symbols', gdb.COMMAND_FILES, gdb.COMPLETE_NONE,
                 False)

    self.libs = {}
    gdb.execute('alias -a wr = update-symbols', True)
    gdb.execute('alias -a wine-reload = update-symbols', True)
    gdb.execute('alias -a us = update-symbols', True)
  

  def invoke(self, arg, from_tty):
    libs = {}
    pid = gdb.selected_inferior().pid
    if not pid in self.libs: self.libs[pid] = {}

    def command(cmd):
      to_string = not from_tty
      gdb.execute(cmd, from_tty=from_tty, to_string=to_string)

    def execute(cmd):
      return subprocess.check_output(cmd, stderr=subprocess.STDOUT) \
                       .decode('utf-8')


    with open('/proc/{}/maps'.format(pid), 'r') as maps:
      for line in maps:
        addr, _, _, _, node, path = (re.split(r'\s+', line) + [''])[:6]
        if node == '0': continue
        if path in libs: continue
        libs[path] = int(addr.split('-')[0], 16)


    for k in set(libs) & set(self.libs[pid]):
      if libs[k] != self.libs[pid][k]:
        command('remove-symbol-file "{}"'.format(k))
        del self.libs[k]


    for k in set(libs) - set(self.libs[pid]):
        if arg is not None and re.search(arg, k) is None: continue
        self.libs[pid][k] = libs[k]

        try:
          out = execute(['file', k])
        except:
          continue

        try:
          out = execute(['readelf', '-l', k])
          for line in out.split('\n'):
            if not 'LOAD' in line: continue
            b = int(line.split()[2], 16)
            break

          out = execute(['objdump', '-h', k])
          for line in out.split('\n'):
            if not '.text' in line: continue
            o = int(line.split()[3], 16) - b
            break
        except:
          try:
            out = execute(['objdump', '-h', k])
            for line in out.split('\n'):
              if not '.text' in line: continue
              o = int(line.split()[5], 16)
              break
          except:
            continue

        command('add-symbol-file "{}" 0x{:x}'.format(k, libs[k] + o))


UpdateSymbols()
