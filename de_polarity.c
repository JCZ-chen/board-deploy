
/* de_polarity.c — DE极性/参数扫描测试
 * 用法: de_polarity <串口> <baud> <站号> <OI_hex> <de_gpio> <polarity:0=发前拉高/1=发前拉低>
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <poll.h>
#include <stdint.h>

static int s_deFd=-1;
static void deInit(int num){
    char p[64]; int fd;
    fd=open("/sys/class/gpio/export",O_WRONLY);
    if(fd>=0){char b[8];snprintf(b,8,"%d\n",num);write(fd,b,strlen(b));close(fd);}
    usleep(50000);
    snprintf(p,64,"/sys/class/gpio/gpio%d/direction",num);
    fd=open(p,O_WRONLY); if(fd>=0){write(fd,"out",3);close(fd);}
    snprintf(p,64,"/sys/class/gpio/gpio%d/value",num);
    if(s_deFd>=0)close(s_deFd); s_deFd=open(p,O_WRONLY);
}
static void deSet(int lv){ if(s_deFd>=0){lseek(s_deFd,0,SEEK_SET);write(s_deFd,lv?"1":"0",1);fsync(s_deFd);} }
static uint16_t crc16(const uint8_t*d,int l){uint16_t c=0xFFFF;for(int i=0;i<l;i++){c^=d[i];for(int j=0;j<8;j++){if(c&1)c=(uint16_t)((c>>1)^0xA001);else c>>=1;}}return c;}

int main(int argc,char**argv){
    if(argc<7){fprintf(stderr,"用法: %s <dev> <baud> <addr> <oi_hex> <de_gpio> <polarity>\n",argv[0]);return 1;}
    int baud=atoi(argv[2]); uint8_t addr=(uint8_t)strtol(argv[3],NULL,10);
    uint16_t oi=(uint16_t)strtol(argv[4],NULL,16);
    int gpio=atoi(argv[5]); int pol=atoi(argv[6]);
    deInit(gpio);
    int fd=open(argv[1],O_RDWR|O_NOCTTY); if(fd<0){perror("open");return 1;}
    struct termios t;memset(&t,0,sizeof(t));
    speed_t sp=B9600;switch(baud){case 2400:sp=B2400;break;case 4800:sp=B4800;break;case 19200:sp=B19200;break;case 38400:sp=B38400;break;case 115200:sp=B115200;break;default:sp=B9600;}
    cfsetispeed(&t,sp);cfsetospeed(&t,sp);
    t.c_cflag=CS8|CREAD|CLOCAL;t.c_cflag&=~(PARENB|CSTOPB|CSIZE);t.c_cflag|=CS8;
    t.c_iflag=0;t.c_oflag=0;t.c_lflag=0;t.c_cc[VMIN]=0;t.c_cc[VTIME]=0;
    tcsetattr(fd,TCSANOW,&t);tcflush(fd,TCIOFLUSH);
    /* pol=0: 发送=高; pol=1: 发送=低 */
    int txLv = pol?0:1;
    uint8_t f[16];int p=0;
    f[p++]=addr;f[p++]=0x66;f[p++]=0x03;f[p++]=0x01;
    f[p++]=(uint8_t)(oi>>8);f[p++]=(uint8_t)(oi&0xFF);
    uint16_t c=crc16(f,p);f[p++]=(uint8_t)(c&0xFF);f[p++]=(uint8_t)(c>>8);
    printf("baud=%d addr=%u oi=0x%04X gpio=%d 发送态=%s\n",baud,addr,oi,gpio,pol?"低":"高");
    /* 连发3帧, 每帧独立DE翻转 */
    for(int n=0;n<3;n++){
        deSet(txLv);usleep(10000);
        write(fd,f,p);tcdrain(fd);usleep(10000);
        deSet(!txLv);usleep(10000);
        /* 短收 */
        uint8_t rx[128];int rc=0;
        for(int i=0;i<8;i++){struct pollfd pf;pf.fd=fd;pf.events=POLLIN;pf.revents=0;
            int r=poll(&pf,1,60);
            if(r>0&&(pf.revents&POLLIN)){int m=read(fd,rx+rc,sizeof(rx)-rc);if(m>0)rc+=m;}
        }
        if(rc>0){
            printf("第%d帧 RX(%d):",n+1,rc);
            for(int i=0;i<rc;i++)printf(" %02X",rx[i]);printf("\n");
            close(fd);return 0;
        }
        printf("第%d帧无应答\n",n+1);
    }
    close(fd);return 2;
}
