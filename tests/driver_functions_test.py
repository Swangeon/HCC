file = open("/dev/misc/tun_driver", "rb+")

file.write(b"wrote")
file.read(1)

file.close()
