void send_0() {
    TR2 = 1;         // LED ON (at 38kHz)
    wait_cycles(10); // Burst
    TR2 = 0;         // LED OFF
    wait_cycles(20); // Gap
}

void send_1() {
    TR2 = 1;         // LED ON (at 38kHz)
    wait_cycles(30); // Burst
    TR2 = 0;         // LED OFF
    wait_cycles(20); // Gap
}
