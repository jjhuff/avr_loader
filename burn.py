#!/usr/bin/python
import sys
import time

import serial

port = sys.argv[1]
filename = sys.argv[2]

ser = serial.Serial(port, 9600, timeout=1, xonxoff=True)

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

data = file(filename).readlines()

s = ""
for l in data:
    ser.write(l)
    time.sleep(.1)
    c = ser.read()
    sys.stdout.write(c)
    sys.stdout.flush()
    if c in ('\r', '\b'):
        s = ""
    else:
        s+=c
    if s.find('EXIT')>0:
        break;	

print
print "Complete"
