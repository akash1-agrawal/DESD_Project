//HEADERS
#include<linux/usb.h> //provides necessary definitons and functions for working with usb devices in linux
//used to declare "struct usb_device" and "struct usb_interface" and "struct usb_driver","struct usb_anchor"
//functions like usb register and usb deregister are also in this header file 
#include<linux/completion.h>//definitions and functions for working with completion events in linux
//used to declare the "DECLARE_COMPLETION()" macro,it is used to define the read and write completion events
//the "complete()"" function is also declared in this file
#include<linux/spinlock.h>//provides necessary definitons and functions for working with spinlocks in linux
//used to declare the "spin_lock_init()"function which is used to initialize the spinlock inside "anchor" structure
#include<linux/module.h>//provides the necessary definitions and functions for working with kernel modules in Linux.
//used to declare "module_init()" and "module_exit()" macros,which are used to define the iniitialization and exit functions for the kernel module
//"MODULE_LICENSE","MODULE AUTHOR","MODULE_DESCRIPTION" macros are also declared in this header file,which provide metadata about the kernel
#include<linux/kernel.h>//provides the necessary definitions and functions for working with the Linux kernel.
//used to declare "printk()" function,which is used to print messages to the kernel log
//"kmalloc()" and "kfree()" functions are also declared in this header file
#include<linux/fs.h>//provides the necessary definitions and functions for working with file systems in Linux.
//used to declare "struct file_operations" type,which is used to define the file operations for USB CDC device
//file_operations isused to define ""open,"read,"write","release"
#include<linux/slab.h>//provides the necessary definitions and functions for working with memory allocation in Linux.
//used to declare "kmalloc()","kzalloc()" and "kfree()" functions which are used to allocate and free memory in the kernel
#include<linux/uaccess.h>//provides the necessary definitions and functions for working with user-space access in Linux.
//used to declalre "copy_to_user()" and "copy_from_user()" functions which are used to copy data between user space and kernel space

//#DEFINES
#define USB_CDC_VENDOR_ID 0x0483 //vendor id of stm32
#define USB_CDC_PRODUCT_ID 0x5740 //product id of stm32
#define USB_CDC_EP_IN 0x81 //endpoint address for IN direction
#define USB_CDC_EP_OUT 0x01 //endpoint address for OUT direction
#define USB_CDC_BUFSIZE 512 //buffer size for data transfer

//USB STRUCTURES
static struct usb_device *device; //struct usb_device - kernel's representation of a USB device
static struct usb_class_driver class; //struct usb_class_driver - identifies a USB driver that wants to use the USB major number
struct usb_anchor *anchor; //to anchor URBs to a common mooring
struct urb *read_urb; //Define urb for read operation
struct urb *write_urb; //Define urb for write operation
DECLARE_COMPLETION(read_completion);
DECLARE_COMPLETION(write_completion);
static char kdata[USB_CDC_BUFSIZE];

//FUNCTION TO HANDLE READ COMPLETION
static void read_complete(struct urb *urb)
{
    //SIGNAL COMPLETION EVENT FOR READ OPERATION
    complete(&read_completion);
}

//FUNCTION TO HANDLE WRITE COMPLETION
static void write_complete(struct urb *urb)
{
    //SIGNAL COMPLETION EVENT FOR WRITE OPERATION
    complete(&write_completion);
}

//DECLERATIONS OF FUNCTIONS
static int cdc_open(struct inode *, struct file *); //for open()
static int cdc_close(struct inode *, struct file *); //for close()
static ssize_t cdc_read(struct file *, char *, size_t, loff_t *); //for read()
static ssize_t cdc_write(struct file *, const char *, size_t, loff_t *); //for write()
static int cdc_probe(struct usb_interface *intf, const struct usb_device_id *id); //for probe()
static void cdc_disconnect(struct usb_interface *intf); //for disconect()

//DEVICE FILE OPERATIONS STRUCTURE VARIABLES
static struct file_operations cdc_fops = 
{
    .owner = THIS_MODULE,
    .open = cdc_open,
    .release = cdc_close,
    .read = cdc_read,
    .write = cdc_write
};

