from datetime import datetime
import struct
import time
  
import paho.mqtt.client as mqtt


sensor_fd = open(f'out/sensor.csv', 'a')
event_fd = open(f'out/events.csv', 'a')

sensor_struct = '<II'
sensor_msg_size = struct.calcsize(sensor_struct)

# The callback for when the client receives a CONNACK response from the server.
def on_connect(client, userdata, flags, rc):
    print("Connected with result code "+str(rc))

    # Subscribing in on_connect() means that if we lose the connection and
    # reconnect then subscriptions will be renewed.
    client.subscribe("cat_door/sensor")
    client.subscribe("cat_door/event")

# The callback for when a PUBLISH message is received from the server.
def on_message(client, userdata, msg):
    timestamp = int(time.time())
    if msg.topic == "cat_door/event":
        event_fd.write(f'{timestamp},{msg.payload.decode("ascii")}\n')
        event_fd.flush()
    elif msg.topic == "cat_door/sensor":
        while len(msg.payload) > sensor_msg_size:
            uptime, val = struct.unpack_from(sensor_struct, msg.payload[:sensor_msg_size])
            msg.payload = msg.payload[sensor_msg_size:]
            sensor_fd.write(f'{timestamp},{uptime},{val}\n')
        sensor_fd.flush()

client = mqtt.Client()
client.on_connect = on_connect
client.on_message = on_message

client.connect("192.168.1.110", 1883, 60)

# Blocking call that processes network traffic, dispatches callbacks and
# handles reconnecting.
# Other loop*() functions are available that give a threaded interface and a
# manual interface.
client.loop_forever()
