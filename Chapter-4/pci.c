#include <linux/cdev.h> /* cdev_ */
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/ioctl.h>
#include <linux/delay.h>
#include <linux/uaccess.h> /* put_user */

#define BAR 0
#define CDEV_NAME "pci_edu"
#define EDU_DEVICE_ID 0x11e8
#define QEMU_VENDOR_ID 0x1234


#define FACTORIA_VAL 0x8
#define IO_FACTORIA_IRQ 0x20
#define IO_IRQ_STATUS 0x24
#define IO_IRQ_RAISE 0x60
#define IO_IRQ_ACK 0x64
#define IO_DMA_SRC 0x80
#define IO_DMA_DST 0x88
#define IO_DMA_CNT 0x90
#define IO_DMA_CMD 0x98

#define DMA_BASE 0x40000
#define DMA_CMD 0x1
#define DMA_FROM_MEM 0x2
#define DMA_TO_MEM 0x0
#define DMA_IRQ 0x4

#define MAGIC 'k'
#define DMA_READ_CMD    _IO(MAGIC,0x1a)
#define DMA_WRITE_CMD    _IO(MAGIC,0x1b)
#define PRINT_EDUINFO_CMD    _IO(MAGIC,0x1c)
#define SEND_INTERRUPT_CMD    _IO(MAGIC,0x1d)
#define FACTORIAL_CMD    _IO(MAGIC,0x1e)

static struct pci_device_id pci_ids[] = {
	{ PCI_DEVICE(QEMU_VENDOR_ID, EDU_DEVICE_ID), },
	{ 0, }
};
MODULE_DEVICE_TABLE(pci, pci_ids);

static int major;
static struct pci_dev *pdev;
static void __iomem *mmio;


static int edu_open(struct inode *inode, struct file *file)
{

    /*
    if(file->f_mode & FMODE_READ) {

    }
    if(file->f_mode & FMODE_WRITE) {

    }*/
    

	return 0;
}

static ssize_t edu_read(struct file *filp, char __user *buf, size_t len, loff_t *off)
{
	ssize_t ret;
	u32 kbuf;

	if (*off % 4 || len == 0) {
		ret = 0;
	} else {
		kbuf = ioread32(mmio + *off);
		if (copy_to_user(buf, (void *)&kbuf, 4)) {
			ret = -EFAULT;
		} else {
			ret = 4;
			(*off)++;
		}
	}
	return ret;
}

static ssize_t edu_write(struct file *filp, const char __user *buf, size_t len, loff_t *off)
{
	ssize_t ret;
	u32 kbuf;

	ret = len;
	if (!(*off % 4)) {
		if (copy_from_user((void *)&kbuf, buf, 4) || len != 4) {
			ret = -EFAULT;
		} else {
			iowrite32(kbuf, mmio + *off);
		}
	}
	return ret;
}

static long edu_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    unsigned i;
    u8 val;
    switch(cmd) {
        case DMA_WRITE_CMD:
            
						
			iowrite32((uintptr_t)(mmio+0x100), mmio + IO_DMA_DST);
           	iowrite32(DMA_BASE, mmio + IO_DMA_SRC);
			iowrite32(4, mmio + IO_DMA_CNT);
			iowrite32(DMA_CMD|DMA_FROM_MEM|DMA_IRQ, mmio + IO_DMA_CMD);
			msleep(1000);
			
			break;
        case DMA_READ_CMD:
			
			iowrite32(DMA_BASE, mmio + IO_DMA_DST);
            iowrite32((uintptr_t)(mmio+0x104), mmio + IO_DMA_SRC);
			iowrite32(4, mmio + IO_DMA_CNT);
			iowrite32(DMA_CMD|DMA_TO_MEM|DMA_IRQ, mmio + IO_DMA_CMD);
			msleep(1000);
			break;

		case PRINT_EDUINFO_CMD:
			if ((pci_resource_flags(pdev, BAR) & IORESOURCE_MEM) != IORESOURCE_MEM) {
				dev_err(&(pdev->dev), "pci_resource_flags\n");
				return 1;
			}
			resource_size_t start = pci_resource_start(pdev, BAR);
			resource_size_t end = pci_resource_end(pdev, BAR);
			pr_info("length %llx\n", (unsigned long long)(end + 1 - start));
			for (i = 0; i < 64u; ++i) {
					pci_read_config_byte(pdev, i, &val);
					pr_info("config %x %x\n", i, val);
			}
			pr_info("dev->irq %x\n", pdev->irq);
			for (i = 0; i < 0x98; i += 4) {
					pr_info("io %x %x\n", i, ioread32((void*)(mmio + i)));
			}
			break;
		case SEND_INTERRUPT_CMD:
			iowrite32(0x12345678, mmio + IO_IRQ_RAISE);
			break;
		case FACTORIAL_CMD:
			iowrite32(0x80, mmio + IO_FACTORIA_IRQ);
			msleep(1000);
			iowrite32(0xA, mmio + FACTORIA_VAL);
			msleep(1000);
			pr_info("computing result %x\n", ioread32((void*)(mmio + FACTORIA_VAL)));
			

    }
    return 0;
}