//DEVICE ID TABLE FOR ENTRY OF OUR DEVICE
static struct usb_device_id cdc_device_table[] = 
{
    {USB_DEVICE(USB_CDC_VENDOR_ID,USB_CDC_PRODUCT_ID)},
    {}
};

//USB DEVICE FUNCTIONALITIES
//struct usb_driver - identifies USB interface driver to usbcore ->it is a data structure that represents a usb_driver
//It's used to define the characteristics and behavior of a USB drive
static struct usb_driver cdc_driver = 
{
    .name = "usb_cdc_device_driver",//The driver name should be unique among USB drivers,*and should normally be the same as the module name.
    .probe = cdc_probe,//This member is a pointer to a function that's called when a USB device is inserted and matches the id_table
                                //The probe function is responsible for initializing the driver and claiming the USB interface.
    .disconnect = cdc_disconnect,//This member is a pointer to a function that's called when a USB device is removed.
                                        //The disconnect function is responsible for cleaning up any resources allocated by the driver
    .id_table = cdc_device_table,//USB drivers use ID table to support hotplugging.
                                //*Export this with MODULE_DEVICE_TABLE(usb,...).This must be set
                                // *or your driver's probe function will never get called.
};

//KERNEL MODULE INIT FUNCTION 
static int __init cdc_init(void)
{
    int ret;
    anchor = kzalloc(sizeof(struct usb_anchor),GFP_KERNEL);//allocate memory. The memory is set to zero.
    if(!anchor) 
    {
        printk(KERN_ERR"%s : Failed to allocate memory for anchor\n",THIS_MODULE->name);//print a kernel message and kern_err reps error condition if occoured
        return -ENOMEM;
    }

    spin_lock_init(&anchor->lock);

    INIT_LIST_HEAD(&anchor->urb_list); 
//  * INIT_LIST_HEAD - Initialize a list_head structure
//  * @list: list_head structure to be initialized.
//  *
//  * Initializes the list_head to point to itself.  If it is a list header,
//  * the result is an empty list.

    ret = usb_register(&cdc_driver); //for register usb device driver
    if(ret < 0)
    {
        printk(KERN_ERR"%s : Failed to register USB CDC device driver\n",THIS_MODULE->name);
        kfree(anchor);
        return ret;
    }
    printk(KERN_INFO"%s : USB CDC device driver registered successfully\n",THIS_MODULE->name);

    return 0;
}

//KERNEL MODULE EXIT FUNCTION
static void __exit cdc_exit(void)
{
    usb_deregister(&cdc_driver); //removing registered device driver
    kfree(anchor);//freeing memory allocated for anchor
    printk(KERN_INFO"%s : usb driver deregistered \n",THIS_MODULE->name);
}

module_init(cdc_init);
module_exit(cdc_exit);

//PROBE FUNCTIONALITY
//struct usb_interface - what usb device drivers talk to 
//struct usb_device_id - identifies USB devices for probing and hotplugging
static int cdc_probe(struct usb_interface *intf, const struct usb_device_id *id) 
{
    int ret;
    
    struct usb_host_interface *iface_desc; //iface_Desc is interface descriptor
    //host-side wrapper for one interface setting's parsed descriptors 
    //i.e. this function lets our host machine(computer) know about one of the interface of the device that is in our device 
    //device can have multiple fuctionalities

    iface_desc = intf->cur_altsetting; //the currently active alternate setting
    //we are assigning the current alternate setting that is the device interface to which we are connecting,its current setting we are storing in iface_desc

    printk(KERN_INFO"%s : USB CDC device plugged with vendor id => %x and product id => %x \n",THIS_MODULE->name,id->idVendor,id->idProduct);
    device = interface_to_usbdev(intf);//we are basically giving access of pur entire device by using the interface we got
    //device: This is a variable that will hold a pointer to a struct usb_device. This structure represents the entire USB device connected to your computer.
    //interface_to_usbdev(): This is a helper function provided by the Linux USB core. It takes a pointer to a usb_interface structure as an argument and returns a pointer to the usb_device structure associated with that interface.
    //intf parameter represents a specific interface of the USB device, which is one part or function of the whole device.
    if(!device)
    {
        printk(KERN_ERR"%s : interface_to_usbdev() failed !\n",THIS_MODULE->name);
        return -ENOMEM;
    }
    printk(KERN_INFO"%s : New USB device connected : %s .\n",THIS_MODULE->name,device->devpath);
    class.name = "usb_cdc%d";//the usb class device name for this driver.  Will show up in sysfs.
    class.fops = &cdc_fops;//pointer to the struct file_operations of this driver.
    ret = usb_register_dev(intf,&class);//used for registering usb device with the usb core with the help of the interface found and the class created
    //here we are creating a node in /dev so that user space applications can communicate with our device 
    //his enables user-space applications to access the USB device using standard file operations like open, read, write, and close
    if(ret < 0)
    {
        printk(KERN_ERR"usb_register_dev() failed.\n");
        return ret;
    }
    printk(KERN_INFO"%s : Device Registered Successfully \n",THIS_MODULE->name);

    return 0;
}

