# ClientServerCpp (Monitoring System)

## Metrics Emulator

The metrics emulator is a tool that simulates client activity by sending random metric data to the monitoring server every 15 seconds.

### Building the Emulator

```bash
mkdir -p build && cd build
cmake ..
make -j
```

### Running the Emulator

```bash
# With default settings (localhost:8080, project_id="emulator_project")
./emulator/emulator

# With custom settings
./emulator/emulator <host> <port> <project_id>
```

The emulator will:
1. Register the project with the server
2. Begin sending random metrics every 15 seconds
3. Continue until you press Enter to stop it

Each metric will have random values with timestamps, random tags, and a randomly selected metric type (DOT or SPEED).
