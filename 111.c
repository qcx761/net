#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>


int main(){
    

    return 0;
}


#define MIN(a, b) ((b) < (a) ? (b) : (a))

int minSizeSubarray(int* nums, int numsSize, int target) {
    long long total = 0;
    for (int i = 0; i < numsSize; i++) {
        total += nums[i];
    }

    int n = numsSize;
    int ans = INT_MAX;
    long long sum = 0;
    int left = 0;
    for (int right = 0; right < n * 2; right++) {
        sum += nums[right % n];
        while (sum > target % total) {
            sum -= nums[left % n];
            left++;
        }
        if (sum == target % total) {
            ans = MIN(ans, right - left + 1);
        }
    }

    return ans == INT_MAX ? -1 : ans + target / total * n;
}
