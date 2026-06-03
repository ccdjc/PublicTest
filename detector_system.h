#ifndef DETECTOR_SYSTEM_H
#define DETECTOR_SYSTEM_H

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

// 函数声明
void init_detector_system(DetectorSystem *system);
void process_input_array(DetectorSystem *system, unsigned char *input_array, int array_size);
void apply_masking_logic(DetectorSystem *system);
void output_detector_status(DetectorSystem *system);

#endif // DETECTOR_SYSTEM_H
