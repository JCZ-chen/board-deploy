
/* dual485.c — 双口判定: ttymxc1(DE翻转)发送, ttymxc2(纯接收)监听
 * 判定: 4852收到4851的帧 = 板子发送链路+DE控制正常, 问题在设备/接线
 * 用法: dual485 <发送口> <监听口> <baud> <addr> <oi_hex> <de_gpio>
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

static int open_raw(const char*dev,int baud){
    int fd=open(dev,O_RDWR|O_NOCTTY); if(fd<0){perror(dev);return -1;}
    struct termios t;memset(&t,0,sizeof(t));
    speed_t sp=B9600;switch(baud){case 2400:sp=B2400;break;case 4800:sp=B4800;break;case 19200:sp=B19200;break;case 38400:sp=B38400;break;case 115200:sp=B115200;break;default:sp=B9600;}
    cfsetispeed(&t,sp);cfsetospeed(&t,sp);
    t.c_cflag=CS8|CREAD|CLOCAL;t.c_cflag&=~(PARENB|CSTOPB|CSIZE);t.c_cflag|=CS8;
    t.c_iflag=0;t.c_oflag=0;t.c_lflag=0;t.c_cc[VMIN]=0;t.c_cc[VTIME]=0;
    tcsetattr(fd,TCSANOW,&t);tcflush(fd,TCIOFLUSH);
    return fd;
}

int main(int argc,char**argv){
    if(argc<7){fprintf(stderr,"用法: %s <发送口> <监听口> <baud> <addr> <oi_hex> <de_gpio>\n",argv[0]);return 1;}
    int baud=atoi(argv[3]); uint8_t addr=(uint8_t)strtol(argv[4],NULL,10);
    uint16_t oi=(uint16_t)strtol(argv[5],NULL,16); int gpio=atoi(argv[6]);
    deInit(gpio);
    int fdt=open_raw(argv[1],baud); if(fdt<0)return 1;
    int fdr=open_raw(argv[2],baud); if(fdr<0)return 1;

    uint8_t f[16];int p=0;
    f[p++]=addr;f[p++]=0x66;f[p++]=0x03;f[p++]=0x01;
    f[p++]=(uint8_t)(oi>>8);f[p++]=(uint8_t)(oi&0xFF);
    uint16_t c=crc16(f,p);f[p++]=(uint8_t)(c&0xFF);f[p++]=(uint8_t)(c>>8);
    printf("发送口=%s 监听口=%s baud=%d gpio=%d\n",argv[1],argv[2],baud,gpio);

    for(int n=0;n<3;n++){
        printf("--- 第%d帧 ---\n",n+1);
        deSet(1);usleep(10000);
        write(fdt,f,p);tcdrain(fdt);usleep(10000);
        deSet(0);
        /* 监听口收 800ms */
        uint8_t rx[128];int rc=0;
        for(int i=0;i<10;i++){struct pollfd pf;pf.fd=fdr;pf.events=POLLIN;pf.revents=0;
            int r=poll(&pf,1,80);
            if(r>0&&(pf.revents&POLLIN)){int m=read(fdr,rx+rc,sizeof(rx)-rc);if(m>0)rc+=m;}
        }
        if(rc>0){printf("监听口收到(%d字节):",rc);for(int i=0;i<rc;i++)printf(" %02X",rx[i]);printf("\n");
            uint16_t cc=crc16(rx,rc>=2?rc-2:rc); if(rc>=8)printf("CRC校验:%s\n",(cc==(uint16_t)(rx[rc-2]|(rx[rc-1]<<8)))?"OK":"MISMATCH");
            close(fdt);close(fdr);return 0;}
        printf("监听口未收到\n");
        usleep(300000);
    }
    printf("\n结论: 4851发出的数据, 4852完全听不到 => 发送链路(DE/收发器)有问题\n");
    close(fdt);close(fdr);return 2;
}
