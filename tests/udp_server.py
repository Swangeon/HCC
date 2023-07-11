import socket as s
from time import sleep

a = s.socket(s.AF_INET, s.SOCK_DGRAM)
a.bind(("1.1.1.1", 3333))
while a:
	print("Receiving...")
	echo, addr = a.recvfrom(1024)
	print(f"Data -> {echo}")
	print("Sending...")
	a.sendto(b"SERVERECHO", addr)
	a.sendto(b"SERVERECHO", addr)
