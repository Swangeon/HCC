import subprocess as sp
# You'll have to run `ifconfig tun inet create` before you run this program
sp.run(["ifconfig", "tun", "inet", "1.1.1.1", "255.255.255.0", "up"])
sp.run(["route", "delete", "tun", "1.1.1.1", "host", "local"])
sp.run(["route", "add", "tun", "1.1.1.1", "host"])
