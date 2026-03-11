void wait_cycles(unsigned int n)
{
    unsigned int count = 0;
    while(count < n)
    {
        // Wait for Timer 2 high-byte overflow flag (set at 38kHz rate)
        while(!TF2H); 
        
        // Clear the flag manually so we can detect the next overflow
        TF2H = 0; 
        
        count++;
    }
}
