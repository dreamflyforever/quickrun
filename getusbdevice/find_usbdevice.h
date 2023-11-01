#ifndef __FIND_USBDEVICE_H_
#define __FIND_USBDEVICE_H_

typedef enum device_type_enum{
        ttyUSB = 0,
        video = 1,
	pcm
}device_type;

int get_usbdevname(char *pid, char *vid, device_type devtype, char *name);
int check_usbdev(char *pid, char *vid);

#endif
