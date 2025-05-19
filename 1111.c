char* removeStars(char* s) {
    char* res = malloc(strlen(s) + 1);
    int len = 0;
    for (int i = 0; s[i] != '\0'; i++) {
        if (s[i] != '*') {
            res[len++] = s[i];
        } else {
            len--;
        }
    }
    res[len] = '\0';
    return res;
}
int q(const void* a,const void* b){
    return *(int*)a-*(int*)b;
}

char* triangleType(int* nums, int numsSize) {
    qsort(nums,numsSize,sizeof(int),q);
    if (nums[0]+nums[1]<=nums[2]){
        return "none";
    }else if(nums[0]==nums[2]){
        return "equilateral";
    }else if(nums[0]==nums[1]||nums[1]==nums[2]){
        return "isosceles";
    }else{
        return "scalene";
    }
}
