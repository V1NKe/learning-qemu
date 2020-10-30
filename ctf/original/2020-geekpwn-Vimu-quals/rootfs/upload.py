import base64

fin = open("./exp", "rb")
fout = open("./base64.txt", "w")

data = fin.read()
base64_str = base64.encodestring(data).decode('ascii')

fout.write(base64_str.replace("\n", "").replace("\r", ""))

fin.close()
fout.close()
