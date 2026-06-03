#include "main.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

#include "stm32f1xx.h"
#include <string.h>
#include <stdio.h>

extern UART_HandleTypeDef huart1;

#define RX_BUF_SIZE 256
volatile uint8_t uart_idle_flag = 0; // 空闲中断标志
uint8_t rx_buf[RX_BUF_SIZE];         // 接收缓冲区
volatile uint16_t rx_index = 0;      // 接收数据索引

/* 主状态枚举（管理大步骤） */
typedef enum
{
    STATE_IDLE,  // 空闲状态（等待初始指令）
    STATE_STEP1, // 第一步所有操作
    STATE_STEP2, // 预留后续步骤...
    STATE_STEP3,
    STATE_STEP4,
    STATE_STEP5,
    STATE_STEP6,
    STATE_ERROR // 错误处理状态
} MainState;

/* 第一步的子状态枚举（管理详细操作） */
typedef enum
{
    SUBSTEP_IDLE,              // 子状态空闲
    SUBSTEP_SEND_VER_CMD,      // 发送查询版本指令
    SUBSTEP_WAIT_VER_RESPONSE, // 等待版本响应
    SUBSTEP_SEND_MAC_CMD,      // 发送查询MAC指令
    SUBSTEP_WAIT_MAC_RESPONSE, // 等待MAC响应
    SUBSTEP_SEND_ROLE_CMD,     // 发送角色配置指令
    SUBSTEP_SEND_REBOOT_CMD,   // 发送重启指令
    SUBSTEP_SEND_ROLE_AGAIN,   // 再次发送角色配置
    SUBSTEP_SEND_MAC_DATA      // 发送存储的MAC地址
} Step1Substate;

MainState current_state = STATE_IDLE;        // 当前主状态
Step1Substate step1_substate = SUBSTEP_IDLE; // 第一步的子状态
char stored_mac[32] = {0};                   // 存储从模块读取的MAC地址

// LED控制变量
typedef struct
{
    GPIO_TypeDef *GPIOx;  // LED所属GPIO组
    uint16_t Pin;         // LED引脚
    uint8_t is_blinking;  // 是否正在闪烁
    uint32_t last_toggle; // 上次状态切换时间戳
} LedControl;

LedControl step_leds[] = {
    {NULL, 0, 0, 0},                      // STATE_IDLE (无LED)
    {LED_G1_GPIO_Port, LED_G1_Pin, 0, 0}, // STATE_STEP1
    {LED_G2_GPIO_Port, LED_G2_Pin, 0, 0}, // STATE_STEP2
    {LED_G3_GPIO_Port, LED_G3_Pin, 0, 0}, // STATE_STEP3
    {LED_G4_GPIO_Port, LED_G4_Pin, 0, 0}, // STATE_STEP4
    {LED_G5_GPIO_Port, LED_G5_Pin, 0, 0}, // STATE_STEP5
    {LED_G6_GPIO_Port, LED_G6_Pin, 0, 0}, // STATE_STEP6
};

#define BLINK_INTERVAL 500 // 闪烁间隔(ms)

void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
void Control_Start(void);
void Control_Relay_PCB2(void);
void Control_Relay_PCB1(void);
void Send_AT_Command(volatile const char *cmd);
void Process_Step1(void);
void Process_Step2(void);
void Process_Step3(void);
void Process_Step4(void);
void Process_Step5(void);
void Process_Step6(void);
void Handle_Error(void);
uint8_t Wait_PCB1_Response(void);

// 更新LED闪烁状态
void UpdateLedBlink(void)
{
    for (int i = 1; i < sizeof(step_leds) / sizeof(step_leds[0]); i++)
    {
        if (step_leds[i].is_blinking)
        {
            if (HAL_GetTick() - step_leds[i].last_toggle >= BLINK_INTERVAL)
            {
                HAL_GPIO_TogglePin(step_leds[i].GPIOx, step_leds[i].Pin);
                step_leds[i].last_toggle = HAL_GetTick();
            }
        }
    }
}

// 启动指定步骤的LED闪烁
void StartStepLedBlink(MainState state)
{
    if (state >= 1 && state <= STATE_STEP6)
    {
        step_leds[state].is_blinking = 1;
        step_leds[state].last_toggle = HAL_GetTick();
        HAL_GPIO_WritePin(step_leds[state].GPIOx, step_leds[state].Pin, GPIO_PIN_SET);
    }
}

// 停止LED闪烁并保持常亮
void StopLedBlink(MainState state)
{
    if (state >= 1 && state <= STATE_STEP6)
    {
        step_leds[state].is_blinking = 0;
        HAL_GPIO_WritePin(step_leds[state].GPIOx, step_leds[state].Pin, GPIO_PIN_SET);
    }
}

#define DEBUG_NO_HARDWARE 1

