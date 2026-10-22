#include <stdio.h>
#include <libusb.h>
int main(){ libusb_context* c; libusb_init(&c); libusb_device** l; ssize_t n=libusb_get_device_list(c,&l);
 for(ssize_t i=0;i<n;i++){ libusb_device_descriptor d; libusb_get_device_descriptor(l[i],&d); if(d.idVendor!=0x045e||d.idProduct!=0x02d8) continue;
  libusb_config_descriptor* cf; if(libusb_get_active_config_descriptor(l[i],&cf)) libusb_get_config_descriptor(l[i],0,&cf);
  printf("cfg ifaces=%d extra=%d\n",cf->bNumInterfaces,cf->extra_length);
  for(int a=0;a<cf->extra_length;a++) printf("%02X ",cf->extra[a]); printf("\n");
  for(int k=0;k<cf->bNumInterfaces;k++) for(int s=0;s<cf->interface[k].num_altsetting;s++){ const libusb_interface_descriptor* id=&cf->interface[k].altsetting[s];
   printf("IF %d alt %d class %d sub %d proto %d eps %d extra:",id->bInterfaceNumber,id->bAlternateSetting,id->bInterfaceClass,id->bInterfaceSubClass,id->bInterfaceProtocol,id->bNumEndpoints);
   for(int a=0;a<id->extra_length;a++) printf(" %02X",id->extra[a]); printf("\n");
   for(int e=0;e<id->bNumEndpoints;e++){ const libusb_endpoint_descriptor* ep=&id->endpoint[e]; printf("  EP %02X attr %02X maxpkt %d interval %d extra:",ep->bEndpointAddress,ep->bmAttributes,ep->wMaxPacketSize,ep->bInterval); for(int a=0;a<ep->extra_length;a++) printf(" %02X",ep->extra[a]); printf("\n"); } }
 } return 0; }
