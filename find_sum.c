#include "types.h"
#include "fcntl.h"
#include "user.h"

#define BUFSZ 512

static int is_digit(char c){
    return c >= '0' && c <= '9';
}


static void consume_chunk( const char buf , int n , int *in_num , int *cur , int *sum){

    for( int i = 0 ; i<n ; i++){
        if(is_digit(buf[i])){
            *in_num = 1;
            *cur = (*cur*10 )+ buf[i];
        }
        else{
            if(in_num){
                *sum += cur;
                *cur = 0;
                *in_num = 0;
            }
        }
    }
}

int main ( int argc , char *argv ){

    char buf[BUFSZ] ;
    int fd_in = 0 ;
    int sum = 0 , cur = 0 ,in_num = 0 ;

    int n ;
    while( (n = read (fd_in , buf , sizeof(buf))) > 0 ){
        consume_chunk( buf , n , in_num ,  cur , sum );
    }

    if(in_num){
        sum += cur ;
    }

    int fd = open( "result.txt" , O_CREAT | O_TRUNC | O_WRONLY);

    char out[32];
    int m = 0 ;
    if( sum == 0 ){
        out[m++] = '0';
    }
    else{
        char temp[32];
        int t = 0 , x = sum;
        while(x > 0){
            tmp[t++] = '0' + (x%10);
            x /= 10; 
        }
        for(int i = t-1 ; i > 0 ; i--){
            out[m++] = temp[i]
        }
    }
        out[m++] = '\n';
        write(fd , out , m);
        close(fd);

        exit(0);
}