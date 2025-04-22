bool isNumber(char* token){
    return strlen(token)>1||('0'<=token[0]&&token[0]<='9');
}
int evalRPN(char** tokens, int tokensSize) {
    int token[tokensSize];
    int top=-1;
    for(int i=0;i<tokensSize;i++){
        char *arr=tokens[i];
        if(isNumber(arr)){
            token[++top]=atoi(arr);
        }
        else{
            int num2=token[top--];
            int num1=token[top--];
            switch(arr[0]){
                case '+':
                    token[++top]=num1+num2;
                    break;
                case '-':
                    token[++top]=num1-num2;
                    break;
                case '*':
                    token[++top]=num1*num2;
                    break;
                case '/':
                    token[++top]=num1/num2;
                    break;

            }
        }
    }
    return token[top];
}