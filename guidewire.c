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

#define LEFT_COIL_ADC_CHANNEL ADC_CHANNEL_0
#define CENTER_COIL_ADC_CHANNEL ADC_CHANNEL_1
#define RIGHT_COIL_ADC_CHANNEL ADC_CHANNEL_2

#define GUIDE_THRESHOLD 50 // minimum sensor value used to distinguish the guide-wire signal from noise, determine later experimentally

/* ADC READING FUNCTION */

uint16_t read_adc(uint32_t channel)
{

    ADC_ChannelConfTypeDef sConfig = {0};
    
    sConfig.Channel = channel;
    sConfig.Rank = ADC_REGULAR_RANK_1;
    sConfig.SamplingTime = ADC_SAMPLETIME_92CYCLES_5; // Adjust as needed for signal stability, determine experiemtnally

    HAL_ADC_ConfigChannel(&hadc1, &sConfig);

    HAL_ADC_Start(&hadc1);
    HAL_ADC_PollForConversion(&hadc1, HAL_MAX_DELAY);

    return_value = HAL_ADC_GetValue(&hadc1);
    HAL_ADC_Stop(&hadc1);
    return return_value;

}  

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

void guidewire_follow(void)
{

    uint16_t left = read_left_sensor();
    uint16_t center = read_center_sensor();
    uint16_t right = read_right_sensor();

    if (center > GUIDE_THRESHOLD && center >= left && center >= right) 
    {
        move_forward();
    } 
    
    else if (left > GUIDE_THRESHOLD && left > center) 
    {
        turn_left();
    } 
    
    else if (right > GUIDE_THRESHOLD && right > center) 
    {
        turn_right();
    } 
    
    else 
    {
        stop_robot(); // Lost the line, stop or implement a search pattern
    }
       
}

int intersection_detection(void)
{
    int left = read_left_sensor();
    int center = read_center_sensor();
    int right = read_right_sensor();

    if(center > GUIDE_THRESHOLD && left > GUIDE_THRESHOLD && right > GUIDE_THRESHOLD)
    {
        return 1; // Intersection detected
    }

    return 0; // No intersection

}