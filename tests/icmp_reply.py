from pypacker.layer3 import ip
from pypacker.layer3 import icmp

with open("/dev/misc/tun_driver", "rb+") as file:
    while True:
        print("Reading...")
        data = file.read(84)
        print(f"Read Data -> {data}\nResponding...")
        temp = ip.IP(data)
        temp.higher_layer.type = icmp.ICMP_ECHO_REPLY
        temp.id = 12345
        print("%s" % temp)
        print("Writing...")
        file.write(temp.bin())
        print("Done")
    
