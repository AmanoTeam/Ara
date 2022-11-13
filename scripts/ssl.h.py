#!/usr/bin/env python3

import os
import http.client
import urllib.parse
import hashlib

CA_CERT_URL = "https://curl.se/ca/cacert-2022-10-11.pem"
CA_CERT_SHA256 = "2cff03f9efdaf52626bd1b451d700605dc1ea000c5da56bd0fc59f8f43071040"

url = urllib.parse.urlparse(url = CA_CERT_URL)

print("Fetching data from %s" % CA_CERT_URL)

connection = http.client.HTTPSConnection(host = url.netloc, port = url.port)

connection.request(method = "GET", url = url.path)

response = connection.getresponse()
content = response.read()

connection.close()

assert hashlib.sha256(content).hexdigest() == CA_CERT_SHA256, "SHA 256 hash didn't match!"

with open(file = "../src/cacert.h", mode = "w") as file:
	file.write("static const char CACERT[] = ")
	
	for line in content.decode().splitlines():
		if line.startswith("#") or not line:
			continue
		
		file.write('\n\t"%s\\r\\n"' % line)
	
	file.write(";\n")
