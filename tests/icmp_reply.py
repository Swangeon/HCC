from pypacker.layer12 import ethernet
from pypacker.layer3 import ip
from pypacker.layer3 import icmp

with open("/dev/misc/tun_driver", "rb+") as file:
  try:
    file.read() # For some reason when open is called it skips the semaphore?
  except OSError:
    print("Reading...")
    data = file.read(84)
    print(f"Read Data -> {data}\nResponding...")
    reply = ethernet.Ethernet(src_s="00:00:00:00:00:00", dst_s="00:00:00:00:00:00")
    temp = ip.IP(data)
    temp.higher_layer.type = icmp.ICMP_ECHO_REPLY
    reply += temp
    print("%s" % reply)
    print("Writing...")
    file.write(reply.bin())
    print("Done")
    
