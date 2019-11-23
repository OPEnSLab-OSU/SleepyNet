# Notes On Designing A Protocol Stack Framework

I have been handed the task of creating a network framework for Loom, a library using many different methods of communication. I would like to encapsulate as much functionality as possible with my logic and use well-known models so extending functionality is simple later on. These are some notes from my researchL

## Initial Research

### Network Layers

Networks operate in layers, with each layer adding a header to the data containing information. This allows each layer to communicate as if it was communicating horizontally, or directly to it's respective layer.

```
 Machine 1       Machine 2
+---------+     +---------+
| HTTP    | - - | HTTP    |
+---------+     +---------+
     |               |
+---------+     +---------+
| SSL     | - - | SSL     |
+---------+     +---------+
     |               |
+---------+     +---------+
| TCP     | - - | TCP     |
+---------+     +---------+
     |               |
+-------------------------+
|        Hardware         |
+-------------------------+
             |
+------------+------------+-------------+------+
| TCP Header | SSL Header | HTTP Header | Data |
+------------+------------+-------------+------+

```
A visual diagram of the network layers, and the packets they produce. If working properly, each layer won't even notice if one of the other layers implementation changes.

Since layers are interoperable, it is important to consider which are needed depending on the design of your project. Some ideas:
* Reliability - How does this protocol implement
  * Error detection/correction (TCP)
  * Routing correction (broken node?)
* Evolution of network, as the network grows, how will these change?
  * Protocol layering
  * Addressing (IPv4 vs. IPv6)
  * Internetworking (moving between different networks with different hardware limitations)
  * Scalability
* Resource allocation, who runs the network?
  * Multiplexing/Congestion control
  * Flow control (receiver communicates with sender to limit data rate)
  * Quality of service (consistency of latency)
* Security
  * Confidentiality
  * Authentication
  * Integrity

Service is a set of primitives provided to the layer above a given layer. Two main kinds of services offered by a network layer:
* Connection: Must undergo some sort of negotiation to complete the service (TCP, SSL).
  * Generally more reliable, as a connection allows for checking at a fine-grained level.
  * Two subcategories: message sequence, or byte stream.
* Connectionless: All data needed for the transmission is contained in the data (simple packet, IR). Often referred to as a datagram service.

### The OSI Model

``` 
+--------------+ 
| Application  |
+--------------+     
| Presentation |     
+--------------+   
| Session      |
+--------------+     
| Transport    |
+--------------+  
| Network      |
+--------------+     
| Data Link    |  
+--------------+   
| Hardware     |
+--------------+
```
* **Physical layer**: Electrical signals and whatnot.
* **Data Link**: Handles error correction, retransition, flow control, and access control.
* **Network Layer**: Handles routing and congestion control
* **Transport Layer**: Handles input/output from user code, and ensures that data is intact on the other end. This layer will behave as if it is end-to-end, and will ignore routing steps.
* **Session Layer**: Maintain dialog between machines.
* **Presentation Layer**: Provides abstract data structures for higher-level data to be defined.
* **Application Layer**: Code the user writes.

Most often the session and presentation layers are implemented in the application layer instead.

IEEE standards, ISO standards are good implementations of most of the layers. Loom will probably need to implement the transport layer and some of the application layer.
802.15.4 - Low frequency mesh networking.

## OPEnS Applications
Network Stack:
```
+--------------+ 
| Application  |  HTTP Query
+--------------+     
| Transport    |  Ethernet (WIZNet chip) or Wifi (AWINC chip)
+--------------+  
| Network      |  Ethernet (WIZNet chip) or Wifi (AWINC chip)
+--------------+     
| Data Link    |  Ethernet (WIZNet chip) or Wifi (AWINC chip)
+--------------+   
| Hardware     |  Ethernet (WIZNet chip)
+--------------+
```

In general, there are a few main networking applications used by the OPEnS lab:
* Ethernet (Transport Layer)
* WiFi (Transport Layer)
* LoRa (Data Link Layer w/ ECC)
* FreeWave (Data Link Layer)
* GPS (Transport Layer)

I plan on categorizing these solutions into two categories: *web connectivity* (Ethernet, WiFi, GPS) and *peer connectivity* (LoRa and FreeWave). Peer connectivity refers to talk between devices, and web connectivity refers to talk between devices and services such as PushingBox and AWS. In general, I plan to keep these interfaces separate--since the goal of web connectivity (publish data) is usually different than the goal of peer connectivity (update state), it makes sense to me that they should have separate interfaces.

