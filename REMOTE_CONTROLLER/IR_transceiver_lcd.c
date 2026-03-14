//  EFM8LB1 IR Transmitter + LCD Display Controller (FULL SPEED)
//  ~C51~

#include <EFM8LB1.h>
#include <stdlib.h>
#include <stdio.h>

#define SYSCLK 72000000L // SYSCLK frequency in Hz

// --- TIMER FREQUENCIES ---
#define TIMER_0_FREQ 1000L
#define TIMER_1_FREQ 2000L
#define TIMER_2_FREQ 38000L // 38kHz for IR sensor
#define TIMER_3_FREQ 4000L
#define TIMER_4_FREQ 5000L
#define TIMER_5_FREQ 6000L
#define PCA_0_FREQ 7000L
#define PCA_1_FREQ 8000L
#define PCA_2_FREQ 9000L
#define PCA_3_FREQ 10000L
#define PCA_4_FREQ 11000L

// --- LCD PIN DEFINITIONS ---
#define LCD_RS P1_1
#define LCD_E  P1_2
#define LCD_D4 P1_3
#define LCD_D5 P1_4
#define LCD_D6 P1_5
// NOTE: P1.6 IS SKIPPED. IT IS THE IR TRANSMITTER!
#define LCD_D7 P1_7
// ---------------------------

#define TIMER_OUT_0 P2_0
#define TIMER_OUT_1 P1_7
#define TIMER_OUT_2 P1_6 // IR transmitter signal
#define TIMER_OUT_3 P1_5
#define TIMER_OUT_4 P1_4
#define TIMER_OUT_5 P1_3
#define MAIN_OUT    P0_1
#define PCA_OUT_0   P1_2
#define PCA_OUT_1   P1_1
#define PCA_OUT_2   P1_0
#define PCA_OUT_3   P0_7
#define PCA_OUT_4   P0_6

char _c51_external_startup (void)
{
    // Disable Watchdog with key sequence
    SFRPAGE = 0x00;
    WDTCN = 0xDE; 
    WDTCN = 0xAD; 
  
    VDM0CN=0x80;       // enable VDD monitor
    RSTSRC=0x02|0x04;  // Enable reset on missing clock detector and VDD
    
    #if (SYSCLK == 48000000L)   
        SFRPAGE = 0x10;
        PFE0CN  = 0x10; // SYSCLK < 50 MHz.
        SFRPAGE = 0x00;
    #elif (SYSCLK == 72000000L)
        SFRPAGE = 0x10;
        PFE0CN  = 0x20; // SYSCLK < 75 MHz.
        SFRPAGE = 0x00;
    #endif
    
    // Before setting clock to 72 MHz, must transition to 24.5 MHz first
    CLKSEL = 0x00;
    CLKSEL = 0x00;
    while ((CLKSEL & 0x80) == 0);
    CLKSEL = 0x03;
    CLKSEL = 0x03;
    while ((CLKSEL & 0x80) == 0);

    // Configure the pins used for square output
    P0MDOUT|=0b_1100_0010;
    P1MDOUT|=0b_1111_1111; // ALL P1 PINS ARE PUSH-PULL (Great for LCD!)
    P2MDOUT|=0b_0000_0001;
    
    XBR0     = 0x00;                     
    XBR1     = 0X00;
    XBR2     = 0x40; // Enable crossbar and weak pull-ups
    
    // Initialize timer 0 
    TR0=0;
    TF0=0;
    CKCON0|=0b_0000_0100; 
    TMOD&=0xf0;
    TMOD|=0x01; 
    TMR0=65536L-(SYSCLK/(2*TIMER_0_FREQ));
    ET0=1;     
    TR0=1;     

    // Initialize timer 1 
    TR1=0;
    TF1=0;
    CKCON0|=0b_0000_1000; 
    TMOD&=0x0f;
    TMOD|=0x10; 
    TMR1=65536L-(SYSCLK/(2*TIMER_1_FREQ));
    ET1=1;     
    TR1=1;     

    // Initialize timer 2 (IR SENSOR TIMER)
    TMR2CN0=0x00;   
    CKCON0|=0b_0001_0000; 
    TMR2RL=(0x10000L-(SYSCLK/(2*TIMER_2_FREQ))); 
    TMR2=0xffff;   
    ET2=0;         
    TR2=1;         

    // Initialize timer 3 
    TMR3CN0=0x00;   
    CKCON0|=0b_0100_0000; 
    TMR3RL=(0x10000L-(SYSCLK/(2*TIMER_3_FREQ))); 
    TMR3=0xffff;   
    EIE1|=0b_1000_0000;     
    TMR3CN0|=0b_0000_0100;  

    // Initialize timer 4 
    SFRPAGE=0x10;
    TMR4CN0=0x00;   
    CKCON1|=0b_0000_0001; 
    TMR4RL=(0x10000L-(SYSCLK/(2*TIMER_4_FREQ))); 
    TMR4=0xffff;   
    EIE2|=0b_0000_0100;     
    TR4=1;

    // Initialize timer 5 
    SFRPAGE=0x10;
    TMR5CN0=0x00;   
    CKCON1|=0b_0000_0100; 
    TMR5RL=(0x10000L-(SYSCLK/(2*TIMER_5_FREQ))); 
    TMR5=0xffff;   
    EIE2|=0b_0000_1000; 
    TR5=1;         

    // PCA Initialization 
    SFRPAGE=0x0;
    PCA0MD=0x00; 
    PCA0L=0; 
    PCA0H=0;
    PCA0MD=0b_0000_1000; 
    PCA0CPM0=PCA0CPM1=PCA0CPM2=PCA0CPM3=PCA0CPM4=0b_0100_1001; 
    PCA0CPL0=(SYSCLK/(2*PCA_0_FREQ))%0x100; PCA0CPH0=(SYSCLK/(2*PCA_0_FREQ))/0x100;
    PCA0CPL1=(SYSCLK/(2*PCA_1_FREQ))%0x100; PCA0CPH1=(SYSCLK/(2*PCA_1_FREQ))/0x100;
    PCA0CPL2=(SYSCLK/(2*PCA_2_FREQ))%0x100; PCA0CPH2=(SYSCLK/(2*PCA_2_FREQ))/0x100;
    PCA0CPL3=(SYSCLK/(2*PCA_3_FREQ))%0x100; PCA0CPH3=(SYSCLK/(2*PCA_3_FREQ))/0x100;
    PCA0CPL4=(SYSCLK/(2*PCA_4_FREQ))%0x100; PCA0CPH4=(SYSCLK/(2*PCA_4_FREQ))/0x100;
    CR=1; 
    EIE1|=0b_0001_0000; 
    
    EA=1; // Enable interrupts
    
    return 0;
}