void UART_IdleCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1)
    {
        HAL_UART_AbortReceive_IT(&huart1);

        // 计算实际接收长度
        rx_index = RX_BUF_SIZE - huart1.RxXferCount;
        rx_buf[rx_index] = '\0'; // 添加字符串结束符
        uart_idle_flag = 1;      // 通知主循环

        // 清空缓冲区并重启接收
        memset(rx_buf, 0, RX_BUF_SIZE); // 防止旧数据干扰
        HAL_UART_Receive_IT(&huart1, rx_buf, RX_BUF_SIZE);
        __HAL_UART_ENABLE_IT(&huart1, UART_IT_IDLE); // 重新使能IDLE中断
    }
}

int main(void)
{
    SystemClock_Config();
    MX_GPIO_Init();
    MX_USART1_UART_Init();
    HAL_NVIC_SetPriority(USART1_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(USART1_IRQn);
    MX_TIM2_Init();

    // 断开所有继电器
    Control_Start();
    HAL_Delay(5000);
    Control_Relay_PCB2();
    HAL_GPIO_WritePin(RF_KEY_GPIO_Port, RF_KEY_Pin, GPIO_PIN_RESET);
    HAL_Delay(3000);
    HAL_UART_Transmit(&huart1, (uint8_t *)"INIT OK\r\n", 9, 100);
    HAL_Delay(20);
    __HAL_UART_ENABLE_IT(&huart1, UART_IT_IDLE);
    HAL_UART_Receive_IT(&huart1, rx_buf, RX_BUF_SIZE);

    while (1)
    {
        UpdateLedBlink(); // 更新所有LED闪烁状态
        switch (current_state)
        {
        case STATE_IDLE:
#ifdef DEBUG_NO_HARDWARE
            if (1) // 强制触发"按键按下"
#else
            if (HAL_GPIO_ReadPin(KEY_START_GPIO_Port, KEY_START_Pin) == GPIO_PIN_RESET)
#endif
            {
                current_state = STATE_STEP1;
                step1_substate = SUBSTEP_SEND_VER_CMD;
            }
            break;

        case STATE_STEP1:
            Process_Step1();
            break;

        case STATE_STEP2:
            Process_Step2();
            break;

        case STATE_STEP3:
            Process_Step3();
            break;

        case STATE_STEP4:
            Process_Step4();
            break;

        case STATE_STEP5:
            Process_Step5();
            break;

        case STATE_STEP6:
            HAL_GPIO_WritePin(LED_G6_GPIO_Port, LED_G6_Pin, GPIO_PIN_SET);
            while (1)
                ;
            break;

        case STATE_ERROR:
            Handle_Error(); // 错误处理
            break;
        }
    }
}

volatile const char *g_at_ver_cmd = "AT+VER=?\r\n";
void Process_Step1(void)
{
    static uint8_t led_initialized = 0;
    if (!led_initialized)
    {
        StartStepLedBlink(STATE_STEP1); // G1开始闪烁
        led_initialized = 1;
    }
    // 断开PCB2蓝牙模块，连接PCB1蓝牙模块
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_7, GPIO_PIN_RESET);
    switch (step1_substate)
    {
    case SUBSTEP_SEND_VER_CMD:
        HAL_GPIO_WritePin(RF_KEY_GPIO_Port, RF_KEY_Pin, GPIO_PIN_SET);
        HAL_Delay(1000);
        HAL_GPIO_WritePin(RF_KEY_GPIO_Port, RF_KEY_Pin, GPIO_PIN_RESET);
        HAL_Delay(1000);
        Send_AT_Command(g_at_ver_cmd);
        HAL_Delay(1000);
        step1_substate = SUBSTEP_WAIT_VER_RESPONSE;
        break;

    case SUBSTEP_WAIT_VER_RESPONSE:
    {
        uint32_t timeout = HAL_GetTick();
        while (!uart_idle_flag)
        {
            if (HAL_GetTick() - timeout > 1000)
            { // 1秒超时
                HAL_UART_Transmit(&huart1, (uint8_t *)"ERROR: Timeout!\r\n", 16, 100);
                current_state = STATE_ERROR;
                break;
            }
        }
        if (uart_idle_flag)
        {
            if (strstr((char *)rx_buf, "AT+VER=b.1.04.120240816172339"))
            {
                step1_substate = SUBSTEP_SEND_MAC_CMD;
            }
            else
            {
                current_state = STATE_ERROR; // 版本不匹配，进入错误状态
            }
            uart_idle_flag = 0;
        }
    }
    break;

    case SUBSTEP_SEND_MAC_CMD:
        Send_AT_Command("AT+MAC=?\r\n");
        step1_substate = SUBSTEP_WAIT_MAC_RESPONSE;
        break;

    case SUBSTEP_WAIT_MAC_RESPONSE:
    {
        if (uart_idle_flag)
        {
            char *mac_ptr = strstr((char *)rx_buf, "AT+MAC=");
            if (mac_ptr && sscanf(mac_ptr, "AT+MAC=%s", stored_mac) == 1)
            {
                step1_substate = SUBSTEP_SEND_ROLE_CMD;
            }
            else
            {
                current_state = STATE_ERROR;
            }
            uart_idle_flag = 0;
        }
    }
    break;

    case SUBSTEP_SEND_ROLE_CMD:
        Send_AT_Command("AT+ROLE=2\r\n");
        step1_substate = SUBSTEP_SEND_REBOOT_CMD;
        break;

    case SUBSTEP_SEND_REBOOT_CMD:
        Send_AT_Command("AT+REBOOT=1\r\n");
        step1_substate = SUBSTEP_SEND_ROLE_AGAIN;
        break;

    case SUBSTEP_SEND_ROLE_AGAIN:

        HAL_Delay(2000); // 等待模块重启
        Control_Start();
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_7, GPIO_PIN_SET);
        Control_Relay_PCB2();
        Send_AT_Command("AT+ROLE=2\r\n");
        step1_substate = SUBSTEP_SEND_MAC_DATA;
        break;

    case SUBSTEP_SEND_MAC_DATA:
    {
        char mac_msg[40];
        sprintf(mac_msg, "AT+SEND=%s\r\n", stored_mac);
        Send_AT_Command(mac_msg);
        Send_AT_Command("AT+REBOOT=1\r\n");
        StopLedBlink(STATE_STEP1); // LED_G1停止闪烁
        HAL_GPIO_WritePin(LED_G1_GPIO_Port, LED_G1_Pin, GPIO_PIN_SET);
        current_state = STATE_STEP2; // 完成第一步
        Send_AT_Command("LED_TEST\r\n");
        step1_substate = SUBSTEP_IDLE;

        break;

    default:
        StopLedBlink(STATE_STEP1);
        led_initialized = 0;
        current_state = STATE_ERROR;
        break;
    }
    }
}