//DISCONNECT FUNCTIONALITY
static void cdc_disconnect(struct usb_interface *intf)
{
    usb_deregister_dev(intf,&class);//this removes the device node from /dev directory and reverses the operations performed by use_register_dev()
    printk(KERN_INFO"%s : USB CDC device successfully disconnected .\n",THIS_MODULE->name);
}

//F_OPS OPEN
static int cdc_open(struct inode *pinode, struct file *pfile)
{
    printk(KERN_INFO"%s : cdc_open() called .\n",THIS_MODULE->name);
    return 0;
}

//FOPS CLOSE
static int cdc_close(struct inode *pinode, struct file *pfile)
{
    printk(KERN_INFO"%s : cdc_close called .\n",THIS_MODULE->name);
    return 0;
}

//F_OPS READ
static ssize_t cdc_read(struct file *pfile, char __user *ubuf, size_t usize, loff_t *poffset)
{
    int pipe,ret_value,length=0,ret;
    char *kbuffer;

    printk(KERN_INFO"%s : cdc_read() called.\n",THIS_MODULE->name); // HERE WE ARE CONFIRMING THAT CDC_READ IS CALLED

    //HERE WE ARE INITIALIZING THE PIPE IN THE DIRECTION OF CLIENT -> HOST I.E. DRIVER IS READING DATA FROM OUR DEVICE
    pipe = usb_rcvbulkpipe(device,USB_CDC_EP_IN);

    //ALLOCATING BUFFER TO RECIEVE DATA
    kbuffer = kmalloc(USB_CDC_BUFSIZE,GFP_KERNEL); // HERE THROUGH KMALLOC WE CREATED A BUFFER OF USB_CDC_sIZE THAT IS 512 BYTES 
    if(!kbuffer)
    {
        printk(KERN_INFO"%s : Failed to allocate memory for recieve buffer .\n",THIS_MODULE->name); //ERROR CHECKING
        return -ENOMEM;
    }
    printk(KERN_INFO"%s : Buffer allocated successfully.\n",THIS_MODULE->name); // IF NO ERROR SUCCESSFULLY CREATED THE BUFFER

    read_urb = usb_alloc_urb(0,GFP_KERNEL);//used for allocating a USB Request Block (URB)
    //and the first arguement is set to zero because we are not transfering data for isochronous transfer
    //other than isochronous transfer for other transfers the first arguement can be set to zero(0)
    if(read_urb == NULL)
    {
        printk(KERN_ERR"%s : Failed to allocate memory for recieve buffer .\n",THIS_MODULE->name); // ERROR CHECKING
        kfree(kbuffer);
        return -ENOMEM;
    }
    printk(KERN_INFO"%s : URB in read is allocated successfully\n",THIS_MODULE->name); //SUCCESSFULLY ALLOCATED A URB FOR DATA TRANSFER 

    //FILLING THE URB FOR TRANSFER
    usb_fill_bulk_urb(read_urb,device,pipe,kbuffer,USB_CDC_BUFSIZE,read_complete,NULL); 
    // HERE THE DATA RECIEVED AT PIPE(I.E. SENSOR DATA) IS SENT TO OUR ALLOCATED URB FOR DATA TRANSFER IN THE KERNEL BUFFER 
    //AND THEN THIS DATA IS STORED IN THE KERNEL BUFFER WE ALLOCATED
    //ARGUEMENTS OF USB_FILL_BULK_URB
    //IN Transfer (Device to Host): For bulk IN transfers, the data to be received from the USB device is stored in the buffer you 
    //specify (transfer_buffer). This means the URB will carry data from the USB device to your system (the host).
    //1ST ARGUEMENT => the URB is allocated/initialised for the data transfer.
    //2ND ARGUEMENT => is the USB device structure from where data will be recieved to the host system
    //3RD ARGUEMENT => the pipe thru which data will be read or we can say the direction from where we wann read as we wanna read the dadta from the device the pipe direction will be IN as we need to read data from the device
    //4TH ARGUEMENT => the buffer in which we want the data in our case it is the kbuffer(kernel buffer) where we want the data from the device to be stored whic will be carried to the buffer by read_urb i.e. usb_alloc_urb functionality
    //5TH ARGUEMENT => the length of the buffer where we want to recieve the data which is in our case is 512 bytes
    //6TH ARGUEMENT => read_complete() function will be called as a sign/signal to confirm that our reading operation is complete
    //7TH ARGUEMENT => is any additional data to be passed to the completion function.

    //SUBMITIING THE URB FOR TRANSFER
    ret_value = usb_submit_urb(read_urb,GFP_KERNEL); // HERE WE ARE SUBMITTING THE URB FOR TRANSFER FROM OUR DEVICE TO THE HOST SYSTEM
    if(ret_value < 0)
    {
        printk(KERN_ERR"%s : Failed to submit urb for bulk transfer.\n",THIS_MODULE->name);//ERROR CHECKING
        usb_free_urb(read_urb);
        kfree(kbuffer);
        return ret_value;
    }
    printk(KERN_INFO"%s : successfully submitted urb for bulk tranfer : direction-> from device to host",THIS_MODULE->name);

    //WAITING FOR TRANSFER OF DATA
    wait_for_completion(&read_completion);//WAITING FOR THE TRANSFER TO COMPLETE FROM DEVICE TO HOST
    // This function call will block until the transfer completes and the completion function i.e.read_completion is called.

    //ACTUAL LENGTH OF DATA RECIEVED
    length = read_urb->actual_length; // // Retrieve or getting the amount of data that was actually transferred.

    //COPYING THE DATA TO USER BUFFER
    ret = copy_to_user(ubuf,kbuffer,min((unsigned long)length,(unsigned long)usize)); 
    //HERE WE ARE COPYING THE DATA RECIEVED FROM DEVICE IN KERNEL BUFFER (KBUFFER) TO THE USER BUFFER (UBUF).
    if(ret != 0)
    {
        printk(KERN_ERR"%s : Failed to copy data to user buffer.\n",THIS_MODULE->name);
        usb_free_urb(read_urb);
        kfree(kbuffer);
        return -EFAULT;
    }
    printk(KERN_INFO"%s : successfully copied data from kernel buffer to user buffer",THIS_MODULE->name);

    usb_free_urb(read_urb); //FREEING THE URB WE HAVE ALLOCATED FOR DATA TRANSFER FRO DEVICE TO OUR KERNEL BUFFER
    kfree(kbuffer); //FREEING THE KERNEL BUFFER WE ALLOCATED TO STORE DATA RECIEVED FROM THE DEVICE

    printk(KERN_INFO"%s : cdc_read() is executed successfully.\n",THIS_MODULE->name);

    return length;
}

