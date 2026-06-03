#include <stdio.h>
void sortColors(int *nums, int numsSize)
{
    int red = 0, white = 0;
    // nums = (int *)malloc(numsSize * sizeof(int));
    // numsSize = sizeof(nums) / sizeof(nums[0]);
    // 先找出有多少个红、白、蓝
    for (int i = 0; i < numsSize; i++)
    {
        if (nums[i] == 0)
        {
            red++;
        }
        if (nums[i] == 1)
        {
            white++;
        }
    }
    for (int i = 0; i < red; i++)
    {
        nums[i] = 0;
    }
    for (int i = red; i < red + white; i++)
    {
        nums[i] = 1;
    }
    for (int i = red + white; i < numsSize; i++)
    {
        nums[i] = 2;
    }
    for (int i = 0; i < numsSize; i++)
    {
        printf("%d ", nums[i]);
    }
}
int main()
{

    // int i = 0;
    // int USART1_RX_BUF[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 5, 17, 11, 12, 13, 14, 15, 16, 17, 5, 17, 5, 5, 13, 14, 15, 16, 17};
    // printf("%d\n", USART1_RX_BUF[5]);
    // for (int k = 0; k < 8; k++)
    // {
    //     ReceiveList[k] = TempReceiveList[k];
    //     printf("%d\n", ReceiveList[k]);
    // }
    int nums[] = {2, 1, 0};
    int numsSize = sizeof(nums) / sizeof(nums[0]);
    // printf("%d ", numsSize);
    sortColors(nums, numsSize);

    return 0;
}