void Process_Step2(void)
{
    static uint8_t led_initialized = 0;
    if (!led_initialized)
    {
        StartStepLedBlink(STATE_STEP2); // G2开始闪烁
        led_initialized = 1;
    }

    if (HAL_GPIO_ReadPin(KEY_PASS_GPIO_Port, KEY_PASS_Pin) == GPIO_PIN_RESET)
    {
        StopLedBlink(STATE_STEP2);
        HAL_GPIO_WritePin(LED_G2_GPIO_Port, LED_G2_Pin, GPIO_PIN_SET);
        current_state = STATE_STEP3;

        Send_AT_Command("ZIGBEE_TEST\r\n");
    }
    else if (HAL_GPIO_ReadPin(KEY_UNPASS_GPIO_Port, KEY_UNPASS_Pin) == GPIO_PIN_RESET)
    {
        StopLedBlink(STATE_STEP2);
        current_state = STATE_ERROR;
        led_initialized = 0;
    }
}

void Process_Step3(void)
{
    //	Send_AT_Command("ZIGBEE_TEST\r\n");
    static uint8_t led_initialized = 0;
    if (!led_initialized)
    {
        StartStepLedBlink(STATE_STEP3); // G3开始闪烁
        led_initialized = 1;
    }

    if (Wait_PCB1_Response())
    {
        StopLedBlink(STATE_STEP3); // LED_G3停止闪烁
        HAL_GPIO_WritePin(LED_G3_GPIO_Port, LED_G3_Pin, GPIO_PIN_SET);
        current_state = STATE_STEP4;

        Send_AT_Command("E2_TEST\r\n");
    }
    else
    {
        StopLedBlink(STATE_STEP3);
        led_initialized = 0;
        current_state = STATE_ERROR;
    }
}

void Process_Step4(void)
{
    //	Send_AT_Command("E2_TEST\r\n");
    static uint8_t led_initialized = 0;
    if (!led_initialized)
    {
        StartStepLedBlink(STATE_STEP4); // G4开始闪烁
        led_initialized = 1;
    }

    if (Wait_PCB1_Response())
    {
        StopLedBlink(STATE_STEP4); // LED_G4停止闪烁
        HAL_GPIO_WritePin(LED_G4_GPIO_Port, LED_G4_Pin, GPIO_PIN_SET);
        current_state = STATE_STEP5;

        Send_AT_Command("RADAR_TEST\r\n");
    }
    else
    {
        StopLedBlink(STATE_STEP4);
        led_initialized = 0;
        current_state = STATE_ERROR;
    }
}