### Peer Connectivity

* Datagram based

Networking implementations have the following considerations:
* Addressing
* Sleep/inactivity
* Topology
* Ease of development/debugging
* Platform independent

Protocol ideas:
* 6LoWPAN: https://github.com/hso-esk/emb6/wiki/emb6-New-Network-Stack
* ZigBee
* Thread

All of these protocols are based off of the 802.15.4 MAC standard. For this reason, I think it would be a smart idea to make all of our peer transmissions 802.15.4 MAC compliant (or at least implementing the same functionality) and then build a protocol on top of that.

#### Peer Connectivity Specification

![Network Topology](images/zigg.jpg)

After a conversation with Chet, we decided on a specification for a Loom low-power networking device:
* The network will consist of three types of devices: Coordinator, Router, and End device:
  * The *Coordinator* will serve as the central router and scheduler for the Loom network. All traffic sent by devices will be to a single Coordinator, and all traffic received from devices will be from the Coordinator. It is the Coordinators responsibility to ensure devices are given proper transmission timing. There will be a single Coordinator per network.
  * A *Router* will serve as an intermediate between a Coordinator and an End device, relaying data up the star to the Coordinator and back. Connections between routers cannot generate circular patterns. Routers will spend more time transmitting and receiving, but will otherwise behave similarly to an endpoint device.
  * An *Endpoint Device* represents a Loom sensor device. These devices will send/receive data from a Router or a Coordinator. An End device can only communicate with a Router or Coordinator.
* The network will allow any device to remain in powered-off mode (RTC deep sleep) for a fixed window of time.
* A node can transmit at any time to the hub, but can only transmit for a limited time window to a router (the router will be asleep most of the time).
* The network will not account for failed nodes, and will assume that any failure detection will be done at the application layer.
* The network will be abstracted from the radio it is implemented on.
* Transmissions on the network will be in the form of periodic reliable datagrams:
  * Datagram reception is assumed to be extremely important to the application.
  * Datagram latency is assumed to be unimportant to the application.
  * Datagram will be sent over the network at periodic intervals. These intervals can vary by device, however the interval for a device must remain fixed for the life of the network.
* Topology and device addresses in network will be preprogrammed into each device.

Some key constraints:
* Events can only be passed from end device->coordinator.
* Bandwidth is low (it is probably prudent to switch network implementation on event)
* Latency is high

#### Medium Access Control

Given the above specification, I am now tasked with finding a protocol that control media access, or who gets to talk when. There are a couple of considerations when selection a protocol:
* Energy Efficiency
* Scalability
* Adaptability
* Low Latency and Predictability
* Reliability
  
In general, it seems like the OPEnS lab optimized for energy efficiency and reliability. I would also like to choose a standard that is scalable however, so this will be a factor in consideration. It should also be noted that our network will not need to be adaptable at all, which can cut a considerable amount of overhead.

A MAC protocol can invoke several actions on a radio:
* Check channel for activity
* Receive data
* Transmit data

For LoRa, check < receive < transmit, however check is non-zero power consumption, and cannot be done continuously. LoRaWAN manages this cleverly by allowing LoRa nodes to transmit at any time, a using a gateway to manage their channels and data rates to ensure the nodes don't collide. LoRaWAN nodes also only offer two receive windows after transmission to minimize the time spent receiving data. LoraWAN gateways must be able to detect/receive data on all channels/frequencies, since nodes can transmit on any one at any time.

An idea, derived from the [WiseMAC](http://perso.citi.insa-lyon.fr/trisset/cours/rts12/articles/AD04-Wisemac.pdf): Nodes all have independent intervals at a fixed rate at which they sample the medium, and every node knows each other nodes rate. If a node would like to transmit data to another node, it simply needs to wake up at the correct interval and transmit a packet.

In general, there are a few broad categories for MAC protocols:
##### Scheduled (Both centralized and decentralized)
Pros:
* Bandwidth is guaranteed, and there will never be a collision on the network.
  * This means the minimum number of transmission possible.
* Sleep is periodic and predictable.
Cons:
* More complicated than other categories
* Requires at least one device to be listening through all time slots (does not optimize for receive energy).
* Not very scalable.
* Does not allow for high bandwidth.

#### Contention-Based
Pros:
* Nodes can do all network activity at same fixed interval
  * Evens out power consumption
* 

Cons:
* Device must be able to handle a collision.
* Retransmissions or additional protocols are standard (does not optimize for transmission).
* Node behavior can be unpredictable for short time samples
* 