static ssize_t cdc_write(struct file *pfile, const char __user *ubuf, size_t usize, loff_t *poffset)
{
    int pipe,ret_value,length=0,ret;

    printk(KERN_INFO"%s : cdc_write() called.\n",THIS_MODULE->name); //CDC_WRITE CALLED SUCCESSFULLY

    //HERE WE ARE INITIALIZING THE PIPE IN THE DIRECTION OF DEVICE => SERVER(WHO SERVES THE REQUEST) 
    //HERE WE WILL BE SENDING DATA FROM OUR HAST MACHINE TO THE DEVICE
    pipe = usb_sndbulkpipe(device,USB_CDC_EP_OUT); //WE ARE SENDING DATA OUT FROM OUR HOST MACHINE TO THE DEVICE

    //ALLOCATING AN URB
    write_urb = usb_alloc_urb(0,GFP_KERNEL); 
    //ALLOCATING URB FOR DATA TRANSFER FROM OUR HOST TO DEVICE AND THAT IS WHY THE ENDPOINT DIRECTION IN SNDBULKPIPE() IS SET TO OUT AS WE WANT TO SEND DATA OUT FROM HOST TO OUR DEVICE 
    if(write_urb == 0)
    {
        printk(KERN_ERR"%s : failed to allocate URB.\n",THIS_MODULE->name);//ERROR CHECKING
        return -ENOMEM;
    }
    printk(KERN_INFO"%s : urb in write allocated successfully.\n",THIS_MODULE->name); //SUCCESSFULLY ALLOCATED URB FOR DATA TRANSFER FROMHOST TO DEVICE

    //COPY DATA FROM USER BUFFER 
    ret = copy_from_user(kdata,ubuf,min((unsigned long)usize,(unsigned long)USB_CDC_BUFSIZE)); //COPY DATA FROM USER BUFFER TO KERNEL BUFFER WHICH IS NAMED AS KDATA HERE IN OUR CODE
    if(ret != 0)
    {
        printk(KERN_ERR"%s : failed to copy data from user buffer.\n",THIS_MODULE->name); //ERROR CHECKING
        usb_free_urb(write_urb);
        return -EFAULT;
    }
    printk(KERN_INFO"%s : successfully copied data from user buffer to kernel buffer.",THIS_MODULE->name);

    usb_fill_bulk_urb(write_urb,device,pipe,kdata,min((unsigned long)usize,(unsigned long)USB_CDC_BUFSIZE),write_complete,NULL);
    //OUT Transfer (Host to Device): For bulk OUT transfers, the data to be sent to the USB device is provided in the buffer you 
    //specify (transfer_buffer). This means the URB will carry the data from the host (your system) to the USB device.
    //HERE WE ARE FIRST COPYING THE DATA WE WANT TO SEND TO DEVICE IN KERNEL BUFFER I.E. COPYING DATA FROM USER BUFFER TO KERNEL BUFFER
    //AND THEN THROUGH URB(WRITE_URB) WE ARE TRANSFERING THE DATA FROM KERNEL BUFFER TO DEVICE USING PIPE 
    //HERE WE ARE SENDING DATA WHICH WILL BE MINIMUM ELSE IT WILL BE THE SIZE OF USER BUFFER SIZE OR IT WILL THE MAX SIZE OF THE BUFFER USB_CDC_BUFSIZE
    //AFTER THIS write_complete() function will be called as a sign/signal to confirm that our reading operation is complete
    //NO EXTRA/ADDITIONAL DATA WE WANT TO SEND TO THE COMPLETION FUNCTION

    //SUBMITTING THE URB FOR TRANSFER
    ret_value = usb_submit_urb(write_urb,GFP_KERNEL); //HERE WE ARE SUBMITTING OUR URB FOR DATA TRANSFER BETWEEN HOST AND THE DEVICE I.E. HOST TO DEVICE DATA WILL BE TRANSFERRED
    if(ret_value < 0)
    {
        printk(KERN_ERR"%s : Failed to submit urb for bulk transfer.\n",THIS_MODULE->name); //ERROR CHECKING
        usb_free_urb(write_urb);
        return ret_value;
    }
    printk(KERN_INFO"%s : successfully submitted urb for bulk transfer.\n",THIS_MODULE->name);

    //WAITING FOR TRANSFER TO COMMPLETE
    wait_for_completion(&write_completion);

    //ACTUAL LENGTH OF THE DATA WRITTEN
    length = write_urb->actual_length;

    usb_free_urb(write_urb);
    
    printk(KERN_ERR"%s : cdc_write() executed.\n",THIS_MODULE->name);

    return length;
}

MODULE_LICENSE("GPL");
MODULE_AUTHOR("PG-DESD Final Project-Ashaya.Ramteke,Pratham.Chudival,Vinayak.Thosar,Akash.Agrawal");
MODULE_DESCRIPTION("USB Device Driver Using STM32F407 Discovery Board");