/* ============================================================
   K98协控固件 最终版: 屏幕内置图片/状态页 + 98灯RGB(绫波丽蓝+按键水波纹)
   换图: 用tools/img2h.py重新生成images.h后重新上传本固件
   ============================================================ */
#include <SPI.h>
#include <Adafruit_NeoPixel.h>
#include <EEPROM.h>
#include "images.h"        /* img2h.py生成: NIMG, IMGS[] */

#define P_SCK   18
#define P_MOSI  19
#define P_CS_L  20
#define P_DC    21
#define P_RST   22
#define P_BL    15
#define P_RGB   14
#define P_BOOST 13
#define P_CHG   12
#define P_RX1    1
#define P_LED   25

#define NLED     98
#define IMG_W    280
#define IMG_H    240
#define IMG_BYTES ((uint32_t)IMG_W * IMG_H * 2)
#define TFT_OFF  20      /* 画面偏移就在20/0之间试 */
#define MADCTL_V 0x60    /* 画面镜像/倒转依次试 0xA0/0xC0/0x20 */

Adafruit_NeoPixel strip(NLED, P_RGB, NEO_GRB + NEO_KHZ800);

/* ---------- 配置(EEPROM断电保存) ---------- */
struct Cfg { uint32_t magic; uint8_t bright, mode, page, img, albumSec, albumOn, screenOn; };
Cfg cfg = { 0x39384B4BUL, 30, 1, 0, 0, 8, 1, 1 };
void loadCfg(){ EEPROM.begin(64); Cfg c; EEPROM.get(0,c);
  if(c.magic==cfg.magic && c.bright<=100 && c.mode<6 && (NIMG==0 || c.img<NIMG)) cfg=c;
  EEPROM.end(); }
void saveCfg(){ EEPROM.begin(64); EEPROM.put(0,cfg); EEPROM.end(); }

/* ---------- 状态 ---------- */
int usbMode=0, profile=0;
uint32_t lastAct=0, lastAlbum=0;
bool slept=false, boostOn=false; uint32_t boostAt=0;

/* ---------- 电池(10M/2.2M分压) ---------- */
uint32_t readVbat(){ analogRead(A0); long s=0;
  for(int i=0;i<16;i++) s+=analogRead(A0);
  return (uint32_t)s*3300UL*122UL/4096UL/22UL/16UL; }
int batPct(){ long v=readVbat();
  return v<3500?0 : v>4150?100 : (int)((v-3500)*100/650); }

/* ---------- TFT ST7789 横屏 ---------- */
void tCmd(uint8_t c){ digitalWrite(P_CS_L,LOW); digitalWrite(P_DC,LOW);
  SPI.transfer(c); digitalWrite(P_CS_L,HIGH); }
void tDat(uint8_t c){ digitalWrite(P_CS_L,LOW); digitalWrite(P_DC,HIGH);
  SPI.transfer(c); digitalWrite(P_CS_L,HIGH); }
void tWin(int x,int y,int w,int h){
  tCmd(0x2A); tDat((TFT_OFF+x)>>8); tDat((TFT_OFF+x)&0xFF);
  tDat((TFT_OFF+x+w-1)>>8); tDat((TFT_OFF+x+w-1)&0xFF);
  tCmd(0x2B); tDat(y>>8); tDat(y&0xFF); tDat((y+h-1)>>8); tDat((y+h-1)&0xFF);
  tCmd(0x2C); }
