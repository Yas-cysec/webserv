print("Content-Type: text/plain")
print()
with open("data.txt", "r") as file:
    print(file.read())
