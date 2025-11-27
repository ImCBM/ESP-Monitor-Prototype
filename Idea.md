I like what we currently have, but we'll expand. Each userESP2 is universal sharing the same code so it needs to intercommunicate, as whole as to the ESP1
- Keep in mind using the async or have two process at once
- These type of ESPs are communicating with each other so this firmware should be universal
- "Messages" should be "enveloped/structured". And can be further "enveloped", an envelope inside an envelope and s on

1. They have to periodically check if there is an available WiFi in the area that corresponds to the list of SSID/Password or if it's openly available, and can actually contact the server (server not built for now)
It should also recognize if the server is in the organization itself, like connect to the laptop hotspot

2. Nearby ESP detector
The multiple ESP2s are able to "ping" each other using ESPNOW, sending message infos about them like their IDs, Owner, RSSI and etc to all when it broadcasts it. This message should be a packed/enveloped structure so it is standardized and those not in the standard are disregarded. Also can have some form of "security check" to make sure it is an appropriate ESP, not just a random one.

And when that envelope message is received, it should also send one back confirming it's receiving and also sending it's own info as well.
Make sure it doesn't become a messaging loop. And this different from the messages sent the monitoring.


2.1. Nearby ESP triangulation 
Now once the "handshake/s" has been made, and there is sufficient amount of ESP2s in the area, I wanna be able to estimate their relative location/direction to a userESP2 being viewed by the monitoring. 

This triangulation should use the ESPNOW pings from earlier to measure the approximate distance and direction(just N/S/E/W no up or down height)from  each other. This should happen periodically.

From the monitoring POV:
Select ESP2A, and we can see that ESP2B is to it's left 5m, ESP2C is to it's immediate front and ESP2D is on its right.
The same should happen if ESP2B is selected, meaning ESP2A is to it's right 5m, ESP2D is to it's upper left 5m and so on


2. Relaying and Message holding.
Periodically the ESP2s update the monitor right? If ESP2A doesnt have internet access, it would sent via ESPNOW to other nearby ESPs that might have and would send their update to the monitor on their behalf, with extra data to track which ESPs recieved and relayed and when and etc. 

ESP2A (without internet) ESPNOW broadcast an update message to nearby ESPs, ESP2B (with Internet) receives it and sends it to the monitoring, with extra info like IDs to tell that ESP2B is the one that sent ESP2A's update.
If ESP2B doesn't have internet it then also relays again ESP2A's message to other ESPs that might have, and so on.

Now for both ESPs, they should hold that message in their storage and just modify that for further updates. This message is being held incase an ESP with the message will have internet access, and can now send what theyve been having.
ESPs should be able to hold multiple messages from others, and updates those messages if they received again from the same ESPs. 



