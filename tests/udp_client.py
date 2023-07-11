from pypacker.layer3 import ip
from pypacker.layer4 import udp

with open("/dev/misc/tun_driver", "rb+") as file:
  echo = b"CLIENTECHO"
	print("Making Packet...")
	ip_header = ip.IP(p=ip.IP_PROTO_UDP, src_s="1.1.1.1", dst_s="1.1.1.1")
	udp_packet = udp.UDP(sport=3334, dport=3333, body_bytes=echo)
	full_packet = ip_header + udp_packet
	print("%s" % full_packet)
	print("Sending...")
	file.write(full_packet.bin())
	print(f"Reading Data...")
	try:
		file.read()
	except OSError:
		received = file.read(51)
		print(f"Data -> {received}")
		#received = ip.IP(received)
		#print("Packet %s" % received)