void Process_Step5(void)
{
    static uint8_t led_initialized = 0;
    if (!led_initialized)
    {
        StartStepLedBlink(STATE_STEP5); // G5开始闪烁
        led_initialized = 1;
    }
    //	Send_AT_Command("RADAR_TEST\r\n");
    if (Wait_PCB1_Response())
    {
        StopLedBlink(STATE_STEP5); // LED_G5停止闪烁
        HAL_GPIO_WritePin(LED_G5_GPIO_Port, LED_G5_Pin, GPIO_PIN_SET);
        current_state = STATE_STEP6;
    }
    else
    {
        StopLedBlink(STATE_STEP5);
        led_initialized = 0;
        current_state = STATE_ERROR;
    }
}

void Handle_Error(void)
{
    // 关闭所有LED
    for (int i = 1; i <= STATE_STEP6; i++)
    {
        HAL_GPIO_WritePin(step_leds[i].GPIOx, step_leds[i].Pin, GPIO_PIN_RESET);
    }
    // 红灯常亮表示错误
    HAL_GPIO_WritePin(LED_R_GPIO_Port, LED_R_Pin, GPIO_PIN_SET);

    // 复位UART模块
    HAL_UART_DeInit(&huart1);
    MX_USART1_UART_Init();
    HAL_UART_Receive_IT(&huart1, rx_buf, RX_BUF_SIZE);
    __HAL_UART_ENABLE_IT(&huart1, UART_IT_IDLE);

    // 重置状态机
    current_state = STATE_IDLE;
    step1_substate = SUBSTEP_IDLE;
}

// 发送AT指令
void Send_AT_Command(volatile const char *cmd)
{
    memset(rx_buf, 0, RX_BUF_SIZE);
    rx_index = 0;
    uart_idle_flag = 0;
    HAL_UART_Transmit(&huart1, (uint8_t *)cmd, strlen((const char *)cmd), 1000);
    HAL_UART_Receive_IT(&huart1, rx_buf, RX_BUF_SIZE);
    __HAL_UART_ENABLE_IT(&huart1, UART_IT_IDLE); // 重新使能IDLE中断
}

uint8_t Wait_PCB1_Response(void)
{
    uint8_t rx_buf[64];
    if (HAL_UART_Receive(&huart1, rx_buf, sizeof(rx_buf), 2000) == HAL_OK)
    {
        if (strstr((char *)rx_buf, "PASS\r\n"))
        {
            return 1;
        }
        else if (strstr((char *)rx_buf, "FAIL\r\n"))
        {
            return 0;
        }
    }
    return 0;
}

void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    /** Initializes the RCC Oscillators according to the specified parameters
     * in the RCC_OscInitTypeDef structure.
     */
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState = RCC_HSE_ON;
    RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
    RCC_OscInitStruct.HSIState = RCC_HSI_ON;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
    {
        Error_Handler();
    }

    /** Initializes the CPU, AHB and APB buses clocks
     */
    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
    {
        Error_Handler();
    }
}

void Control_Start(void)
{
    HAL_GPIO_WritePin(TEST_BT_1_GPIO_Port, TEST_BT_1_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(TEST_BT_2_GPIO_Port, TEST_BT_2_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(TEST_BT_3_GPIO_Port, TEST_BT_3_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(TEST_BT_4_GPIO_Port, TEST_BT_4_Pin, GPIO_PIN_RESET);
}

void Control_Relay_PCB1(void)
{
    HAL_GPIO_WritePin(TEST_BT_3_GPIO_Port, TEST_BT_3_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(TEST_BT_4_GPIO_Port, TEST_BT_4_Pin, GPIO_PIN_RESET);
    HAL_Delay(200);
    HAL_GPIO_WritePin(TEST_BT_1_GPIO_Port, TEST_BT_1_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(TEST_BT_2_GPIO_Port, TEST_BT_2_Pin, GPIO_PIN_SET);
}

void Control_Relay_PCB2(void)
{
    HAL_GPIO_WritePin(TEST_BT_1_GPIO_Port, TEST_BT_1_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(TEST_BT_2_GPIO_Port, TEST_BT_2_Pin, GPIO_PIN_RESET);
    HAL_Delay(200);
    HAL_GPIO_WritePin(TEST_BT_3_GPIO_Port, TEST_BT_3_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(TEST_BT_4_GPIO_Port, TEST_BT_4_Pin, GPIO_PIN_SET);
}

void Error_Handler(void)
{
    /* USER CODE BEGIN Error_Handler_Debug */
    /* User can add his own implementation to report the HAL error return state */
    __disable_irq();
    while (1)
    {
    }
    /* USER CODE END Error_Handler_Debug */
}

#ifdef USE_FULL_ASSERT
/**
 * @brief  Reports the name of the source file and the source line number
 *         where the assert_param error has occurred.
 * @param  file: pointer to the source file name
 * @param  line: assert_param error line source number
 * @retval None
 */
void assert_failed(uint8_t *file, uint32_t line)
{
    /* USER CODE BEGIN 6 */
    /* User can add his own implementation to report the file name and line number,
       ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
    /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
