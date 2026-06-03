#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

// 探测器状态枚举
typedef enum
{
    DETECTOR_NORMAL = 0,  // 正常
    DETECTOR_OFFLINE = 1, // 离线
    DETECTOR_ALARM = 2,   // 报警
    DETECTOR_FAULT = 3    // 故障
} DetectorStatus;

// 司机室占用状态
typedef enum
{
    CABIN_NONE = 0, // 无占用
    CABIN_END1 = 1, // 一端司机室占用
    CABIN_END2 = 2  // 二端司机室占用
} CabinStatus;

// 探测器系统结构体
typedef struct
{
    DetectorStatus detectors[32];        // 32个探测器的原始状态
    DetectorStatus output_detectors[32]; // 32个探测器的输出状态（经过屏蔽处理）
    CabinStatus cabin_status;            // 司机室占用状态
} DetectorSystem;

// 状态名称数组，用于输出显示
const char *status_names[] = {
    "正常", "离线", "报警", "故障"};

// 初始化探测器系统
void init_detector_system(DetectorSystem *system)
{
    // 初始化所有探测器为正常状态
    for (int i = 0; i < 32; i++)
    {
        system->detectors[i] = DETECTOR_NORMAL;
        system->output_detectors[i] = DETECTOR_NORMAL;
    }
    system->cabin_status = CABIN_NONE;
}

// 处理接收到的数组数据
void process_input_array(DetectorSystem *system, unsigned char *input_array, int array_size)
{
    if (array_size < 2)
    {
        printf("错误：输入数组长度不足\n");
        return;
    }

    // 解析司机室占用状态（第二位字节，索引为1）
    system->cabin_status = (CabinStatus)input_array[1];

    // 如果数组包含探测器状态数据，更新探测器状态
    // 假设从第3个字节开始是探测器状态数据
    if (array_size >= 34)
    { // 2字节头部 + 32字节探测器状态
        for (int i = 0; i < 32; i++)
        {
            if (input_array[i + 2] <= DETECTOR_FAULT)
            {
                system->detectors[i] = (DetectorStatus)input_array[i + 2];
            }
        }
    }
}

// 应用屏蔽逻辑并生成输出状态
void apply_masking_logic(DetectorSystem *system)
{
    // 首先复制所有原始状态到输出状态
    for (int i = 0; i < 32; i++)
    {
        system->output_detectors[i] = system->detectors[i];
    }

    // 根据司机室占用状态应用屏蔽逻辑
    switch (system->cabin_status)
    {
    case CABIN_END1:
        // 一端司机室占用，屏蔽1号探测器（索引0）
        if (system->detectors[0] == DETECTOR_ALARM)
        {
            system->output_detectors[0] = DETECTOR_NORMAL; // 将报警状态屏蔽为正常
            printf("屏蔽信息：1号探测器报警已被屏蔽（一端司机室占用）\n");
        }
        break;

    case CABIN_END2:
        // 二端司机室占用，屏蔽17号探测器（索引16）
        if (system->detectors[16] == DETECTOR_ALARM)
        {
            system->output_detectors[16] = DETECTOR_NORMAL; // 将报警状态屏蔽为正常
            printf("屏蔽信息：17号探测器报警已被屏蔽（二端司机室占用）\n");
        }
        break;

    case CABIN_NONE:
    default:
        // 无司机室占用，不进行屏蔽
        break;
    }
}

// 输出32个探测器的状态
void output_detector_status(DetectorSystem *system)
{
    printf("\n=== 探测器状态输出 ===\n");
    printf("司机室占用状态: ");
    switch (system->cabin_status)
    {
    case CABIN_NONE:
        printf("无占用\n");
        break;
    case CABIN_END1:
        printf("一端占用\n");
        break;
    case CABIN_END2:
        printf("二端占用\n");
        break;
    default:
        printf("未知状态\n");
        break;
    }

    printf("探测器状态:\n");
    for (int i = 0; i < 32; i++)
    {
        printf("探测器%02d: %s", i + 1, status_names[system->output_detectors[i]]);

        // 如果原始状态和输出状态不同，显示屏蔽信息
        if (system->detectors[i] != system->output_detectors[i])
        {
            printf(" (原状态: %s - 已屏蔽)", status_names[system->detectors[i]]);
        }

        printf("\n");
    }
    printf("========================\n\n");
}

// 模拟生成测试数据
void generate_test_data(unsigned char *test_array, int *array_size)
{
    static int test_cycle = 0;

    *array_size = 34; // 2字节头部 + 32字节探测器状态

    // 第一个字节可以是其他数据
    test_array[0] = 0x55;

    // 第二个字节是司机室占用状态
    test_array[1] = (test_cycle % 3); // 循环测试 0, 1, 2

    // 生成32个探测器的随机状态
    for (int i = 0; i < 32; i++)
    {
        // 大部分时间是正常状态，偶尔有其他状态
        int rand_val = rand() % 100;
        if (rand_val < 80)
        {
            test_array[i + 2] = DETECTOR_NORMAL;
        }
        else if (rand_val < 90)
        {
            test_array[i + 2] = DETECTOR_ALARM;
        }
        else if (rand_val < 95)
        {
            test_array[i + 2] = DETECTOR_OFFLINE;
        }
        else
        {
            test_array[i + 2] = DETECTOR_FAULT;
        }
    }

    // 为了演示屏蔽功能，在特定周期设置特定探测器为报警状态
    if (test_array[1] == CABIN_END1)
    {
        test_array[2] = DETECTOR_ALARM; // 1号探测器报警
    }
    if (test_array[1] == CABIN_END2)
    {
        test_array[18] = DETECTOR_ALARM; // 17号探测器报警
    }

    test_cycle++;
}

int main()
{
    DetectorSystem system;
    unsigned char input_array[64];
    int array_size;
    int cycle_count = 0;

    // 初始化随机数种子
    srand(time(NULL));

    // 初始化探测器系统
    init_detector_system(&system);

    printf("探测器状态管理系统启动\n");
    printf("按 Ctrl+C 退出程序\n\n");

    // 主循环：每秒处理一次数据
    while (1)
    {
        cycle_count++;
        printf("=== 第 %d 秒 ===\n", cycle_count);

        // 模拟接收数组数据（在实际应用中，这里应该是从外部接收数据）
        generate_test_data(input_array, &array_size);

        // 处理输入数组
        process_input_array(&system, input_array, array_size);

        // 应用屏蔽逻辑
        apply_masking_logic(&system);

        // 输出探测器状态
        output_detector_status(&system);

        // 等待1秒
        sleep(1);

        // 为了演示，运行10个周期后退出
        if (cycle_count >= 10)
        {
            printf("演示完成，程序退出。\n");
            break;
        }
    }

    return 0;
}
