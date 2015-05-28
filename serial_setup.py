import serial

# Setting up serial ports for 3 megabaud in C is hard...

port_format = "/dev/ttyUSB%d" 
baud = 3000000
found = 0

for i in range(10):
    port = port_format % i
    try:
        s=serial.Serial(port, baud)
        s.timeout = 1 # 1 second timeout on reads
        s.setRTS(True)
        print "Found:", port
        found += 1
    except:
        pass

if found:
    print "Configured %d port(s) for %d baud." % (found, baud)
else:
    print "No serial ports found (e.g. '%s')" % (port_format % 1)
