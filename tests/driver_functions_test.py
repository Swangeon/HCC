file = open("/dev/misc/tun_driver", "rb+")

input("Press enter to continue to write")
file.write(b"0000000000000000000000000800450000549ca40000fe01f7dc0a0a0a0a0a0a0a0a0800e80357020019039856486efd050008090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f202122232425262728292a2b2c2d2e2f3031323334353637")
input("Press enter to continue to read")
file.read(1)

file.close()
