
#include "test.h"

#include "stm32f4xx_hal.h"
#include "stm32f405xx.h"
#include "assert.h"
#include "cmsis_os.h"

#include "adc.h"
#include "tim.h"
#include "spi.h"
#include "drv8301.h"

#include "math.h"
#include "stdint.h"

void start_pwm(){
    //Init PWM
    int half_load = htim1.Instance->ARR/2;
    htim1.Instance->CCR1 = half_load;
    htim1.Instance->CCR2 = half_load;
    htim1.Instance->CCR3 = half_load;
    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);
    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_2);
    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_3);

    htim1.Instance->CCR4 = 1;
    HAL_TIM_PWM_Start_IT(&htim1, TIM_CHANNEL_4);

    //Turn off output
    //__HAL_TIM_MOE_DISABLE(&htim1);

}

DRV8301_Obj gate_drivers[] = {
    {
        //M0

        .spiHandle = &hspi3,
        //Note: this board has the EN_Gate pin shared!
        .EngpioHandle = EN_GATE_GPIO_Port,
        .EngpioNumber = EN_GATE_Pin,
        .nCSgpioHandle = M0_nCS_GPIO_Port,
        .nCSgpioNumber = M0_nCS_Pin,
        .RxTimeOut = false,
        .enableTimeOut = false
    }
};
static const int num_motors = sizeof(gate_drivers)/sizeof(gate_drivers[0]);

//Local view of DRV registers
static DRV_SPI_8301_Vars_t gate_driver_regs[1/*num_motors*/];

void test_DRV8301_setup() {
    for (int i = 0; i < num_motors; ++i) {
        DRV8301_enable(&gate_drivers[i]);
        DRV8301_setupSpi(&gate_drivers[i], &gate_driver_regs[i]);

        //@TODO we can use reporting only if we actually wire up the nOCTW pin
        gate_driver_regs[i].Ctrl_Reg_1.OC_MODE = DRV8301_OcMode_LatchShutDown;
        //Overcurrent set to approximately 150A at 100degC. This may need tweaking.
        gate_driver_regs[i].Ctrl_Reg_1.OC_ADJ_SET = DRV8301_VdsLevel_0p730_V;
        //20V/V on 500uOhm gives a range of +/- 150A
        gate_driver_regs[i].Ctrl_Reg_2.GAIN = DRV8301_ShuntAmpGain_20VpV;

        gate_driver_regs[i].SndCmd = true;
        DRV8301_writeData(&gate_drivers[i], &gate_driver_regs[i]);
        gate_driver_regs[i].RcvCmd = true;
        DRV8301_readData(&gate_drivers[i], &gate_driver_regs[i]);
    }
}

/////////////////////////////////////////////////
//Test adc conversion latency and triggering

void test_adc_trigger() {
    //Set trigger to mid phase to check for trigger polarity
    htim1.Instance->CCR4 = 2048;

    HAL_TIM_PWM_Start_IT(&htim1, TIM_CHANNEL_4);
    __HAL_ADC_ENABLE(&hadc2);

    //Warp field stabilize.
    osDelay(2);
    __HAL_ADC_ENABLE_IT(&hadc2, ADC_IT_JEOC);
}

static int test = 0;
static int test2 = 0;
static uint32_t testcnt[16];
static int tcidx = 0;
void test_adc_trigger_cb(ADC_HandleTypeDef* hadc) {
    assert(hadc == &hadc2);

    //float unknown_ch_volts = read_ADC_volts(hadc, 1);

    //Capture value to read with debugger
    uint32_t ADCValue = HAL_ADCEx_InjectedGetValue(hadc, 1);
    (void)ADCValue; //Dummy use to avoid warning.

    uint32_t cnt = htim1.Instance->CNT;
    int dir = htim1.Instance->CR1 & TIM_CR1_DIR;
    if(dir){
        test++;
    } else {
        test2++;
        testcnt[tcidx] = cnt - 2048;
        if(++tcidx == 16)
            tcidx = 0;
    }
}
/////////////////////////////////////////////////

static int cbcnt = 0;
void test_cb_count(){
    ++cbcnt;
}


//Test setup: Setup tests in main, and set callbacks
void test_main(void) {

    //test_adc_trigger();

    test_DRV8301_setup();
    osDelay(10000);
    start_pwm();
}

void HAL_ADCEx_InjectedConvCpltCallback(ADC_HandleTypeDef* hadc) {
    //test_adc_trigger_cb(hadc);
    test_cb_count();
}