const uint8_t G0[14]={0xD0,0x08,0x11,0x08,0x0C,0x15,0x39,0x33,0x50,0x36,0x13,0x14,0x29,0x2D};
const uint8_t G1[14]={0xD0,0x08,0x10,0x08,0x06,0x06,0x39,0x44,0x50,0x5C,0x32,0x3F,0x3F,0x28};
void tInit(){ digitalWrite(P_RST,LOW); delay(50); digitalWrite(P_RST,HIGH); delay(150);
  tCmd(0x11); delay(120);
  tCmd(0x36); tDat(MADCTL_V);
  tCmd(0x3A); tDat(0x55);
  tCmd(0x21);                    /* IPS反色; 颜色反了就删这行 */
  tCmd(0xB2); tDat(0x0C);tDat(0x0C);tDat(0x00);tDat(0x33);tDat(0x33);
  tCmd(0xB7); tDat(0x35);
  tCmd(0xBB); tDat(0x1F);
  tCmd(0xD0); tDat(0xA4); tDat(0xA1);
  tCmd(0xE0); for(int i=0;i<14;i++) tDat(G0[i]);
  tCmd(0xE1); for(int i=0;i<14;i++) tDat(G1[i]);
  tCmd(0x29); analogWrite(P_BL,255); }
void rect(int x,int y,int w,int h,uint16_t c){ tWin(x,y,w,h);
  digitalWrite(P_CS_L,LOW); digitalWrite(P_DC,HIGH);
  uint8_t b[64]; for(int i=0;i<64;i+=2){ b[i]=c>>8; b[i+1]=c&0xFF; }
  long n=(long)w*h*2;
  for(long k=0;k<n;k+=64) SPI.transfer(b,(int)min(64L,n-k));
  digitalWrite(P_CS_L,HIGH); }
void tFill(uint16_t c){ rect(0,0,IMG_W,IMG_H,c); }

/* ---------- 七段数字/进度条 ---------- */
const uint8_t SEG7[10]={0x3F,0x06,0x5B,0x4F,0x66,0x6D,0x7D,0x07,0x7F,0x6F};
void digit(int x,int y,int h,int d,uint16_t c){
  int w=h/2, t=h/12+1, u=h/3; uint8_t s=SEG7[d&15];
  if(s&1)  rect(x, y, w, t, c);
  if(s&2)  rect(x+w-t, y, t, u, c);
  if(s&4)  rect(x+w-t, y+u, t, u, c);
  if(s&8)  rect(x, y+2*u-t, w, t, c);
  if(s&16) rect(x, y+u, t, u, c);
  if(s&32) rect(x, y, t, u, c);
  if(s&64) rect(x, y+u-t/2, w, t, c); }
void bar(int x,int y,int w,int h,int pct,uint16_t c){
  rect(x,y,w,h,0x2104); rect(x+2,y+2,(w-4)*pct/100,h-4,c); }

/* ---------- 显示 ---------- */
void showStatus(){
  tFill(0x0000);
  int p=batPct(); bool chg=(digitalRead(P_CHG)==LOW);
  int d3=p/100, x0=d3?64:92;
  if(d3) digit(x0,16,44,d3,0x07FF);
  digit(x0+(d3?52:0),16,44,(p/10)%10,0x07FF);
  digit(x0+(d3?104:52),16,44,p%10,0x07FF);
  bar(30,84,220,22,p, chg?0x07E0:0x07FF);
  if(usbMode) rect(90,130,100,14,0x07FF);
  else{ digit(132,122,36,profile+1,0xF81F); rect(112,170,56,10,0xF81F); }
  bar(30,198,220,16,cfg.bright,0xF81F); }
void showImg(int i){
  if(NIMG==0){ tFill(0x0000); return; }
  if(i<0) i=0; if(i>=NIMG) i=NIMG-1;
  cfg.img=i;
  tWin(0,0,IMG_W,IMG_H);
  const uint8_t* p=(const uint8_t*)IMGS[i];
  digitalWrite(P_CS_L,LOW); digitalWrite(P_DC,HIGH);
  static uint8_t buf[2048];
  for(uint32_t o=0;o<IMG_BYTES;){
    uint32_t n=IMG_BYTES-o; if(n>sizeof(buf)) n=sizeof(buf);
    memcpy(buf,p+o,n); SPI.transfer(buf,(int)n); o+=n; }
  digitalWrite(P_CS_L,HIGH); }
void nextImg(int d){ if(NIMG<2) return;
  cfg.img=(cfg.img+d+NIMG)%NIMG; showImg(cfg.img); saveCfg(); }
