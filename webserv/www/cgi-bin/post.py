#!/usr/bin/python3
import sys
import os

length = int(os.environ.get("CONTENT_LENGTH", 0))
data = sys.stdin.read(length)

print("Content-Type: text/html")
print("")
print("<h1>Reçu : " + data + "</h1>")