// ==========================================
// ============ TIMER INTERRUPTS ============
// ==========================================

void Timer0_ISR (void) interrupt INTERRUPT_TIMER0
{
    SFRPAGE=0x0;
    TMR0=0x10000L-(SYSCLK/(2*TIMER_0_FREQ));
    TIMER_OUT_0=!TIMER_OUT_0;
}

void Timer1_ISR (void) interrupt INTERRUPT_TIMER1
{
    SFRPAGE=0x0;
    TMR1=0x10000L-(SYSCLK/(2*TIMER_1_FREQ));
    // TIMER_OUT_1=!TIMER_OUT_1; <-- SILENCED TO PROTECT LCD
}

void Timer3_ISR (void) interrupt INTERRUPT_TIMER3
{
    SFRPAGE=0x0;
    TMR3CN0&=0b_0011_1111; 
    // TIMER_OUT_3=!TIMER_OUT_3; <-- SILENCED TO PROTECT LCD
}

void Timer4_ISR (void) interrupt INTERRUPT_TIMER4
{
    SFRPAGE=0x10;
    TF4H = 0; 
    // TIMER_OUT_4=!TIMER_OUT_4; <-- SILENCED TO PROTECT LCD
}

void Timer5_ISR (void) interrupt INTERRUPT_TIMER5
{
    SFRPAGE=0x10;
    TF5H = 0; 
    // TIMER_OUT_5=!TIMER_OUT_5; <-- SILENCED TO PROTECT LCD
}

void PCA_ISR (void) interrupt INTERRUPT_PCA0
{
    unsigned int j;
    SFRPAGE=0x0;
    
    if (CCF0) {
        j=(PCA0CPH0*0x100+PCA0CPL0)+(SYSCLK/(2L*PCA_0_FREQ));
        PCA0CPL0=j%0x100; PCA0CPH0=j/0x100; CCF0=0; 
        // PCA_OUT_0=!PCA_OUT_0; <-- SILENCED TO PROTECT LCD
    }
    if (CCF1) {
        j=(PCA0CPH1*0x100+PCA0CPL1)+(SYSCLK/(2L*PCA_1_FREQ));
        PCA0CPL1=j%0x100; PCA0CPH1=j/0x100; CCF1=0; 
        // PCA_OUT_1=!PCA_OUT_1; <-- SILENCED TO PROTECT LCD
    }
    if (CCF2) {
        j=(PCA0CPH2*0x100+PCA0CPL2)+(SYSCLK/(2L*PCA_2_FREQ));
        PCA0CPL2=j%0x100; PCA0CPH2=j/0x100; CCF2=0; PCA_OUT_2=!PCA_OUT_2;
    }
    if (CCF3) {
        j=(PCA0CPH3*0x100+PCA0CPL3)+(SYSCLK/(2L*PCA_3_FREQ));
        PCA0CPL3=j%0x100; PCA0CPH3=j/0x100; CCF3=0; PCA_OUT_3=!PCA_OUT_3;
    }
    if (CCF4) {
        j=(PCA0CPH4*0x100+PCA0CPL4)+(SYSCLK/(2L*PCA_4_FREQ));
        PCA0CPL4=j%0x100; PCA0CPH4=j/0x100; CCF4=0; PCA_OUT_4=!PCA_OUT_4;
    }
    CF=0;
}