void refresh(){ if(cfg.page) showStatus(); else showImg(cfg.img); }
void screenOff(){ tCmd(0x10); analogWrite(P_BL,0); }
void screenOn(){ tCmd(0x11); delay(120); analogWrite(P_BL,255); refresh(); }

/* ---------- RGB (0流水蓝 1流动 2呼吸 3涟漪 4常亮 5关) 固定绫波丽蓝 ---------- */
/* 绫波丽蓝: RGB(120,200,255) 冰蓝. 波纹: 按键水波从按键位置扩散叠加在所有亮灯模式上 */
#define REI_R 120
#define REI_G 200
#define REI_B 255
#define NWAVES 10            /* 同时存在的波数上限 */
#define WAVE_LIFE 2040UL     /* 波寿命ms(之后丢弃) */
#define RIPPLE_SPREAD 8      /* 波速: age>>3 = 半径mm (≈125mm/s) */
#define RIPPLE_SIG 8         /* 波前高斯半宽σ mm */
static const int16_t LED_XY[NLED][2] = {   /* 灯物理坐标mm (k98_data 生成) */
  {14,22},{14,41},{16,117},{18,60},{21,79},{25,98},{33,41},
  {40,117},{42,60},{47,79},{52,22},{52,41},{56,98},{61,60},
  {64,117},{71,22},{66,79},{71,41},{75,98},{80,60},{85,79},
  {90,22},{90,41},{94,98},{99,60},{104,79},{109,22},{109,41},
  {114,98},{118,60},{123,79},{128,41},{133,98},{135,117},{137,22},
  {142,79},{147,41},{137,60},{152,98},{156,22},{161,79},{166,41},
  {156,60},{171,98},{175,22},{180,79},{185,41},{175,60},{190,98},
  {194,22},{199,79},{204,41},{206,117},{194,60},{209,98},{223,22},
  {214,60},{218,79},{223,41},{228,98},{242,22},{230,117},{233,60},
  {237,79},{261,22},{242,41},{252,60},{254,117},{280,22},{264,98},
  {275,60},{271,41},{278,117},{268,79},{304,117},{309,60},{323,98},
  {323,117},{328,60},{342,117},{347,60},{375,41},{375,60},{375,79},
  {375,98},{385,117},{395,41},{395,60},{395,79},{395,98},{414,41},
  {414,60},{414,79},{414,98},{414,117},{433,41},{433,69},{433,107},
};
/* 定点查找表: EXP[i]=255*exp(-i/2) 波前高斯; EXP2[i]=255*exp(-i/112.5) 时间衰减 */
const uint8_t EXP[256] = {
  255,155,94,57,35,21,13,8,5,3,2,1,1,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
const uint8_t EXP2[256] = {
  255,253,251,248,246,244,242,240,237,235,233,231,229,227,225,223,
  221,219,217,215,213,212,210,208,206,204,202,201,199,197,195,194,
  192,190,188,187,185,184,182,180,179,177,176,174,172,171,169,168,
  166,165,164,162,161,159,158,156,155,154,152,151,150,148,147,146,
  144,143,142,141,139,138,137,136,134,133,132,131,130,129,127,126,
  125,124,123,122,121,120,119,118,117,116,115,114,113,112,111,110,
  109,108,107,106,105,104,103,102,101,100,99,99,98,97,96,95,
  94,93,93,92,91,90,89,89,88,87,86,85,85,84,83,82,
  82,81,80,80,79,78,77,77,76,75,75,74,73,73,72,72,
  71,70,70,69,68,68,67,67,66,65,65,64,64,63,63,62,
  62,61,60,60,59,59,58,58,57,57,56,56,55,55,54,54,
  53,53,52,52,51,51,51,50,50,49,49,48,48,48,47,47,
  46,46,45,45,45,44,44,43,43,43,42,42,42,41,41,40,
  40,40,39,39,39,38,38,38,37,37,37,36,36,36,35,35,
  35,35,34,34,34,33,33,33,32,32,32,32,31,31,31,30,
  30,30,30,29,29,29,29,28,28,28,28,27,27,27,27,26};
int posToLed(int pos){ if(pos==76) return -1;      /* 76=旋钮位,无灯 */
  return (pos<76)?pos:pos-1; }
/* 波槽: (x, y, t0) */
int16_t wav_x[NWAVES]; int16_t wav_y[NWAVES]; uint32_t wav_t0[NWAVES];
uint8_t wav_n = 0, wav_head = 0;
void ripplePush(uint8_t led){                   /* 按键按下: 在灯位置激发水波 */
  wav_x[wav_head] = LED_XY[led][0]; wav_y[wav_head] = LED_XY[led][1];
  wav_t0[wav_head] = millis(); wav_head = (wav_head+1)%NWAVES;
  if(wav_n < NWAVES) wav_n++; }
static uint32_t isqrt32(uint32_t v){            /* v<=210000 (对角线距离平方) */
  uint32_t x = v, y = (x+1)>>1;
  while(y < x){ x = y; y = (x + v/x)>>1; }
  return x; }
/* 波纹幅值 0..255 (定点: 环带裁剪+查表, 98灯×10波 <0.5ms) */
uint8_t rippleAmp(uint8_t led, uint32_t t){
  uint32_t a = 0;
  for(uint8_t w = 0; w < wav_n; w++){
    uint32_t age = t - wav_t0[w];
    if(age > WAVE_LIFE) continue;
    uint16_t rad = age >> 3;                    /* 波前半径mm = age/8 (125mm/s) */
    int32_t dx = LED_XY[led][0] - wav_x[w], dy = LED_XY[led][1] - wav_y[w];
    uint32_t g2 = (uint32_t)(dx*dx + dy*dy);
    uint32_t lo = (rad > 40) ? (uint32_t)(rad-40)*(rad-40) : 0;
    uint32_t hi = (uint32_t)(rad+40)*(rad+40);
    if(g2 > hi || g2 < lo) continue;            /* 波前±40mm环带外幅值≈0, 裁剪 */
    uint16_t d = (uint16_t)isqrt32(g2);
    uint16_t gd = (d > rad) ? d - rad : rad - d;
    uint32_t band = EXP[min(255, (gd*gd) >> 6)];       /* exp(-gd²/128), σ=8mm */
    uint32_t fade = EXP2[min(255, age >> 3)];          /* exp(-age/900ms) */
    a += (band * fade) >> 8;
    if(a >= 255) return 255; }
  return (uint8_t)a; }
void rgbTick(uint32_t dt){
  static uint32_t t=0; t+=dt;
  if(cfg.mode==5 || cfg.bright==0){
    for(int i=0;i<NLED;i++) strip.setPixelColor(i,0);
    strip.show(); digitalWrite(P_BOOST,LOW); boostOn=false; return; }
  if(!boostOn){ digitalWrite(P_BOOST,HIGH); boostAt=millis(); boostOn=true; }
  if(millis()-boostAt<50) return;
  float f=cfg.bright/100.0;
  /* 限幅: 1.5A保险丝极限. 绫波丽蓝三色常开, 常亮25%其余50%; 涟漪波提亮同样受f限幅 */
  if(cfg.mode!=3){ float cap=(cfg.mode==4)?0.25f:0.5f; if(f>cap) f=cap; }
  for(int i=0;i<NLED;i++){
    /* 底色: 全部染成绫波丽蓝色系 */
    uint8_t bl;
    switch(cfg.mode){
      case 0: bl = 90 + (uint8_t)(90*sinf(i*0.35f)); break;                  /* 静态明暗蓝 */
      case 1: bl = 90 + (uint8_t)(90*sinf(i*0.35f - t*0.004f)); break;       /* 流动 */
      case 2: bl = (uint8_t)(160*(0.5+0.5*sinf(t/600.f))); break;            /* 呼吸 */
      case 3: bl = 0; break;                                                  /* 纯涟漪 */
      case 4: bl = 255; break;                                                /* 常亮蓝 */
      default: bl = 0; }
    uint8_t amp = rippleAmp(i, t);
    uint16_t v = (uint16_t)min(255UL, ((uint32_t)bl + amp) * (uint32_t)(f*255) / 255);
    strip.setPixelColor(i, strip.Color(REI_R*v/255, REI_G*v/255, REI_B*v/255)); }
  strip.show(); }

/* ---------- 主控UART: K=按键 C=命令 S=状态 ---------- */
String uline="";
void pollUart(){
  while(Serial1.available()){
    char c=Serial1.read();
    if(c=='\n'){
      bool act = uline.startsWith("K,") || uline.startsWith("C,");
      if(act){ lastAct=millis();
        if(slept && cfg.screenOn){ slept=false; screenOn(); } }
      if(uline.startsWith("S,")){ int a,b;
        sscanf(uline.c_str(),"S,%d,%d",&a,&b); usbMode=a; profile=b;
        if(cfg.page && cfg.screenOn && !slept) showStatus(); }
      else if(uline.startsWith("K,")){ int a,b;
        sscanf(uline.c_str(),"K,%d,%d",&a,&b);
        if(b){ int led=posToLed(a); if(led>=0) ripplePush(led); } }   /* 按键激发水波(所有亮灯模式) */
      else if(uline.startsWith("C,")){
        int cmdv=uline.substring(2).toInt();
        switch(cmdv){
          case 68: cfg.bright=(cfg.bright>=10)?cfg.bright-10:0; saveCfg(); break;
          case 69: cfg.bright=(cfg.bright<=90)?cfg.bright+10:100; saveCfg(); break;
          case 70: cfg.mode=(cfg.mode+1)%6; saveCfg(); break;
          case 76: cfg.mode=(cfg.mode+5)%6; saveCfg(); break;
          case 71: cfg.screenOn=!cfg.screenOn; saveCfg();
                   if(cfg.screenOn){ slept=false; screenOn(); }
                   else { screenOff(); slept=true; } break;
          case 72: cfg.page^=1; saveCfg(); if(cfg.screenOn&&!slept) refresh(); break;
          case 73: nextImg(-1); break;
          case 74: nextImg(1); break;
          case 75: cfg.albumOn=!cfg.albumOn; saveCfg(); break;
        }
        if(((cmdv>=68&&cmdv<=70)||cmdv==76) && cfg.page && cfg.screenOn && !slept)
          showStatus(); }
      uline=""; }
    else if(c!='\r' && uline.length()<32) uline+=c; } }

/* ---------- 主程序 ---------- */
void setup(){
  pinMode(P_CS_L,OUTPUT); pinMode(P_DC,OUTPUT); pinMode(P_RST,OUTPUT);
  pinMode(P_BOOST,OUTPUT); pinMode(P_CHG,INPUT_PULLUP);
  pinMode(P_LED,OUTPUT);
  digitalWrite(P_BOOST,LOW); digitalWrite(P_LED,HIGH);
  Serial.begin(115200);
  Serial1.setRX(P_RX1); Serial1.begin(115200);
  SPI.setTX(P_MOSI); SPI.setSCK(P_SCK); SPI.begin();
  SPI.beginTransaction(SPISettings(40000000,MSBFIRST,SPI_MODE0));
  strip.begin(); strip.show();
  tInit(); loadCfg();
  Serial.printf("[K98] NIMG=%d bright=%d mode=%d\n",NIMG,cfg.bright,cfg.mode);
  if(cfg.screenOn) refresh(); else { screenOff(); slept=true; }
  digitalWrite(P_LED,LOW); }
void loop(){
  static uint32_t prev=0; uint32_t now=millis();
  uint32_t dt=now-prev; prev=now;
  pollUart(); rgbTick(dt);
  if(cfg.screenOn){
    if(!slept && now-lastAct>180000UL){ slept=true; screenOff(); }   /* 3分钟息屏 */
    if(cfg.albumOn && !cfg.page && !slept &&
       now-lastAlbum>(uint32_t)cfg.albumSec*1000UL){ lastAlbum=now; nextImg(1); } }
  else if(!slept){ slept=true; screenOff(); }
  delay(3); }