static loff_t llseek(struct file *filp, loff_t off, int whence)
{
	filp->f_pos = off;
	return off;
}


static struct file_operations fops = {
	.owner   = THIS_MODULE,
	.llseek  = llseek,
	.read    = edu_read,
	.write   = edu_write,
	.open    = edu_open,
	.unlocked_ioctl  = edu_ioctl,
};

static irqreturn_t irq_handler(int irq, void *dev)
{
	int devi;
	irqreturn_t ret;
	u32 irq_status;

	devi = *(int *)dev;
	if (devi == major) {
		irq_status = ioread32(mmio + IO_IRQ_STATUS);
		if(irq_status == 0x100){
			pr_info("receive a DMA read interrupter!\n");
		}else if(irq_status == 0x101){
			pr_info("receive a DMA write interrupter!\n");
		}else if(irq_status == 0x1){
			pr_info("receive a FACTORIAL interrupter!\n");
		}
		pr_info("irq_handler irq = %d dev = %d irq_status = %llx\n",
				irq, devi, (unsigned long long)irq_status);
		iowrite32(irq_status, mmio + IO_IRQ_ACK);
		ret = IRQ_HANDLED;
	} else {
		ret = IRQ_NONE;
	}
	return ret;
}

static int pci_probe(struct pci_dev *dev, const struct pci_device_id *id)
{

	dev_info(&(dev->dev), "pci_probe\n");
	major = register_chrdev(0, CDEV_NAME, &fops);
	pdev = dev;
	if (pci_enable_device(dev) < 0) {
		dev_err(&(dev->dev), "pci_enable_device\n");
		goto error;
	}
	if (pci_request_region(dev, BAR, "myregion0")) {
		dev_err(&(dev->dev), "pci_request_region\n");
		goto error;
	}
	mmio = pci_iomap(dev, BAR, pci_resource_len(dev, BAR));

	// IRQ setup.

	if (request_irq(dev->irq, irq_handler, IRQF_SHARED, "pci_irq_handler0", &major) < 0) {
		dev_err(&(dev->dev), "request_irq\n");
		goto error;
	}
	return 0;
error:
	return 1;
}

static void pci_remove(struct pci_dev *dev)
{
	pr_info("pci_remove\n");
	free_irq(pdev->irq, &major);
	pci_release_region(dev, BAR);
	unregister_chrdev(major, CDEV_NAME);
}

static struct pci_driver pci_driver = {
	.name     = "edu_pci",
	.id_table = pci_ids,
	.probe    = pci_probe,
	.remove   = pci_remove,
};

static int edu_init(void)
{

    if (pci_register_driver(&pci_driver) < 0) {
		return 1;
	}
	return 0;
}

static void edu_exit(void)
{
	pci_unregister_driver(&pci_driver);
}

module_init(edu_init);
module_exit(edu_exit);
MODULE_LICENSE("GPL");


