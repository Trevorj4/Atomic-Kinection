// Kinect v2 audio capture over libusb (IF3 alt1, iso EP 0x85, 16 kHz, 4 ch, 32-bit)
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <windows.h>
#include <libusb.h>
static FILE* wav; static long long bytes=0, pkts=0, badpkts=0; static double sq[4]; static long long nsamp=0; static int isfloat=-1;
static void cb(libusb_transfer* t){
  for(int i=0;i<t->num_iso_packets;i++){ auto&p=t->iso_packet_desc[i]; if(p.status!=LIBUSB_TRANSFER_COMPLETED){badpkts++; continue;} pkts++;
    unsigned char* b=libusb_get_iso_packet_buffer_simple(t,i); int n=p.actual_length; if(!n) continue; fwrite(b,1,n,wav); bytes+=n;
    int ns=n/16; for(int s=0;s<ns;s++) for(int c=0;c<4;c++){ float f; memcpy(&f,b+s*16+c*4,4); sq[c]+= (double)f*f; } nsamp+=ns; }
  if(t->status==LIBUSB_TRANSFER_COMPLETED || t->status==LIBUSB_TRANSFER_ERROR) libusb_submit_transfer(t); }
int main(int argc,char**argv){ int secs=argc>1?atoi(argv[1]):10;
  libusb_context* c; libusb_init(&c); auto h=libusb_open_device_with_vid_pid(c,0x045e,0x02d8); if(!h){printf("open failed\n");return 1;}
  int r=libusb_claim_interface(h,3); printf("claim3 %d\n",r); printf("alt0 %d\n",libusb_set_interface_alt_setting(h,3,0)); Sleep(50); r=libusb_set_interface_alt_setting(h,3,1); printf("alt %d\n",r); printf("clearhalt %d\n",libusb_clear_halt(h,0x85)); printf("claim2 %d\n",libusb_claim_interface(h,2)); unsigned char fr[3]={0x80,0x3E,0x00}; r=libusb_control_transfer(h,0x22,0x01,0x0100,0x0085,fr,3,1000); printf("setfreq %d\n",r); unsigned char g[3]={0}; r=libusb_control_transfer(h,0xA2,0x81,0x0100,0x0085,g,3,1000); printf("getfreq %d %02X%02X%02X\n",r,g[2],g[1],g[0]);
  wav=fopen(argc>2?argv[2]:"kaudio.raw","wb"); const int NP=32, NX=8;
  for(int i=0;i<NX;i++){ auto t=libusb_alloc_transfer(NP); auto buf=new unsigned char[256*NP]; libusb_fill_iso_transfer(t,h,0x85,buf,256*NP,NP,cb,nullptr,1000); libusb_set_iso_packet_lengths(t,256); printf("submit %d\n",libusb_submit_transfer(t)); }
  DWORD t0=GetTickCount(); timeval tv{0,100000};
  while(GetTickCount()-t0<(DWORD)secs*1000) libusb_handle_events_timeout(c,&tv);
  printf("RESULT secs=%d bytes=%lld pkts=%lld bad=%lld samples=%lld rate=%.0f Hz\n",secs,bytes,pkts,badpkts,nsamp,nsamp/(double)secs);
  for(int k=0;k<4;k++) printf("ch%d rms(float)=%.6f\n",k,nsamp?sqrt(sq[k]/nsamp):0);
  fclose(wav); return 0; }