// ==========================================
// ========== LCD DRIVER FUNCTIONS ==========
// ==========================================

void lcd_delay_ms(unsigned int ms) {
    volatile unsigned int i, j;
    for(i = 0; i < ms; i++) {
        for(j = 0; j < 20000; j++) {} 
    }
}

void lcd_delay_us(unsigned int us) {
    volatile unsigned int i, j;
    for(i = 0; i < us; i++) {
        for(j = 0; j < 20; j++) {}
    }
}

void lcd_pulse_enable(void) {
    LCD_E = 1;
    lcd_delay_us(50); 
    LCD_E = 0;
    lcd_delay_us(100); 
}

void lcd_send_nibble(unsigned char nibble) {
    LCD_D4 = (nibble & 0x01) ? 1 : 0;
    LCD_D5 = (nibble & 0x02) ? 1 : 0;
    LCD_D6 = (nibble & 0x04) ? 1 : 0;
    LCD_D7 = (nibble & 0x08) ? 1 : 0;
    lcd_pulse_enable();
}

void lcd_command(unsigned char cmd) {
    LCD_RS = 0; 
    lcd_send_nibble(cmd >> 4);   
    lcd_send_nibble(cmd & 0x0F); 
    
    if (cmd < 4) {
        lcd_delay_ms(5); 
    } else {
        lcd_delay_us(100);
    }
}

void lcd_data(unsigned char data_char) {
    LCD_RS = 1; 
    lcd_send_nibble(data_char >> 4);
    lcd_send_nibble(data_char & 0x0F);
    lcd_delay_us(100);
}

void lcd_init(void) {
    lcd_delay_ms(100); 
    
    LCD_RS = 0;
    LCD_E = 0;

    lcd_send_nibble(0x03);
    lcd_delay_ms(10);      
    
    lcd_send_nibble(0x03);
    lcd_delay_ms(2);       
    
    lcd_send_nibble(0x03);
    lcd_delay_ms(2);
    
    lcd_send_nibble(0x02);
    lcd_delay_ms(2);

    lcd_command(0x28); 
    lcd_command(0x08); 
    lcd_command(0x01); 
    lcd_delay_ms(10);  
    lcd_command(0x06); 
    lcd_command(0x0C); 
}

void lcd_print(char* str) {
    while (*str) {
        lcd_data(*str++); 
    }
}

void lcd_set_cursor(unsigned char row, unsigned char col) {
    unsigned char address;
    if (row == 0) address = 0x80 + col; 
    else          address = 0xC0 + col; 
    lcd_command(address);
}

// ==========================================
// ====== IR TRANSMISSION FUNCTIONS =========
// ==========================================

void wait_cycles(unsigned int n, unsigned char burst)
{
    unsigned int count = 0;
    SFRPAGE = 0x00; 
    while(count < n)
    {
        while(!(TMR2CN0 & 0x80)); // Wait for Timer 2 overflow
        TMR2CN0 &= ~0x80;         // Clear flag
        
        if (burst == 1) 
        {
            TIMER_OUT_2 = !TIMER_OUT_2; // Toggle 38kHz wave (P1.6)
        } 
        else 
        {
            TIMER_OUT_2 = 0;            // Silence
        }
        count++;
    }
}

void send_space(void) { wait_cycles(38, 0); }
void send_header(void) { wait_cycles(32, 1); send_space(); }
void send_zero(void) { wait_cycles(16, 1); send_space(); }
void send_one(void) { wait_cycles(64, 1); send_space(); }

void send_forward(void) {
    send_header(); send_one(); send_zero(); send_zero(); send_one(); // 1001
}
void send_left(void) {
    send_header(); send_zero(); send_zero(); send_zero(); send_one(); // 0001
}
void send_right(void) {
    send_header(); send_one(); send_zero(); send_zero(); send_zero(); // 1000
}
void send_stop(void) {
    send_header(); send_zero(); send_zero(); send_zero(); send_zero(); // 0000
}

// ==========================================
// ================= MAIN ===================
// ==========================================

void main (void)
{
    _c51_external_startup();
    
    // --- LCD STARTUP ---
    lcd_init();                    // Wake up the screen, wipe the gibberish
    lcd_set_cursor(0, 0);          // Move to Top left corner
    lcd_print("System Ready");     // Print a test message!
    // -------------------
    
    while(1)
    {
        // Update screen and send command
        lcd_set_cursor(1, 0);      // Bottom left corner
        lcd_print("Sending STOP... "); 
        
        send_stop();
        
        {
            unsigned long delay;
            for(delay = 0; delay < 900000; delay++); 
        }
    }
}
