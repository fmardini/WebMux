from websocket import create_connection

ws = create_connection("ws://localhost:4000")
ws.send("Hello, World")
for i in range(3):
  result =  ws.recv()
  print result

ws.close()

