## Plan: Phases for Universal ESP2 Firmware

Define clear development and operational phases to structure the implementation and evolution of the universal ESP2 firmware as described in your `Idea.md`.

### Steps
1. **Phase 1: Core Communication & Envelope**
   - Implement basic ESP-NOW peer-to-peer messaging with a standardized, extensible envelope structure.
   - Ensure all ESP2s can broadcast and parse enveloped messages.

2. **Phase 2: WiFi/Server Scanning & Mode Switching**
   - Add periodic WiFi scanning for known/open networks and server reachability checks.
   - Enable dynamic switching between ESP-NOW and WiFi/server communication modes.

3. **Phase 3: Peer Discovery & Handshake**
   - Develop ESP2 "ping" and handshake protocol for peer detection, including security/validation.
   - Implement reply logic to confirm receipt and share own info, avoiding message loops.

4. **Phase 4: Triangulation & Relative Positioning**
   - Use RSSI from handshakes to estimate and periodically update relative positions (N/S/E/W, distance) of nearby ESP2s.

5. **Phase 5: Message Relaying & Holding**
   - Enable ESP2s to store, update, and relay messages from peers, including tracking relay history and delivery status.
   - Implement logic for forwarding messages to the monitor/server when possible.

6. **Phase 6: Robustness & Optimization**
   - Add persistence (flash storage if needed), optimize memory usage, and refine security.
   - Test for edge cases, scalability, and resilience to network changes.
