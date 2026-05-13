/*
Main Master
1. Send configure slave
2. Ranging as master
3. Send own ranging over WiFi
4. Switch to slave, receive config
5. Ranging as slave
6. Receive data over LoRa
7. Send ACK
8. Send Master's ranging over WiFi

Master
1. Receive configure slave
2. Ranging as slave
3. Switch to master, send configure slave
4. Ranging as master
5. Send data over LoRa
6. Receive ACK 
*/

#include "node/Anchor.h"
#include "node/NodeConfig.h"
