#include <EFM8LB1.h>

#define SYSCLK 72000000L
#define TIMER_2_FREQ 38000L
#define TIMER_OUT_2 P1_6

// --- Function Prototypes ---
void wait_cycles(unsigned int n);
void send_byte(unsigned char cmd);
void Waitms(unsigned int ms);

// Empty startup to avoid compiler confusion
char _c51_external_startup (void) {
    WDTCN = 0xDE; WDTCN = 0xAD; // Disable Watchdog
    return 0; 
}

void main (void)
{
    // --- STEP 1: HARDWARE INIT ---
    SFRPAGE = 0x00;
    
    // Set Clock to 72MHz
    SFRPAGE = 0x10;
    PFE0CN  = 0x20; 
    CLKSEL  = 0x03; 
    while ((CLKSEL & 0x80) == 0);
    SFRPAGE = 0x00;

    P1MDOUT |= 0x40; // P1.6 Push-Pull
    XBR2     = 0x40; // Enable Crossbar
    
    // Timer 2: 38kHz Setup
    TMR2CN0 = 0x00;   
    CKCON0 |= 0x10; 
    TMR2RL  = 65536L - (SYSCLK / (2 * TIMER_2_FREQ)); 
    TMR2    = 0xffff;   
    TR2     = 0; 

    // --- STEP 2: INFINITE LOOP ---
    while(1)
    {
        // Start Bit (3ms Burst, 2ms Gap)
        TR2 = 1;
        wait_cycles(228); 
        TR2 = 0;
        TIMER_OUT_2 = 0;
        wait_cycles(152); 

        // Send Command 0x01
        send_byte(0x01); 
        
        // Wait 100ms
        Waitms(100); 
    }
}

// --- ENGINE FUNCTIONS ---

void wait_cycles(unsigned int n)
{
    unsigned int count = 0;
    while(count < n)
    {
        if (TMR2CN0 & 0x80) // Monitor the Overflow Flag
        {
            TMR2CN0 &= ~0x80; 
            if (TR2) TIMER_OUT_2 = !TIMER_OUT_2;
            count++;
        }
    }
}

void send_byte(unsigned char cmd) {
    unsigned char i;
    for (i = 0; i < 8; i++) {
        if (cmd & 0x01) { 
            TR2 = 1; wait_cycles(92);    
            TR2 = 0; TIMER_OUT_2 = 0; wait_cycles(46); 
        } else {
            TR2 = 1; wait_cycles(46);    
            TR2 = 0; TIMER_OUT_2 = 0; wait_cycles(92); 
        }
        cmd >>= 1; 
    }
}

void Waitms(unsigned int ms) {
    unsigned int i, j;
    for(i = 0; i < ms; i++) {
        for (j = 0; j < 17000; j++); 
    }
}
