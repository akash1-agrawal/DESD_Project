#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

int main() {
	int i, fd,choice;
	char read_buf[32],write_buf[2];
	fd = open("/dev/usb_cdc1", O_RDWR);
	if(fd < 0) {
		perror("open() failed");
		_exit(1);
	}
	
	do{
		printf("1.LDR\n2.LM35\n3.Blink LEDs\n0.Exit\nEnter your choice : ");
		scanf("%d",&choice);
		switch(choice){
			case 1:
				strcpy(write_buf,"1");
				write(fd,write_buf,1);
				read(fd,read_buf,32);
				printf("Value of LDR = %s\n",read_buf);
				break;
			case 2:
				strcpy(write_buf,"2");
				write(fd,write_buf,1);
				read(fd,read_buf,32);
				printf("Value of LM35 = %s\n",read_buf);
				break;
			case 3:
				strcpy(write_buf,"2");
				write(fd,write_buf,1);
				printf("Blinking LED check on device\n");
				break;
			default :
				strcpy(write_buf,"3");
				write(fd,write_buf,1);
				break;
		}
	}while(choice != 0);

	close(fd);
	return 0;
}

