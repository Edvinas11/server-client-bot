## STRUCTURE

### Central server
- maintains a list of registered clients
- collects incoming messages (forwarded by the three servers)

### Three servers each listening on a different port
- Each server accepts client connections.
- When a client sends a message, the server forwards it (username + text) to the central server
- The servers themselves don’t do the bad-word check

### Admin bot client that connects to the central server that
- registers a new user
- blacklists a user if the user says something bad in 1min interval after joining