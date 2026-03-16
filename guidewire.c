/* ASSUMPTIONS

INCLUDES:
#include <stdint.h>

FUNCTIONS DEFINED ELSEWHERE:
void move_forward();
void turn_left();
void turn_right();
void stop_robot();

ASSUMED HARDWARE:
- 3 inductors for line following and intersection detection (left, center, right)
- assume the path is inductor -> amplifier -> filter -> rectifier -> ADC (not yet completed, Rex/Nathan update this once hardware done)
- ADC channels are defined as LEFT_COIL_ADC_CHANNEL, CENTER_COIL_ADC_CHANNEL, RIGHT_COIL_ADC_CHANNEL (undetermined yet, figure out by Friday)

ADC:
- assume ADC handle is provided to us from the main function/other code

*/

#define LEFT_COIL_ADC_CHANNEL 0
#define CENTER_COIL_ADC_CHANNEL 1
#define RIGHT_COIL_ADC_CHANNEL 2

#define GUIDE_THRESHOLD 50 // minimum sensor value used to distinguish the guide-wire signal from noise, determine later experimentally


/* ADC READING FUNCTION */

uint16_t read_adc(uint8_t channel)
{
    ADC1->CHSELR = (1 << channel);        // select ADC channel

    ADC1->CR |= ADC_CR_ADSTART;           // start conversion

    while(!(ADC1->ISR & ADC_ISR_EOC));    // wait until conversion finished

    return ADC1->DR;                      // return ADC result
}  

  typedef enum {
    GUIDE_LEFT,
    GUIDE_CENTER,
    GUIDE_RIGHT,
    GUIDE_LOST
    } guidestate;

/* GUIDE WIRE SENSING FUNCTIONS */

int read_left_sensor(void)
{
    return read_adc(LEFT_COIL_ADC_CHANNEL);

}

int read_center_sensor(void)
{
    return read_adc(CENTER_COIL_ADC_CHANNEL);
}

int read_right_sensor(void)
{
    return read_adc(RIGHT_COIL_ADC_CHANNEL);
}

/* GUIDE WIRE CONTROL FUNCTIONS */

guidestate guidewire_follow(void)
{

    uint16_t left_value = read_left_sensor();
    uint16_t center_value = read_center_sensor();
    uint16_t right_value = read_right_sensor();

    if (center_value > GUIDE_THRESHOLD && center_value >= left_value && center_value >= right_value) {
        return GUIDE_CENTER; 


    } else if (left_value > GUIDE_THRESHOLD && left_value > center_value && left_value > right_value) {
        return GUIDE_LEFT; 
    } 

    else if (right_value > GUIDE_THRESHOLD && right_value > center_value && right_value > left_value) {
        return GUIDE_RIGHT; // Veering right
    } 

    else {
        return GUIDE_LOST; // Guide wire lost
    } 
}


void guidewire_control(void)
{
    guidestate state = guidewire_follow();

    switch (state) {
        case GUIDE_CENTER:
            move_forward();
            break;
        case GUIDE_LEFT:
            turn_left();
            break;
        case GUIDE_RIGHT:
            turn_right();
            break;
        case GUIDE_LOST:
            stop_robot();
            break;
    }
}

int intersection_detection(void)
{
    uint16_t left = read_left_sensor();
    uint16_t center = read_center_sensor();
    uint16_t right = read_right_sensor();

    if(center > GUIDE_THRESHOLD && left > GUIDE_THRESHOLD && right > GUIDE_THRESHOLD)
    {
        return 1; // Intersection detected
    }

    return 0; // No intersection

}