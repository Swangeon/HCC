file = open("/dev/misc/tun_driver", "rb+")

file.write("wrote")
file.read(1)

file.close()
