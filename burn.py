#!/usr/bin/python
import sys

import serial

port = sys.argv[1]
filename = sys.argv[2]

ser = serial.Serial(port, 9600, timeout=10, xonxoff=True)

print 'Going into upgrade mode'
while True:
    print '.',
    sys.stdout.flush()
    ser.write('u') # enter upgrade mode if we need to
    l = ser.readline().strip()
    if l.find('LOADER')>=0:
        break
print
print 'In the bootloader'

data = file(filename).read()
ser.write(data)

s = ""
while s.find('EXIT')<0:
    c = ser.read()
    sys.stdout.write(c)
    sys.stdout.flush()
    if c in ('\r', '\b'):
        s = ""
    else:
        s+=c

print
print "Complete"
