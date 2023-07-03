from pypacker.layer3 import ip
from pypacker.layer3 import icmp

with open("/dev/net/tun_driver", "rb+") as file:
  try:
    file.read()
  except OSError:
    print("Reading...")
    data = file.read(84) # Standard length
    print(f"Read Data -> {data}\nResponding...")
    reply = ip.IP(data)
    reply.higher_layer.type = icmp.ICMP_ECHO_REPLY
    print("%s" % reply)
    
