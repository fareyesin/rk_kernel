 /*
*   FILE NAME  : wk2xxx_spi.c   
*
*   WKIC Ltd.
*   By  Xu XunWei Tech  
*   DEMO Version :2.4 Data:2022-07-24
*   DESCRIPTION: Implements an interface for the wk2xxx of spi interface
*
*  
*   
*/
#include <linux/init.h>                        
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/console.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>
#include <linux/serial_core.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/freezer.h>
#include <linux/spi/spi.h>
#include <linux/timer.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/workqueue.h>
#include <linux/platform_device.h>
#include <asm/irq.h>
#include <asm/io.h>
#include "linux/version.h"
#include <linux/regmap.h>
#include <linux/uaccess.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <uapi/linux/sched.h>

#include <uapi/linux/sched/types.h>



MODULE_LICENSE("Dual BSD/GPL");



#define DRIVER_AUTHOR "Xuxunwei"
#define DRIVER_DESC   "SPI driver for spi to Uart chip WK2XXX, etc."
#define VERSION_DESC  "V2.4 On 2022.07.24"
/*************The debug control **********************************/
#define _DEBUG_WK_FUNCTION
//#define _DEBUG_WK_RX
//#define _DEBUG_WK_TX
//#define _DEBUG_WK_IRQ
//#define _DEBUG_WK_VALUE
//#define _DEBUG_WK_TEST

/*************Functional control interface************************/
#define WK_FIFO_FUNCTION
//#define WK_FlowControl_FUNCTION
#define WK_WORK_KTHREAD
//#define WK_RS485_FUNCTION
//#define WK_RSTGPIO_FUNCTION
//#define WK_CSGPIO_FUNCTION
/*************SPI control interface******************************/
#define SPI_LEN_LIMIT       30    //MAX<=255

/*************Uart Setting interface******************************/
#define WK2XXX_TXFIFO_LEVEL		(0x01) /* TX FIFO level */
#define WK2XXX_RXFIFO_LEVEL		(0x40) /* RX FIFO level */

#define WK2XXX_STATUS_PE    1
#define WK2XXX_STATUS_FE    2
#define WK2XXX_STATUS_BRK   4
#define WK2XXX_STATUS_OE    8





static DEFINE_MUTEX(wk2xxxs_lock);               
static DEFINE_MUTEX(wk2xxxs_reg_lock);              
static DEFINE_MUTEX(wk2xxxs_global_lock);

/******************************************/
#define 	NR_PORTS 	                4         
//
#define 	SERIAL_WK2XXX_MAJOR	    	207
#define 	CALLOUT_WK2XXX_MAJOR		208	
#define 	MINOR_START		            5
//wk2xxx hardware configuration
#define		wk2xxx_spi_speed	        10000000
//#define 	WK_CRASTAL_CLK		        (24000000)
#define 	WK_CRASTAL_CLK		        (11059200)
#define 	WK2XXX_ISR_PASS_LIMIT	    2
#define		PORT_WK2XXX                 1
/******************************************/


/************** WK2XXX register definitions********************/
/*wk2xxx  Global register address defines*/
#define 	WK2XXX_GENA_REG     0X00    /*Slave UART Clock Set */
#define 	WK2XXX_GRST_REG     0X01    /*Reset Slave UART*/ 
#define		WK2XXX_GMUT_REG     0X02    /*Master UART Control*/
#define 	WK2XXX_GIER_REG     0X10    /*Slave UART Interrupt Enable */
#define 	WK2XXX_GIFR_REG     0X11    /*Slave UART Interrupt Flag*/
#define 	WK2XXX_GPDIR_REG    0X21    /*GPIO Direction*/ /*WK2168/WK2212*/
#define 	WK2XXX_GPDAT_REG    0X31    /*GPIO Data Input and Data Output*/ /*WK2168/WK2212*/


/*****************************
****wk2xxx  slave uarts  register address defines****
******************************/
#define 	WK2XXX_SPAGE_REG    0X03    /*Slave UART Register page selection*/
#define 	WK2XXX_PAGE1        1
#define 	WK2XXX_PAGE0        0

/*PAGE0**/
#define 	WK2XXX_SCR_REG      0X04    /*Slave UART Transmitter and Receiver Enable*/
#define 	WK2XXX_LCR_REG      0X05    /* Line Control */
#define 	WK2XXX_FCR_REG      0X06    /* FIFO control */
#define 	WK2XXX_SIER_REG     0X07    /* Interrupt enable */
#define 	WK2XXX_SIFR_REG     0X08    /* Interrupt Identification */
#define 	WK2XXX_TFCNT_REG    0X09    /* TX FIFO counter */
#define 	WK2XXX_RFCNT_REG    0X0A    /* RX FIFO counter */
#define 	WK2XXX_FSR_REG      0X0B    /* FIFO Status */
#define 	WK2XXX_LSR_REG      0X0C    /* Line Status */
#define 	WK2XXX_FDAT_REG     0X0D    /*  Write transmit FIFO data or Read receive FIFO data */
#define 	WK2XXX_FWCR_REG     0X0E    /* Flow  Control */
#define 	WK2XXX_RS485_REG    0X0F    /* RS485 Control */
/*PAGE1*/
#define 	WK2XXX_BAUD1_REG    0X04    /* Divisor Latch High */
#define 	WK2XXX_BAUD0_REG    0X05    /* Divisor Latch Low */
#define 	WK2XXX_PRES_REG     0X06    /* Divisor Latch Fractional Part */
#define 	WK2XXX_RFTL_REG     0X07    /* Receive FIFO Trigger Level */
#define 	WK2XXX_TFTL_REG     0X08    /* Transmit FIFO Trigger Level */
#define 	WK2XXX_FWTH_REG     0X09    /*Flow control trigger high level*/
#define 	WK2XXX_FWTL_REG     0X0A    /*Flow control trigger low level*/
#define 	WK2XXX_XON1_REG     0X0B    /* Xon1 word */
#define 	WK2XXX_XOFF1_REG    0X0C    /* Xoff1 word */
#define 	WK2XXX_SADR_REG     0X0D    /*RS485 auto address*/
#define 	WK2XXX_SAEN_REG     0X0E    /*RS485 auto address mask*/
#define 	WK2XXX_RRSDLY_REG   0X0F    /*RTS delay when transmit in RS485*/


//wkxxx register bit defines
/*GENA register*/
#define 	WK2XXX_GENA_UT4EN_BIT	    0x08
#define 	WK2XXX_GENA_UT3EN_BIT	    0x04
#define 	WK2XXX_GENA_UT2EN_BIT	    0x02
#define 	WK2XXX_GENA_UT1EN_BIT	    0x01
/*GRST register*/
#define 	WK2XXX_GRST_UT4SLEEP_BIT	0x80
#define 	WK2XXX_GRST_UT3SLEEP_BIT	0x40
#define 	WK2XXX_GRST_UT2SLEEP_BIT	0x20
#define 	WK2XXX_GRST_UT1SLEEP_BIT	0x10
#define 	WK2XXX_GRST_UT4RST_BIT	    0x08
#define 	WK2XXX_GRST_UT3RST_BIT		0x04
#define 	WK2XXX_GRST_UT2RST_BIT		0x02
#define 	WK2XXX_GRST_UT1RST_BIT		0x01
/*GIER register bits*/
#define 	WK2XXX_GIER_UT4IE_BIT		0x08
#define 	WK2XXX_GIER_UT3IE_BIT		0x04
#define 	WK2XXX_GIER_UT2IE_BIT		0x02
#define 	WK2XXX_GIER_UT1IE_BIT		0x01
/*GIFR register bits*/
#define 	WK2XXX_GIFR_UT4INT_BIT		0x08
#define 	WK2XXX_GIFR_UT3INT_BIT		0x04
#define 	WK2XXX_GIFR_UT2INT_BIT		0x02
#define 	WK2XXX_GIFR_UT1INT_BIT		0x01
/*SPAGE register bits*/
#define 	WK2XXX_SPAGE_PAGE_BIT	    0x01
/*SCR register bits*/
#define 	WK2XXX_SCR_SLEEPEN_BIT      0x04
#define 	WK2XXX_SCR_TXEN_BIT         0x02
#define 	WK2XXX_SCR_RXEN_BIT         0x01
/*LCR register bits*/
#define 	WK2XXX_LCR_BREAK_BIT	    0x20
#define 	WK2XXX_LCR_IREN_BIT         0x10

#define 	WK2XXX_LCR_PAEN_BIT         0x08
#define 	WK2XXX_LCR_PAM1_BIT         0x04
#define 	WK2XXX_LCR_PAM0_BIT         0x02
/*ODD Parity*/
#define     WK2XXX_LCR_ODD_PARITY       0x0a
/*Even Parity*/
#define     WK2XXX_LCR_EVEN_PARITY      0x0c
/*Parity :=0*/
#define     WK2XXX_LCR_SPACE_PARITY     0x08
/*Parity :=1*/           
#define     WK2XXX_LCR_MARK_PARITY      0x0e

#define 	WK2XXX_LCR_STPL_BIT         0x01


/*FCR register bits*/
#define 	WK2XXX_FCR_TFEN_BIT           0x08
#define 	WK2XXX_FCR_RFEN_BIT           0x02
#define 	WK2XXX_FCR_RFRST_BIT          0x01
/*SIER register bits*/
#define 	WK2XXX_SIER_FERR_IEN_BIT      0x80
#define 	WK2XXX_SIER_CTS_IEN_BIT       0x40
#define 	WK2XXX_SIER_RTS_IEN_BIT       0x20
#define 	WK2XXX_SIER_XOFF_IEN_BIT      0x10
#define 	WK2XXX_SIER_TFEMPTY_IEN_BIT   0x08
#define 	WK2XXX_SIER_TFTRIG_IEN_BIT    0x04
#define 	WK2XXX_SIER_RXOUT_IEN_BIT     0x02
#define 	WK2XXX_SIER_RFTRIG_IEN_BIT    0x01
/*SIFR register bits*/
#define 	WK2XXX_SIFR_FERR_INT_BIT      0x80
#define 	WK2XXX_SIFR_CTS_INT_BIT       0x40
#define 	WK2XXX_SIFR_RTS_INT_BIT       0x20
#define 	WK2XXX_SIFR_XOFF_INT_BIT      0x10
#define 	WK2XXX_SIFR_TFEMPTY_INT_BIT   0x08
#define 	WK2XXX_SIFR_TFTRIG_INT_BIT    0x04
#define 	WK2XXX_SIFR_RXOVT_INT_BIT     0x02
#define 	WK2XXX_SIFR_RFTRIG_INT_BIT    0x01
/*FSR register bits*/
#define 	WK2XXX_FSR_RFOE_BIT           0x80
#define 	WK2XXX_FSR_RFBI_BIT           0x40
#define 	WK2XXX_FSR_RFFE_BIT           0x20
#define 	WK2XXX_FSR_RFPE_BIT           0x10

#define 	WK2XXX_FSR_ERR_MASK           0xF0

#define 	WK2XXX_FSR_RDAT_BIT           0x08
#define 	WK2XXX_FSR_TDAT_BIT           0x04
#define 	WK2XXX_FSR_TFULL_BIT          0x02
#define 	WK2XXX_FSR_TBUSY_BIT          0x01
/*LSR register bits*/
#define     WK2XXX_LSR_BRK_ERROR_MASK     0X0F  /* BI, FE, PE, OE bits */
#define 	WK2XXX_LSR_OE_BIT             0x08
#define 	WK2XXX_LSR_BI_BIT             0x04
#define 	WK2XXX_LSR_FE_BIT             0x02
#define 	WK2XXX_LSR_PE_BIT             0x01
/*FWCR register bits*/
#define 	WK2XXX_FWCR_RTS_BIT           0x02
#define 	WK2XXX_FWCR_CTS _BIT          0x01
/*RS485 register bits*/
#define 	WK2XXX_RS485_RSRS485_BIT      0x40
#define 	WK2XXX_RS485_ATADD_BIT        0x20
#define 	WK2XXX_RS485_DATEN_BIT        0x10
#define 	WK2XXX_RS485_RTSEN_BIT        0x02
#define 	WK2XXX_RS485_RTSINV_BIT       0x01

#ifdef WK_CSGPIO_FUNCTION 
int cs_gpio_num;
#endif

struct wk2xxx_devtype {
	char name[10];
	int	 nr_uart;
};
struct wk2xxx_one 
{

    struct uart_port port;//[NR_PORTS];
    struct kthread_work		start_tx_work;
	struct kthread_work		stop_rx_work;
    uint8_t line;
    uint8_t new_lcr_reg;
	uint8_t new_fwcr_reg;
    uint8_t new_scr_reg; 
    /*baud register*/
    uint8_t new_baud1_reg;
    uint8_t new_baud0_reg;
    uint8_t new_pres_reg;
};

struct wk2xxx_port 
{
    const struct wk2xxx_devtype	*devtype;
    struct uart_driver		uart;
    struct spi_device *spi_wk;
    struct workqueue_struct *workqueue;
    struct work_struct work;
    unsigned char			buf[256];
	struct kthread_worker	kworker;
	struct task_struct		*kworker_task;
	struct kthread_work		irq_work;
    //int cs_gpio_num;
    int irq_gpio_num;
    int rst_gpio_num;
    int irq_gpio;
    int minor;      /* minor number */
    int tx_empty; 
    struct wk2xxx_one		p[NR_PORTS];
};

static const struct wk2xxx_devtype wk2124_devtype = {
	.name		= "WK2124",
	.nr_uart	= 4,
};
static const struct wk2xxx_devtype wk2132_devtype = {
	.name		= "WK2132",
	.nr_uart	= 2,
};
static const struct wk2xxx_devtype wk2204_devtype = {
	.name		= "WK2204",
	.nr_uart	= 4,
};
static const struct wk2xxx_devtype wk2168_devtype = {
	.name		= "WK2168",
	.nr_uart	= 4,
};
static const struct wk2xxx_devtype wk2202_devtype = {
	.name		= "WK2202",
	.nr_uart	= 2,
};

#define to_wk2xxx_port(p,e)	((container_of((p), struct wk2xxx_port, e)))
#define to_wk2xxx_one(p,e)	((container_of((p), struct wk2xxx_one, e)))

/*
* This function read wk2xxx of Global register:
*/
static int wk2xxx_read_global_reg(struct spi_device *spi,uint8_t reg,uint8_t *dat)
{
    struct spi_message msg;
    uint8_t buf_wdat[2];
    uint8_t buf_rdat[2];
    int status;
    struct spi_transfer index_xfer = {
                .len            = 2,
                .speed_hz	= wk2xxx_spi_speed,
    };
    mutex_lock(&wk2xxxs_reg_lock);
    status =0;
    #ifdef WK_CSGPIO_FUNCTION 
    gpio_set_value(cs_gpio_num, 0); 
    #endif
    spi_message_init(&msg);
    buf_wdat[0] = 0x40|reg;
    buf_wdat[1] = 0x00;
    buf_rdat[0] = 0x00;
    buf_rdat[1] = 0x00;
    index_xfer.tx_buf = buf_wdat;
    index_xfer.rx_buf =(void *) buf_rdat;
    spi_message_add_tail(&index_xfer, &msg);
    status = spi_sync(spi, &msg);
    #ifdef WK_CSGPIO_FUNCTION 
    gpio_set_value(cs_gpio_num, 1); 
    #endif
    mutex_unlock(&wk2xxxs_reg_lock);
    if(status){
        return status;
    }
    *dat = buf_rdat[1];
    return 0;
}
/*
* This function write wk2xxx of Global register:
*/
static int wk2xxx_write_global_reg(struct spi_device *spi,uint8_t reg,uint8_t dat)
{
    struct spi_message msg;
    uint8_t buf_reg[2];
    int status;
    struct spi_transfer index_xfer = {
            .len            = 2,
            .speed_hz	= wk2xxx_spi_speed,
    };
    mutex_lock(&wk2xxxs_reg_lock);
    #ifdef WK_CSGPIO_FUNCTION 
    gpio_set_value(cs_gpio_num, 0); 
    #endif
    spi_message_init(&msg);
    /* register index */
    buf_reg[0] = 0x00|reg;
    buf_reg[1] = dat;
    index_xfer.tx_buf = buf_reg;
    spi_message_add_tail(&index_xfer, &msg);
    status = spi_sync(spi, &msg);
    #ifdef WK_CSGPIO_FUNCTION 
    gpio_set_value(cs_gpio_num, 1);
    #endif
    mutex_unlock(&wk2xxxs_reg_lock);
    return status;
}
/*
* This function read wk2xxx of slave register:
*/
static int wk2xxx_read_slave_reg(struct spi_device *spi,uint8_t port,uint8_t reg,uint8_t *dat)
{
    struct spi_message msg;
    uint8_t buf_wdat[2];
    uint8_t buf_rdat[2];
    int status;
    struct spi_transfer index_xfer = {
            .len            = 2,
            .speed_hz	= wk2xxx_spi_speed,
    };
    mutex_lock(&wk2xxxs_reg_lock);
    #ifdef WK_CSGPIO_FUNCTION 
    gpio_set_value(cs_gpio_num, 0);
    #endif
    status =0;
    spi_message_init(&msg);
    buf_wdat[0] = 0x40|(((port-1)<<4)|reg);
    buf_wdat[1] = 0x00;
    buf_rdat[0] = 0x00;
    buf_rdat[1] = 0x00;
    index_xfer.tx_buf = buf_wdat;
    index_xfer.rx_buf =(void *) buf_rdat;
    spi_message_add_tail(&index_xfer, &msg);
    status = spi_sync(spi, &msg);
    #ifdef WK_CSGPIO_FUNCTION 
    gpio_set_value(cs_gpio_num, 1);
    #endif
    mutex_unlock(&wk2xxxs_reg_lock);
    if(status){
        return status;
    }
    *dat = buf_rdat[1];
    return 0;

}
/*
* This function write wk2xxx of Slave register:
*/
static int wk2xxx_write_slave_reg(struct spi_device *spi,uint8_t port,uint8_t reg,uint8_t dat)
{
    struct spi_message msg;
    uint8_t buf_reg[2];
    int status;
    struct spi_transfer index_xfer = {
        .len            = 2,
		.speed_hz	= wk2xxx_spi_speed,
    };
    mutex_lock(&wk2xxxs_reg_lock);
    #ifdef WK_CSGPIO_FUNCTION 
    gpio_set_value(cs_gpio_num, 0);
    #endif
    spi_message_init(&msg);
    /* register index */
    buf_reg[0] = ((port-1)<<4)|reg;
    buf_reg[1] = dat;
    index_xfer.tx_buf = buf_reg;
    spi_message_add_tail(&index_xfer, &msg);
    status = spi_sync(spi, &msg);
    #ifdef WK_CSGPIO_FUNCTION 
    gpio_set_value(cs_gpio_num, 1);
    #endif
    mutex_unlock(&wk2xxxs_reg_lock);
    return status;
}

#define MAX_RFCOUNT_SIZE 256

/*
* This function read wk2xxx of fifo:
*/
static int wk2xxx_read_fifo(struct spi_device *spi,uint8_t port,uint8_t fifolen,uint8_t *dat)
{
    struct spi_message msg;
    int status,i;
    uint8_t recive_fifo_data[MAX_RFCOUNT_SIZE+1]={0};
    uint8_t transmit_fifo_data[MAX_RFCOUNT_SIZE+1]={0};
    struct spi_transfer index_xfer = {
            .len            = fifolen+1,
            .speed_hz	= wk2xxx_spi_speed,
    }; 
	if(!(fifolen>0)){
		printk(KERN_ERR "%s,fifolen error!!\n", __func__);
		return 1;
	}
    mutex_lock(&wk2xxxs_reg_lock);
    #ifdef WK_CSGPIO_FUNCTION 
    gpio_set_value(cs_gpio_num, 0);
    #endif
    spi_message_init(&msg);
    /* register index */
    transmit_fifo_data[0] = ((port-1)<<4)|0xc0;
    index_xfer.tx_buf = transmit_fifo_data;
    index_xfer.rx_buf =(void *) recive_fifo_data;
    spi_message_add_tail(&index_xfer, &msg); 
    status = spi_sync(spi, &msg);
    for(i=0;i<fifolen;i++)
        *(dat+i)=recive_fifo_data[i+1];
    #ifdef WK_CSGPIO_FUNCTION 
    gpio_set_value(cs_gpio_num, 1);
    #endif
    mutex_unlock(&wk2xxxs_reg_lock);
    return status;
        
}
/*
* This function write wk2xxx of fifo:
*/
static int wk2xxx_write_fifo(struct spi_device *spi,uint8_t port,uint8_t fifolen,uint8_t *dat)
{
    struct spi_message msg;
    int status,i;
    uint8_t recive_fifo_data[MAX_RFCOUNT_SIZE+1]={0};
    uint8_t transmit_fifo_data[MAX_RFCOUNT_SIZE+1]={0};
    struct spi_transfer index_xfer = {
            .len            = fifolen+1,
            .speed_hz	= wk2xxx_spi_speed,
    }; 
	if(!(fifolen>0)){
		printk(KERN_ERR "%s,fifolen error,fifolen:%d!!\n", __func__,fifolen);
		return 1;
	}
    mutex_lock(&wk2xxxs_reg_lock);
    #ifdef WK_CSGPIO_FUNCTION 
    gpio_set_value(cs_gpio_num, 0);
    #endif
    spi_message_init(&msg);
    /* register index */
    transmit_fifo_data[0] = ((port-1)<<4)|0x80;
    for(i=0;i<fifolen;i++){
        transmit_fifo_data[i+1]=*(dat+i);
    }
    index_xfer.tx_buf = transmit_fifo_data;
    index_xfer.rx_buf =(void *) recive_fifo_data;
    spi_message_add_tail(&index_xfer, &msg);
    status = spi_sync(spi, &msg);
    #ifdef WK_CSGPIO_FUNCTION 
    gpio_set_value(cs_gpio_num, 1);
    #endif
    mutex_unlock(&wk2xxxs_reg_lock);
    return status;        
}

static void conf_wk2xxx_subport(struct uart_port *port);
static void wk2xxx_stop_tx(struct uart_port *port);
static u_int wk2xxx_tx_empty(struct uart_port *port);


static void wk2xxx_rx_chars(struct uart_port *port)
{   
    //struct wk2xxx_port *s = container_of(port,struct wk2xxx_port,port);
    struct wk2xxx_port *s = dev_get_drvdata(port->dev);
    struct wk2xxx_one *one = to_wk2xxx_one(port, port);
    uint8_t fsr,rx_dat[256]={0};
    uint8_t  rfcnt=0,rfcnt2=0;
    unsigned int flg, status = 0,rx_count=0;
    int rx_num=0,rxlen=0;
	int len_rfcnt,len_limit,len_p=0;
	len_limit=SPI_LEN_LIMIT;
    #ifdef _DEBUG_WK_FUNCTION
		printk(KERN_ALERT "%s!!-port:%ld;--in--\n", __func__,one->port.iobase);
    #endif
    wk2xxx_read_slave_reg(s->spi_wk,one->port.iobase,WK2XXX_FSR_REG,&fsr);
    if (fsr& WK2XXX_FSR_RDAT_BIT){
        wk2xxx_read_slave_reg(s->spi_wk,one->port.iobase,WK2XXX_RFCNT_REG,&rfcnt);
        if(rfcnt==0){
            wk2xxx_read_slave_reg(s->spi_wk,one->port.iobase,WK2XXX_RFCNT_REG,&rfcnt);   
        }
		wk2xxx_read_slave_reg(s->spi_wk,one->port.iobase,WK2XXX_RFCNT_REG,&rfcnt2);
        if(rfcnt2==0){
            wk2xxx_read_slave_reg(s->spi_wk,one->port.iobase,WK2XXX_RFCNT_REG,&rfcnt2);
        }
        rfcnt=(rfcnt2>=rfcnt)?rfcnt:rfcnt2;
	    rxlen=(rfcnt==0)?256:rfcnt;	
    }
    #ifdef _DEBUG_WK_RX
     printk(KERN_ALERT "rx_chars()-port:%lx--fsr:0x%x--rxlen:%d--\n",one->port.iobase,fsr,rxlen);
    #endif
    flg = TTY_NORMAL;
        #ifdef WK_FIFO_FUNCTION  
	            len_rfcnt=rxlen;
	            while(len_rfcnt){
			        if(len_rfcnt>len_limit){
				        wk2xxx_read_fifo(s->spi_wk,one->port.iobase,len_limit,rx_dat+len_p);
				        len_rfcnt=len_rfcnt-len_limit;
				        len_p=len_p+len_limit;
			        }else{
				        wk2xxx_read_fifo(s->spi_wk,one->port.iobase,len_rfcnt,rx_dat+len_p);//
				        len_rfcnt=0;
			        }
	            }
        #else
	        for(rx_num=0;rx_num<rxlen;rx_num++){
		        wk2xxx_read_slave_reg(s->spi_wk,one->port.iobase,WK2XXX_FDAT_REG,&rx_dat[rx_num]);
		    }
        #endif

            one->port.icount.rx+=rxlen;
            for(rx_num=0;rx_num<rxlen;rx_num++){

                if(fsr&WK2XXX_FSR_ERR_MASK){ 
                    fsr &= WK2XXX_FSR_ERR_MASK ;
                        if (fsr&(WK2XXX_FSR_RFOE_BIT |WK2XXX_FSR_RFBI_BIT|WK2XXX_FSR_RFFE_BIT|WK2XXX_FSR_RFPE_BIT)){
                            if(fsr & WK2XXX_FSR_RFPE_BIT){
                                one->port.icount.parity++;
                                status |= WK2XXX_STATUS_PE;
                                flg = TTY_PARITY;
                            }

                            if (fsr & WK2XXX_FSR_RFFE_BIT){
                                one->port.icount.frame++;
                                status |= WK2XXX_STATUS_FE;
                                flg = TTY_FRAME;
                            }

                            if(fsr & WK2XXX_FSR_RFOE_BIT){
                                one->port.icount.overrun++;
                                status |= WK2XXX_STATUS_OE;
                                flg = TTY_OVERRUN;
                            }
                            if(fsr & WK2XXX_FSR_RFBI_BIT){
                                one->port.icount.brk++;
                                status |= WK2XXX_STATUS_BRK;
                                flg = TTY_BREAK;
                            }        
                        }
                } 
                if (uart_handle_sysrq_char(port,rx_dat[rx_num]))
                    continue;//
                #ifdef _DEBUG_WK_RX
                    printk(KERN_ALERT "rx_chars:0x%x----\n",rx_dat[rx_num]);
                #endif
                uart_insert_char(port, status, WK2XXX_STATUS_OE, rx_dat[rx_num], flg);
                rx_count++;
            }
            if(rx_count > 0){
  		        #ifdef _DEBUG_WK_RX
                    printk(KERN_ALERT  "push buffer tty flip port = :%lx count =:%d\n",one->port.iobase,rx_count);
  		        #endif
                tty_flip_buffer_push(&port->state->port);
                rx_count = 0;
            }
    #ifdef _DEBUG_WK_FUNCTION
	    printk(KERN_ALERT "%s!!-port:%ld;--exit--\n", __func__,one->port.iobase);
    #endif 	
}

static void wk2xxx_tx_chars(struct uart_port *port)
{   
    struct wk2xxx_port *s = dev_get_drvdata(port->dev);
    //struct wk2xxx_port *s = container_of(port,struct wk2xxx_port,port);
    struct wk2xxx_one *one = to_wk2xxx_one(port, port);
    uint8_t fsr,tfcnt,dat[1],txbuf[256]={0};
    int count,tx_count,i;
	int len_tfcnt,len_limit,len_p=0;
	len_limit=SPI_LEN_LIMIT;
	#ifdef _DEBUG_WK_FUNCTION
        printk(KERN_ALERT "%s!!-port:%ld;--in--\n", __func__,one->port.iobase);
	#endif
    if (one->port.x_char) {   
        #ifdef _DEBUG_WK_TX
            printk(KERN_ALERT "wk2xxx_tx_chars   one->port.x_char:%x,port = %ld\n",one->port.x_char,one->port.iobase);
       	#endif
        wk2xxx_write_slave_reg(s->spi_wk,one->port.iobase,WK2XXX_FDAT_REG,one->port.x_char);
        one->port.icount.tx++;
        one->port.x_char = 0;
        goto out;
    }

    if(uart_circ_empty(&one->port.state->xmit) || uart_tx_stopped(&one->port)){
        goto out;
    }

    wk2xxx_read_slave_reg(s->spi_wk,one->port.iobase,WK2XXX_FSR_REG,&fsr);
    wk2xxx_read_slave_reg(s->spi_wk,one->port.iobase,WK2XXX_TFCNT_REG,&tfcnt); 
	#ifdef _DEBUG_WK_TX
		printk(KERN_ALERT "wk2xxx_tx_chars   fsr:0x%x,tfcnt:0x%x,port = %ld\n",fsr,tfcnt,one->port.iobase);
	#endif
    if(tfcnt==0){
	    tx_count=(fsr & WK2XXX_FSR_TFULL_BIT)?0:256;
        #ifdef _DEBUG_WK_TX
            printk(KERN_ALERT "wk2xxx_tx_chars2   tx_count:%x,port = %ld\n",tx_count,one->port.iobase);
		#endif
    }else{
		tx_count=256-tfcnt;
	    #ifdef _DEBUG_WK_TX
            printk(KERN_ALERT "wk2xxx_tx_chars2   tx_count:%x,port = %ld\n",tx_count,one->port.iobase);
		#endif 
    } 
    if(tx_count>200){
        tx_count=200;
    } 
	count = tx_count;
	i=0;
	while(count){
  		if(uart_circ_empty(&one->port.state->xmit))
     		break;
	   	txbuf[i]=one->port.state->xmit.buf[one->port.state->xmit.tail];
	   	one->port.state->xmit.tail = (one->port.state->xmit.tail + 1) & (UART_XMIT_SIZE - 1);
	   	one->port.icount.tx++;
	   	i++;
		count=count-1;
        #ifdef _DEBUG_WK_TX
            printk(KERN_ALERT "tx_chars:0x%x--\n",txbuf[i-1]);
        #endif
    };

    #ifdef WK_FIFO_FUNCTION 
	len_tfcnt=i;    
	while(len_tfcnt){
		if(len_tfcnt>len_limit){
            wk2xxx_write_fifo(s->spi_wk,one->port.iobase,len_limit,txbuf+len_p);	
			len_p=len_p+len_limit;
			len_tfcnt=len_tfcnt-len_limit;
        }else{
            wk2xxx_write_fifo(s->spi_wk,one->port.iobase,len_tfcnt,txbuf+len_p);
			len_p=len_p+len_tfcnt;
			len_tfcnt=0;
		}
	}
    #else
	    for(count=0;count<i;count++){
            wk2xxx_write_slave_reg(s->spi_wk,one->port.iobase,WK2XXX_FDAT_REG,txbuf[count]);	
        }
    #endif
    out:wk2xxx_read_slave_reg(s->spi_wk,one->port.iobase,WK2XXX_FSR_REG,dat);
        fsr = dat[0];
    #ifdef _DEBUG_WK_VALUE
        printk(KERN_ALERT "%s!!-port:%ld;--FSR:0X%X--\n", __func__,one->port.iobase,fsr);
	#endif
    if(((fsr&WK2XXX_FSR_TDAT_BIT)==0)&&((fsr&WK2XXX_FSR_TBUSY_BIT)==0)){
        if (uart_circ_chars_pending(&one->port.state->xmit) < WAKEUP_CHARS){
            uart_write_wakeup(&one->port); 
        }
        if (uart_circ_empty(&one->port.state->xmit)){
            wk2xxx_stop_tx(&one->port);
        }
    }
    #ifdef _DEBUG_WK_FUNCTION
        printk(KERN_ALERT "%s!!-port:%ld;--exit--\n", __func__,one->port.iobase);
    #endif
}






static void wk2xxx_port_irq(struct wk2xxx_port *s, int portno)//
{   
    //struct wk2xxx_port *s = container_of(port,struct wk2xxx_port,port);
    struct wk2xxx_one *one = &s->p[portno];
    unsigned int  pass_counter = 0;
    uint8_t sifr,sier;

    #ifdef _DEBUG_WK_IRQ
        uint8_t gier,sifr0,sifr1,sifr2,sifr3,sier1,sier0,sier2,sier3,gifr;

    #endif

    #ifdef _DEBUG_WK_FUNCTION
        printk(KERN_ALERT "%s!!-port:%ld;--in--\n", __func__,one->port.iobase);
    #endif


    #ifdef _DEBUG_WK_IRQ
        wk2xxx_read_global_reg(s->spi_wk,WK2XXX_GIFR_REG ,&gifr);
        wk2xxx_read_global_reg(s->spi_wk,WK2XXX_GIER_REG ,&gier);
        wk2xxx_read_slave_reg(s->spi_wk,1,WK2XXX_SIFR_REG,&sifr0);
        wk2xxx_read_slave_reg(s->spi_wk,2,WK2XXX_SIFR_REG,&sifr1);
        wk2xxx_read_slave_reg(s->spi_wk,3,WK2XXX_SIFR_REG,&sifr2);
        wk2xxx_read_slave_reg(s->spi_wk,4,WK2XXX_SIFR_REG,&sifr3);
        wk2xxx_read_slave_reg(s->spi_wk,1,WK2XXX_SIER_REG,&sier0);
        wk2xxx_read_slave_reg(s->spi_wk,2,WK2XXX_SIER_REG,&sier1);
        wk2xxx_read_slave_reg(s->spi_wk,3,WK2XXX_SIER_REG,&sier2);
        wk2xxx_read_slave_reg(s->spi_wk,4,WK2XXX_SIER_REG,&sier3);
        printk(KERN_ALERT "irq_app....gifr:%x  gier:%x  sier1:%x  sier2:%x sier3:%x sier4:%x   sifr1:%x sifr2:%x sifr3:%x sifr4:%x \n",gifr,gier,sier0,sier1,sier2,sier3,sifr0,sifr1,sifr2,sifr3);
    #endif           
    wk2xxx_read_slave_reg(s->spi_wk,one->port.iobase,WK2XXX_SIFR_REG,&sifr);
    wk2xxx_read_slave_reg(s->spi_wk,one->port.iobase,WK2XXX_SIER_REG,&sier);
    #ifdef _DEBUG_WK_IRQ
        printk(KERN_ALERT "irq_app....port:%ld......sifr:%x sier:%x \n",one->port.iobase,sifr,sier);
    #endif

    do {
        if ((sifr&WK2XXX_SIFR_RFTRIG_INT_BIT)||(sifr&WK2XXX_SIFR_RXOVT_INT_BIT)){
            wk2xxx_rx_chars(&one->port);
        }
        
        if ((sifr & WK2XXX_SIFR_TFTRIG_INT_BIT)&&(sier & WK2XXX_SIER_TFTRIG_IEN_BIT)){
            wk2xxx_tx_chars(&one->port);
            return;
        }
        if (pass_counter++ > WK2XXX_ISR_PASS_LIMIT)
            break;
        wk2xxx_read_slave_reg(s->spi_wk,one->port.iobase,WK2XXX_SIFR_REG,&sifr);
        wk2xxx_read_slave_reg(s->spi_wk,one->port.iobase,WK2XXX_SIER_REG,&sier);
        #ifdef _DEBUG_WK_VALUE
            printk(KERN_ALERT "irq_app...........rx............tx  sifr:%x sier:%x port:%ld\n",sifr,sier,one->port.iobase);
        #endif
    } while ((sifr&(WK2XXX_SIFR_RXOVT_INT_BIT|WK2XXX_SIFR_RFTRIG_INT_BIT))||((sifr & WK2XXX_SIFR_TFTRIG_INT_BIT)&&(sier & WK2XXX_SIER_TFTRIG_IEN_BIT)));
    #ifdef _DEBUG_WK_FUNCTION
        printk(KERN_ALERT "%s!!-port:%ld;--exit--\n", __func__,one->port.iobase);
    #endif
}


static void wk2xxx_ist(struct kthread_work *ws)
{  
    struct wk2xxx_port *s = container_of(ws, struct wk2xxx_port, irq_work);

    uint8_t gifr,i;
    #ifdef _DEBUG_WK_FUNCTION
        printk(KERN_ALERT "%s!!---in--\n", __func__);
    #endif

    wk2xxx_read_global_reg(s->spi_wk,WK2XXX_GIFR_REG ,&gifr);
    while(1){

        for (i = 0; i < s->devtype->nr_uart; ++i){
            if(gifr&(0x01<<i)){
               wk2xxx_port_irq(s,i);
            }
        }

        wk2xxx_read_global_reg(s->spi_wk,WK2XXX_GIFR_REG ,&gifr);
        if(!(gifr&0x0f)){
            break;
        }
        
			
    }

    #ifdef _DEBUG_WK_FUNCTION
        printk(KERN_ALERT "%s!!---exit--\n", __func__);
    #endif

}

static irqreturn_t wk2xxx_irq(int irq, void *dev_id)//
{
    struct wk2xxx_port *s = (struct wk2xxx_port *)dev_id;
    bool ret;
    #ifdef _DEBUG_WK_FUNCTION
        printk(KERN_ALERT "%s!!---in--\n", __func__);
    #endif  
#ifdef WK_WORK_KTHREAD
    ret=kthread_queue_work(&s->kworker, &s->irq_work); 
#else
    ret=queue_kthread_work(&s->kworker, &s->irq_work);  
#endif

    #ifdef _DEBUG_WK_FUNCTION
        printk(KERN_ALERT "%s!!ret:%d---exit--\n", __func__,ret);
    #endif
    return IRQ_HANDLED;
}

/*
 *   Return TIOCSER_TEMT when transmitter is not busy.
 */

static u_int wk2xxx_tx_empty(struct uart_port *port)// or query the tx fifo is not empty?
{
    uint8_t fsr=0;
    // struct wk2xxx_port *s = container_of(port,struct wk2xxx_port,port);
    struct wk2xxx_port *s = dev_get_drvdata(port->dev);
    struct wk2xxx_one *one = to_wk2xxx_one(port, port);
    #ifdef _DEBUG_WK_FUNCTION
	    printk(KERN_ALERT "%s!!-port:%ld;--in--\n", __func__,one->port.iobase);
    #endif
	mutex_lock(&wk2xxxs_lock);
  
    wk2xxx_read_slave_reg(s->spi_wk,one->port.iobase,WK2XXX_FSR_REG,&fsr);
    while((fsr & WK2XXX_FSR_TDAT_BIT)|(fsr&WK2XXX_FSR_TBUSY_BIT)){
	   	wk2xxx_read_slave_reg(s->spi_wk,one->port.iobase,WK2XXX_FSR_REG,&fsr);
	}
    s->tx_empty=((fsr&(WK2XXX_FSR_TBUSY_BIT|WK2XXX_FSR_TDAT_BIT))==0) ? TIOCSER_TEMT : 0;
	mutex_unlock(&wk2xxxs_lock);
      
    #ifdef _DEBUG_WK_FUNCTION
       printk(KERN_ALERT "%s!!-port:%ld;tx_empty:0x%x,fsr:0x%x--exit--\n", __func__,one->port.iobase,s->tx_empty,fsr);
    #endif
    return s->tx_empty;
}

static void wk2xxx_set_mctrl(struct uart_port *port, u_int mctrl)
{
    #ifdef _DEBUG_WK_FUNCTION
        printk(KERN_ALERT "%s!!---in--\n", __func__);
    #endif
}
static u_int wk2xxx_get_mctrl(struct uart_port *port)
{       
    #ifdef _DEBUG_WK_FUNCTION
        printk(KERN_ALERT "%s!!---in--\n", __func__);
    #endif
    return TIOCM_CTS | TIOCM_DSR | TIOCM_CAR;
}


static void wk2xxx_stop_tx(struct uart_port *port)//
{

    uint8_t sier;
    struct wk2xxx_one *one = to_wk2xxx_one(port, port);
	//struct wk2xxx_port *s = container_of(port,struct wk2xxx_port,port);
    struct wk2xxx_port *s = dev_get_drvdata(port->dev);
	#ifdef _DEBUG_WK_FUNCTION
    printk(KERN_ALERT "%s!!-port:%ld;--in--\n", __func__,one->port.iobase);
	#endif

    mutex_lock(&wk2xxxs_lock);
	wk2xxx_read_slave_reg(s->spi_wk,one->port.iobase,WK2XXX_SIER_REG,&sier);
    sier&=~WK2XXX_SIER_TFTRIG_IEN_BIT;
    wk2xxx_write_slave_reg(s->spi_wk,one->port.iobase,WK2XXX_SIER_REG,sier);
	mutex_unlock(&wk2xxxs_lock); 
	#ifdef _DEBUG_WK_FUNCTION
    printk(KERN_ALERT "%s!!-port:%ld;--exit--\n", __func__,one->port.iobase);
	#endif
  
}

static void wk2xxx_start_tx_proc(struct kthread_work *ws)
{
    struct wk2xxx_one *one = to_wk2xxx_one(ws, start_tx_work);
    struct uart_port *port = &(to_wk2xxx_one(ws, start_tx_work)->port);
    struct wk2xxx_port *s = dev_get_drvdata(port->dev);

    uint8_t rx;
	#ifdef _DEBUG_WK_FUNCTION
        printk(KERN_ALERT "%s!!-port:%ld;--in--\n", __func__,one->port.iobase);
	#endif
    mutex_lock(&wk2xxxs_lock);
    wk2xxx_read_slave_reg(s->spi_wk,one->port.iobase,WK2XXX_SIER_REG,&rx);
    rx |= WK2XXX_SIER_TFTRIG_IEN_BIT|WK2XXX_SIER_RFTRIG_IEN_BIT|WK2XXX_SIER_RXOUT_IEN_BIT; 
    wk2xxx_write_slave_reg(s->spi_wk,one->port.iobase,WK2XXX_SIER_REG,rx);
    mutex_unlock(&wk2xxxs_lock); 
}

/*
 *  * 
*/
static void wk2xxx_start_tx(struct uart_port *port)
{
    struct wk2xxx_port *s = dev_get_drvdata(port->dev);
    struct wk2xxx_one *one = to_wk2xxx_one(port, port);
    bool ret;
	#ifdef _DEBUG_WK_FUNCTION
        printk(KERN_ALERT "%s!!-port:%ld;--in--\n", __func__,one->port.iobase);
	#endif
#ifdef WK_WORK_KTHREAD
    ret=kthread_queue_work(&s->kworker, &one->start_tx_work);
#else
    ret=queue_kthread_work(&s->kworker, &one->start_tx_work);
#endif

    #ifdef _DEBUG_WK_FUNCTION
        printk(KERN_ALERT "%s!!-port:%ld;ret=%d--exit--\n", __func__,one->port.iobase,ret);
	#endif



}


static void wk2xxx_stop_rx_proc(struct kthread_work *ws)
{   
    struct wk2xxx_one *one = to_wk2xxx_one(ws, stop_rx_work);
    struct uart_port *port = &(to_wk2xxx_one(ws, stop_rx_work)->port);
    struct wk2xxx_port *s = dev_get_drvdata(port->dev);
    uint8_t rx;  
    #ifdef _DEBUG_WK_FUNCTION
        printk(KERN_ALERT "%s!!-port:%ld;--in--\n", __func__,one->port.iobase);
	#endif  
    mutex_lock(&wk2xxxs_lock); 
    wk2xxx_read_slave_reg(s->spi_wk,one->port.iobase,WK2XXX_SIER_REG,&rx);
    rx &=~WK2XXX_SIER_RFTRIG_IEN_BIT;
    rx &=~WK2XXX_SIER_RXOUT_IEN_BIT;
    wk2xxx_write_slave_reg(s->spi_wk,one->port.iobase,WK2XXX_SIER_REG,rx);

    wk2xxx_read_slave_reg(s->spi_wk,one->port.iobase,WK2XXX_SCR_REG,&rx);
    rx &=~WK2XXX_SCR_RXEN_BIT;
    wk2xxx_write_slave_reg(s->spi_wk,one->port.iobase,WK2XXX_SCR_REG,rx);
    mutex_unlock(&wk2xxxs_lock); 

    

}

static void wk2xxx_stop_rx(struct uart_port *port)
{
    struct wk2xxx_port *s = dev_get_drvdata(port->dev);
    struct wk2xxx_one *one = to_wk2xxx_one(port, port);
    bool ret;
	#ifdef _DEBUG_WK_FUNCTION
        printk(KERN_ALERT "%s!!-port:%ld;--in--\n", __func__,one->port.iobase);
       
	#endif
#ifdef WK_WORK_KTHREAD
    ret=kthread_queue_work(&s->kworker, &one->stop_rx_work);
#else
   ret=queue_kthread_work(&s->kworker, &one->stop_rx_work);
#endif

	#ifdef _DEBUG_WK_FUNCTION
        printk(KERN_ALERT "%s!!-port:%ld;ret:%d--exit--\n", __func__,one->port.iobase,ret);
	#endif
}


/*
 *  * No modem control lines
 *   */
static void wk2xxx_enable_ms(struct uart_port *port)    //nothing
{
	#ifdef _DEBUG_WK_FUNCTION
        printk(KERN_ALERT "%s!!---in--\n", __func__);
	#endif


}
/*
 *  * Interrupts always disabled.
*/   
static void wk2xxx_break_ctl(struct uart_port *port, int break_state)
{
	#ifdef _DEBUG_WK_FUNCTION
        printk(KERN_ALERT "%s!!---in--\n", __func__);
	#endif
}


static int wk2xxx_startup(struct uart_port *port)//i
{
    uint8_t gena,grst,gier,sier,scr,dat[1];
    struct wk2xxx_port *s = dev_get_drvdata(port->dev);
    struct wk2xxx_one *one = to_wk2xxx_one(port, port);

	#ifdef _DEBUG_WK_FUNCTION
        printk(KERN_ALERT "%s!!-port:%ld;--in--\n", __func__,one->port.iobase);
        printk(KERN_ALERT "wk2xxx_start(iobase) port1:%ld,port2:%ld,port3:%ld,port4:%ld\n",s->p[0].port.iobase,s->p[1].port.iobase,s->p[2].port.iobase,s->p[3].port.iobase );
        printk(KERN_ALERT "wk2xxx_start(iobase) line1:%d,line2:%d,line3:%d,line4:%d\n",s->p[0].line,s->p[1].line,s->p[2].line,s->p[3].line );  
	#endif

    mutex_lock(&wk2xxxs_global_lock);  
    wk2xxx_read_global_reg(s->spi_wk,WK2XXX_GENA_REG,dat);
    gena=dat[0];
    switch (one->port.iobase){
        case 1:
            gena|=WK2XXX_GENA_UT1EN_BIT;
            wk2xxx_write_global_reg(s->spi_wk,WK2XXX_GENA_REG,gena);
            break;
        case 2:
            gena|=WK2XXX_GENA_UT2EN_BIT;
            wk2xxx_write_global_reg(s->spi_wk,WK2XXX_GENA_REG,gena);
            break;
        case 3:
            gena|=WK2XXX_GENA_UT3EN_BIT;
            wk2xxx_write_global_reg(s->spi_wk,WK2XXX_GENA_REG,gena);
            break;
        case 4:
            gena|=WK2XXX_GENA_UT4EN_BIT;
            wk2xxx_write_global_reg(s->spi_wk,WK2XXX_GENA_REG,gena);
            break;
        default:
		    printk(KERN_ALERT ":%s！！ bad iobase1: %d.\n", __func__,(uint8_t)one->port.iobase);
            break;
    }
 
    //wk2xxx_read_global_reg(s->spi_wk,WK2XXX_GRST_REG,dat);
    grst=0;
    switch (one->port.iobase){
        case 1:
            grst|=WK2XXX_GRST_UT1RST_BIT;
            wk2xxx_write_global_reg(s->spi_wk,WK2XXX_GRST_REG,grst);
            break;
        case 2:
            grst|=WK2XXX_GRST_UT2RST_BIT;
            wk2xxx_write_global_reg(s->spi_wk,WK2XXX_GRST_REG,grst);
            break;
        case 3:
            grst|=WK2XXX_GRST_UT3RST_BIT;
            wk2xxx_write_global_reg(s->spi_wk,WK2XXX_GRST_REG,grst);
            break;
        case 4:
            grst|=WK2XXX_GRST_UT4RST_BIT;
            wk2xxx_write_global_reg(s->spi_wk,WK2XXX_GRST_REG,grst);
            break;
        default:
            printk(KERN_ALERT ":%s！！ bad iobase2: %d.\n", __func__,(uint8_t)one->port.iobase);
            break;
    }

	//enable the sub port interrupt
	wk2xxx_read_global_reg(s->spi_wk,WK2XXX_GIER_REG,dat);
	gier = dat[0];		
	switch (one->port.iobase)
	{
		case 1:
			gier|=WK2XXX_GIER_UT1IE_BIT;
			wk2xxx_write_global_reg(s->spi_wk,WK2XXX_GIER_REG,gier);
			break;
		case 2:
			gier|=WK2XXX_GIER_UT2IE_BIT;
			wk2xxx_write_global_reg(s->spi_wk,WK2XXX_GIER_REG,gier);
			break;
		case 3:
			gier|=WK2XXX_GIER_UT3IE_BIT;
			wk2xxx_write_global_reg(s->spi_wk,WK2XXX_GIER_REG,gier);
			break;
		case 4:
			gier|=WK2XXX_GIER_UT4IE_BIT;
			wk2xxx_write_global_reg(s->spi_wk,WK2XXX_GIER_REG,gier);
			break;
		default:
			printk(KERN_ALERT ":%s！！bad iobase3: %d.\n",__func__, (uint8_t)one->port.iobase);
			break;
	}   
    wk2xxx_read_slave_reg(s->spi_wk,one->port.iobase,WK2XXX_SIER_REG,dat);
    sier = dat[0];
    sier &= ~WK2XXX_SIER_TFTRIG_IEN_BIT;
    sier |= WK2XXX_SIER_RFTRIG_IEN_BIT;
    sier |= WK2XXX_SIER_RXOUT_IEN_BIT;
    wk2xxx_write_slave_reg(s->spi_wk,one->port.iobase,WK2XXX_SIER_REG,sier);

    wk2xxx_read_slave_reg(s->spi_wk,one->port.iobase,WK2XXX_SCR_REG,dat);
    scr = dat[0] | WK2XXX_SCR_TXEN_BIT|WK2XXX_SCR_RXEN_BIT;
    wk2xxx_write_slave_reg(s->spi_wk,one->port.iobase,WK2XXX_SCR_REG,scr);

    //initiate the fifos
    wk2xxx_write_slave_reg(s->spi_wk,one->port.iobase,WK2XXX_FCR_REG,0xff);//initiate the fifos
    wk2xxx_write_slave_reg(s->spi_wk,one->port.iobase,WK2XXX_FCR_REG,0xfc);
    //set rx/tx interrupt 
    wk2xxx_write_slave_reg(s->spi_wk,one->port.iobase,WK2XXX_SPAGE_REG,1);  
    wk2xxx_write_slave_reg(s->spi_wk,one->port.iobase,WK2XXX_RFTL_REG,WK2XXX_RXFIFO_LEVEL);
    wk2xxx_write_slave_reg(s->spi_wk,one->port.iobase,WK2XXX_TFTL_REG,WK2XXX_TXFIFO_LEVEL);
    wk2xxx_write_slave_reg(s->spi_wk,one->port.iobase,WK2XXX_SPAGE_REG,0);  

	/*enable rs485*/
	#ifdef WK_RS485_FUNCTION
    wk2xxx_write_slave_reg(s->spi_wk,one->port.iobase,WK2XXX_RS485_REG,0X02);//default  high
	//wk2xxx_write_slave_reg(s->spi_wk,one->port.iobase,WK2XXX_RS485,0X03);//default low
	wk2xxx_write_slave_reg(s->spi_wk,one->port.iobase,WK2XXX_SPAGE_REG,0X01);
	wk2xxx_write_slave_reg(s->spi_wk,one->port.iobase,WK2XXX_RRSDLY_REG,0X10);
	wk2xxx_write_slave_reg(s->spi_wk,one->port.iobase,WK2XXX_SPAGE_REG,0X00);
	#endif
        /*****************************test**************************************/
    #ifdef _DEBUG_WK_TEST
        wk2xxx_read_global_reg(s->spi_wk,WK2XXX_GENA_REG,&gena);
		wk2xxx_read_global_reg(s->spi_wk,WK2XXX_GIER_REG,&gier);
		wk2xxx_read_slave_reg(s->spi_wk,one->port.iobase,WK2XXX_SIER_REG,&sier);
		wk2xxx_read_slave_reg(s->spi_wk,one->port.iobase,WK2XXX_SCR_REG,&scr);
		wk2xxx_read_slave_reg(s->spi_wk,one->port.iobase,WK2XXX_FCR_REG,dat);
		printk(KERN_ALERT "%s!!-port:%ld;gena:0x%x;gier:0x%x;sier:0x%x;scr:0x%x;fcr:0x%x----\n", __func__,one->port.iobase,gena,gier,sier,scr,dat[0]);	
	#endif
		/**********************************************************************/

    mutex_unlock(&wk2xxxs_global_lock);
    uart_circ_clear(&one->port.state->xmit);
    wk2xxx_enable_ms(&one->port);

    #ifdef _DEBUG_WK_FUNCTION
        printk(KERN_ALERT "%s!!-port:%ld;--exit--\n", __func__,one->port.iobase);
    #endif
    return 0;
}

static void wk2xxx_shutdown(struct uart_port *port)
{

    uint8_t gena,grst,gier,dat[1];
    //struct wk2xxx_port *s = container_of(port,struct wk2xxx_port,port);
    struct wk2xxx_port *s = dev_get_drvdata(port->dev);
    struct wk2xxx_one *one = to_wk2xxx_one(port, port);
    #ifdef _DEBUG_WK_FUNCTION
        printk(KERN_ALERT "%s!!-port:%ld;--in--\n", __func__,one->port.iobase);
    #endif

    mutex_lock(&wk2xxxs_global_lock);
    wk2xxx_read_global_reg(s->spi_wk,WK2XXX_GIER_REG,&gier);
    switch (one->port.iobase){
        case 1:
            gier&=~WK2XXX_GIER_UT1IE_BIT;
            wk2xxx_write_global_reg(s->spi_wk,WK2XXX_GIER_REG,gier);
            break;
        case 2:
            gier&=~WK2XXX_GIER_UT2IE_BIT;;
            wk2xxx_write_global_reg(s->spi_wk,WK2XXX_GIER_REG,gier);
            break;
        case 3:
            gier&=~WK2XXX_GIER_UT3IE_BIT;;
            wk2xxx_write_global_reg(s->spi_wk,WK2XXX_GIER_REG,gier);
            break;
        case 4:
            gier&=~WK2XXX_GIER_UT4IE_BIT;;
            wk2xxx_write_global_reg(s->spi_wk,WK2XXX_GIER_REG,gier);
            break;
        default:
            printk(KERN_ALERT "%s!! (GIER)bad iobase %d\n",__func__, (uint8_t)one->port.iobase);;
            break;
    }

    wk2xxx_write_slave_reg(s->spi_wk,one->port.iobase,WK2XXX_SIER_REG,0x0);
	mutex_unlock(&wk2xxxs_global_lock);

    #ifdef WK_WORK_KTHREAD
        kthread_flush_work(&one->start_tx_work);
        kthread_flush_work(&one->stop_rx_work);
        kthread_flush_work(&s->irq_work);
        //kthread_flush_worker(&s->kworker);
    #else
        flush_kthread_work(&one->start_tx_work);
        flush_kthread_work(&one->stop_rx_work);
        flush_kthread_work(&s->irq_work);
        //flush_kthread_worker(&s->kworker);
    #endif


    mutex_lock(&wk2xxxs_global_lock);
    wk2xxx_read_global_reg(s->spi_wk,WK2XXX_GRST_REG,dat);
    grst=dat[0];
    switch (one->port.iobase){
        case 1:
            grst|=WK2XXX_GRST_UT1RST_BIT;
            wk2xxx_write_global_reg(s->spi_wk,WK2XXX_GRST_REG,grst);
            break;
        case 2:
            grst|=WK2XXX_GRST_UT2RST_BIT;
            wk2xxx_write_global_reg(s->spi_wk,WK2XXX_GRST_REG,grst);
            break;
        case 3:
            grst|=WK2XXX_GRST_UT3RST_BIT;
            wk2xxx_write_global_reg(s->spi_wk,WK2XXX_GRST_REG,grst);
            break;
        case 4:
            grst|=WK2XXX_GRST_UT4RST_BIT;
            wk2xxx_write_global_reg(s->spi_wk,WK2XXX_GRST_REG,grst);
            break;
        default:
            printk(KERN_ALERT "%s!! bad iobase %d\n",__func__, (uint8_t)one->port.iobase);
            break;
    }

    wk2xxx_read_global_reg(s->spi_wk,WK2XXX_GENA_REG,dat);
    gena=dat[0];
    switch (one->port.iobase){
        case 1:
            gena&=~WK2XXX_GENA_UT1EN_BIT;
            wk2xxx_write_global_reg(s->spi_wk,WK2XXX_GENA_REG,gena);
            break;
        case 2:
            gena&=~WK2XXX_GENA_UT2EN_BIT;
            wk2xxx_write_global_reg(s->spi_wk,WK2XXX_GENA_REG,gena);
            break;
        case 3:
            gena&=~WK2XXX_GENA_UT3EN_BIT;
            wk2xxx_write_global_reg(s->spi_wk,WK2XXX_GENA_REG,gena);
            break;
        case 4:
            gena&=~WK2XXX_GENA_UT4EN_BIT;
            wk2xxx_write_global_reg(s->spi_wk,WK2XXX_GENA_REG,gena);
            break;
        default:
            printk(KERN_ALERT "%s!! bad iobase %d\n",__func__, (uint8_t)one->port.iobase);;
            break;
    }

	mutex_unlock(&wk2xxxs_global_lock);
    #ifdef _DEBUG_WK_FUNCTION
        printk(KERN_ALERT "%s!!-port:%ld;--exit--\n", __func__,one->port.iobase);
    #endif
}

static void conf_wk2xxx_subport(struct uart_port *port)//i
{   
    struct wk2xxx_port *s = dev_get_drvdata(port->dev);
    struct wk2xxx_one *one = to_wk2xxx_one(port, port);
	uint8_t sier=0,fwcr=0,lcr=0,scr=0,dat[1],baud0=0,baud1=0,pres=0,count=200;
    #ifdef _DEBUG_WK_FUNCTION
        printk(KERN_ALERT "%s!!-port:%ld;--in--\n", __func__,one->port.iobase);
    #endif
	lcr = one->new_lcr_reg;
	//scr = s->new_scr_reg;
	baud0=one->new_baud0_reg;
	baud1=one->new_baud1_reg;
	pres=one->new_pres_reg;
	fwcr=one->new_fwcr_reg;
    /* Disable Uart all interrupts */
	wk2xxx_read_slave_reg(s->spi_wk,one->port.iobase,WK2XXX_SIER_REG ,dat);
	sier = dat[0];
	wk2xxx_write_slave_reg(s->spi_wk,one->port.iobase,WK2XXX_SIER_REG,0X0);

	do{
        wk2xxx_read_slave_reg(s->spi_wk,one->port.iobase,WK2XXX_FSR_REG,dat);
	} while ((dat[0] & WK2XXX_FSR_TBUSY_BIT)&&(count--));
    // then, disable tx and rx
    wk2xxx_read_slave_reg(s->spi_wk,one->port.iobase,WK2XXX_SCR_REG,dat);
    scr = dat[0];
    wk2xxx_write_slave_reg(s->spi_wk,one->port.iobase,WK2XXX_SCR_REG,scr&(~(WK2XXX_SCR_RXEN_BIT|WK2XXX_SCR_TXEN_BIT)));
    // set the parity, stop bits and data size //
    wk2xxx_write_slave_reg(s->spi_wk,one->port.iobase,WK2XXX_LCR_REG,lcr);
    #ifdef WK_FlowControl_FUNCTION
	if(fwcr>0){  
        printk(KERN_ALERT "%s!!---Flow Control  fwcr=0x%X\n",__func__,fwcr);
        // Configure flow control levels 
		wk2xxx_write_slave_reg(s->spi_wk,one->port.iobase,WK2XXX_FWCR_REG,fwcr);
        //Flow control halt level 0XF0, resume level 0X80 
		wk2xxx_write_slave_reg(s->spi_wk,one->port.iobase,WK2XXX_SPAGE_REG ,1);
		wk2xxx_write_slave_reg(s->spi_wk,one->port.iobase,WK2XXX_FWTH_REG,0XF0);
		wk2xxx_write_slave_reg(s->spi_wk,one->port.iobase,WK2XXX_FWTL_REG,0X80);
		wk2xxx_write_slave_reg(s->spi_wk,one->port.iobase,WK2XXX_SPAGE_REG ,0);
    }
    #endif
    /* Setup baudrate generator */
    wk2xxx_write_slave_reg(s->spi_wk,one->port.iobase,WK2XXX_SPAGE_REG ,1);
    wk2xxx_write_slave_reg(s->spi_wk,one->port.iobase,WK2XXX_BAUD0_REG ,baud0);
    wk2xxx_write_slave_reg(s->spi_wk,one->port.iobase,WK2XXX_BAUD1_REG ,baud1);
    wk2xxx_write_slave_reg(s->spi_wk,one->port.iobase,WK2XXX_PRES_REG ,pres);
	#ifdef _DEBUG_WK_FUNCTION
        wk2xxx_read_slave_reg(s->spi_wk,one->port.iobase,WK2XXX_BAUD0_REG,&baud1);
        wk2xxx_read_slave_reg(s->spi_wk,one->port.iobase,WK2XXX_BAUD1_REG,&baud0);
        wk2xxx_read_slave_reg(s->spi_wk,one->port.iobase,WK2XXX_PRES_REG,&pres);
        printk(KERN_ALERT "%s!!---baud1:0x%x;baud0:0x%x;pres=0x%X.---\n", __func__,baud1,baud0,pres);
    #endif
    wk2xxx_write_slave_reg(s->spi_wk,one->port.iobase,WK2XXX_SPAGE_REG ,0);
    wk2xxx_write_slave_reg(s->spi_wk,one->port.iobase,WK2XXX_SCR_REG ,scr|(WK2XXX_SCR_RXEN_BIT|WK2XXX_SCR_TXEN_BIT));

    wk2xxx_write_slave_reg(s->spi_wk,one->port.iobase,WK2XXX_SIER_REG ,sier);

    #ifdef _DEBUG_WK_FUNCTION
        printk(KERN_ALERT "%s!!-port:%ld;--exit--\n", __func__,one->port.iobase);
    #endif
}


static void wk2xxx_termios( struct uart_port *port, struct ktermios *termios,struct ktermios *old)
{

    struct wk2xxx_one *one = to_wk2xxx_one(port, port);
	int baud = 0;
    uint32_t temp=0,freq=0;
	uint8_t lcr=0,fwcr=0,baud1=0,baud0=0,pres=0,bParityType=0;

	#ifdef _DEBUG_WK_FUNCTION
        printk(KERN_ALERT "%s!!-port:%ld;--in--\n", __func__,one->port.iobase);
        printk(KERN_ALERT "%s!!---c_cflag:0x%x,c_iflag:0x%x.\n",__func__,termios->c_cflag,termios->c_iflag);
	#endif
	baud1=0;
	baud0=0;
	pres=0;
	baud = tty_termios_baud_rate(termios);

    freq=one->port.uartclk;
    if(freq>=(baud*16)){
		temp=(freq)/(baud*16);
		temp=temp-1;
		baud1=(uint8_t)((temp>>8)&0xff);
		baud0=(uint8_t)(temp&0xff);
		temp=(((freq%(baud*16))*100)/(baud));
		pres=(temp+100/2)/100;
		printk(KERN_ALERT "%s!!---freq:%d,baudrate:%d\n",__func__,freq,baud);
		printk(KERN_ALERT "%s!!---baud1:%x,baud0:%x,pres:%x\n",__func__,baud1,baud0,pres);
	}else{
		printk(KERN_ALERT "the baud rate:%d is too high！ \n",baud);
	}
	tty_termios_encode_baud_rate(termios, baud, baud);

    lcr =0;
    if (termios->c_cflag & CSTOPB)
        lcr|=WK2XXX_LCR_STPL_BIT;//two  stop_bits
    else
        lcr&=~WK2XXX_LCR_STPL_BIT;//one  stop_bits

    
    bParityType = termios->c_cflag & PARENB ?(termios->c_cflag & PARODD ? 1 : 2) +(termios->c_cflag & CMSPAR ? 2 : 0) : 0;
    if (termios->c_cflag & PARENB) {
        lcr|=WK2XXX_LCR_PAEN_BIT;//enbale spa
        switch (bParityType) {
            case 0x01:                  //ODD
                lcr |= WK2XXX_LCR_PAM0_BIT;  
                lcr &= ~WK2XXX_LCR_PAM1_BIT;
                break;
            case 0x02:                 //EVEN
		        lcr |= WK2XXX_LCR_PAM1_BIT;
                lcr &= ~WK2XXX_LCR_PAM0_BIT;
                break;
            case 0x03:                 //MARK--1
		        lcr |= WK2XXX_LCR_PAM1_BIT|WK2XXX_LCR_PAM0_BIT;
                break;
            case 0x04:                //SPACE--0
		        lcr &= ~WK2XXX_LCR_PAM1_BIT;
                lcr &= ~WK2XXX_LCR_PAM0_BIT;
                break;
            default:
		        lcr &= ~WK2XXX_LCR_PAEN_BIT;
                break;
	    }
    }


	/* Set read status mask */
	port->read_status_mask = WK2XXX_LSR_OE_BIT;
	if (termios->c_iflag & INPCK)
		port->read_status_mask |= WK2XXX_LSR_PE_BIT |
					  WK2XXX_LSR_FE_BIT;
	if (termios->c_iflag & (BRKINT | PARMRK))
		port->read_status_mask |= WK2XXX_LSR_BI_BIT;

    	/* Set status ignore mask */
	port->ignore_status_mask = 0;
	if (termios->c_iflag & IGNBRK)
		port->ignore_status_mask |= WK2XXX_LSR_BI_BIT;
	if (!(termios->c_cflag & CREAD))
		port->ignore_status_mask |= WK2XXX_LSR_BRK_ERROR_MASK;

    #ifdef WK_FlowControl_FUNCTION
	/* Configure flow control */
    if (termios->c_cflag & CRTSCTS){
        fwcr=0X30;
        printk(KERN_ALERT "wk2xxx_termios(2)----port:%lx;lcr:0x%x;fwcr:0x%x---\n",one->port.iobase,lcr,fwcr);
    }
    
	if (termios->c_iflag & IXON){
        printk(KERN_ALERT "%s!!---c_cflag:0x%x,IXON:0x%x.\n",__func__,termios->c_cflag,IXON);

    }
	if (termios->c_iflag & IXOFF){
        printk(KERN_ALERT "%s!!---c_cflag:0x%x,IXOFF:0x%x.\n",__func__,termios->c_cflag,IXOFF);
 
    }
    #endif


	one->new_baud1_reg=baud1;
	one->new_baud0_reg=baud0;	
	one->new_pres_reg=pres;
	one->new_lcr_reg = lcr;
	one->new_fwcr_reg = fwcr;

    #ifdef _DEBUG_WK_VALUE
        printk(KERN_ALERT "wk2xxx_termios()----port:%lx;lcr:0x%x;fwcr:0x%x---\n",one->port.iobase,lcr,fwcr);
    #endif

	conf_wk2xxx_subport(&one->port);
	#ifdef _DEBUG_WK_FUNCTION
        printk(KERN_ALERT "%s!!-port:%ld;--exit--\n", __func__,one->port.iobase);
	#endif
}


static const char *wk2xxx_type(struct uart_port *port)
{

	#ifdef _DEBUG_WK_FUNCTION
        printk(KERN_ALERT "%s!!---in--\n", __func__);
	#endif
    return port->type == PORT_WK2XXX ? "wk2xxx" : NULL;
}


static void wk2xxx_release_port(struct uart_port *port)
{
	#ifdef _DEBUG_WK_FUNCTION
    printk(KERN_ALERT "%s!!---in--\n", __func__);
	#endif

}


static int wk2xxx_request_port(struct uart_port *port)
{
	#ifdef _DEBUG_WK_FUNCTION
    printk(KERN_ALERT "%s!!---in--\n", __func__);
	#endif
    return 0;
}


static void wk2xxx_config_port(struct uart_port *port, int flags)
{
   //struct wk2xxx_port *s = dev_get_drvdata(port->dev);
   struct wk2xxx_one *one = to_wk2xxx_one(port, port);
    #ifdef _DEBUG_WK_FUNCTION
        printk(KERN_ALERT "%s!!\n", __func__);
    #endif

    if (flags & UART_CONFIG_TYPE && wk2xxx_request_port(port) == 0)
        one->port.type = PORT_WK2XXX;
}


static int wk2xxx_verify_port(struct uart_port *port, struct serial_struct *ser)
{

    int ret = 0;
    #ifdef _DEBUG_WK_FUNCTION
        printk(KERN_ALERT "%s!!---in--\n", __func__);
	#endif
    if (ser->type != PORT_UNKNOWN && ser->type != PORT_WK2XXX)
        ret = -EINVAL;
    if (port->irq != ser->irq)
        ret = -EINVAL;
    if (ser->io_type != SERIAL_IO_PORT)
        ret = -EINVAL;
    //if (port->uartclk / 16 != ser->baud_base)
    //     ret = -EINVAL;
    if (port->iobase != ser->port)
        ret = -EINVAL;
    if (ser->hub6 != 0)
        ret = -EINVAL;
    return ret;
}


static struct uart_ops wk2xxx_pops = {
    tx_empty:       wk2xxx_tx_empty,
    set_mctrl:      wk2xxx_set_mctrl,
    get_mctrl:      wk2xxx_get_mctrl,
    stop_tx:        wk2xxx_stop_tx,
    start_tx:       wk2xxx_start_tx,
    stop_rx:        wk2xxx_stop_rx,
    enable_ms:      wk2xxx_enable_ms,
    break_ctl:      wk2xxx_break_ctl,
    startup:        wk2xxx_startup,
    shutdown:       wk2xxx_shutdown,
    set_termios:    wk2xxx_termios,
    type:           wk2xxx_type,
    release_port:   wk2xxx_release_port,
    request_port:   wk2xxx_request_port,
    config_port:    wk2xxx_config_port,
    verify_port:    wk2xxx_verify_port,

};
static struct uart_driver wk2xxx_uart_driver = {

    owner:                  THIS_MODULE,
    major:                  SERIAL_WK2XXX_MAJOR,

    driver_name:            "ttySWK",
    dev_name:               "ttysWK",

    minor:                  MINOR_START,
    nr:                     NR_PORTS,
    cons:                   NULL
};

static int uart_driver_registered;
static struct spi_driver wk2xxx_driver;

#ifdef WK_RSTGPIO_FUNCTION
static int wk2xxx_spi_rstgpio_parse_dt(struct device *dev,int *rst_gpio)
{

	enum of_gpio_flags rst_flags; 
	#ifdef _DEBUG_WK_FUNCTION
        printk(KERN_ALERT "%s!!--in--\n", __func__);
	#endif
	*rst_gpio = of_get_named_gpio_flags(dev->of_node, "reset_gpio", 0,&rst_flags);
    if (!gpio_is_valid(*rst_gpio)){
		printk(KERN_ERR"invalid wk2xxx_rst_gpio: %d\n", *rst_gpio);
		return -1;
    }
   
    if(	*rst_gpio){
		if (gpio_request(*rst_gpio , "rst_gpio")){
            printk(KERN_ERR"gpio_request failed!! rst_gpio: %d!\n",*rst_gpio);
		    gpio_free(*rst_gpio);
		    return  IRQ_NONE;
        }
    }
    gpio_direction_output(*rst_gpio,1);// output high
	printk(KERN_ERR"wk2xxx_rst_gpio: %d", *rst_gpio);
	#ifdef _DEBUG_WK_FUNCTION
        printk(KERN_ALERT "%s!!--exit--\n", __func__);
	#endif
	return 0;
}

#endif

#ifdef WK_CSGPIO_FUNCTION 

static int wk2xxx_spi_csgpio_parse_dt(struct device *dev,int *cs_gpio)
{

	enum of_gpio_flags cs_flags; 
	#ifdef _DEBUG_WK_FUNCTION
        printk(KERN_ALERT "%s!!--in--\n", __func__);
	#endif
	*cs_gpio = of_get_named_gpio_flags(dev->of_node, "cs-gpios", 0,&cs_flags);
    if (!gpio_is_valid(*cs_gpio)){
		printk(KERN_ERR"invalid wk2xxx_cs_gpio: %d\n", *cs_gpio);
		return -1;
    }
   
    if(	*cs_gpio){
		if (gpio_request(*cs_gpio , "cs-gpios")){
            printk(KERN_ERR"gpio_request failed!! cs_gpio: %d!\n",*cs_gpio);
		    gpio_free(*cs_gpio);
		    return  IRQ_NONE;
        }
    }
	printk(KERN_ERR"wk2xxx_cs_gpio: %d", *cs_gpio);
    gpio_direction_output(*cs_gpio,1);// output high
	#ifdef _DEBUG_WK_FUNCTION
        printk(KERN_ALERT "%s!!--exit--\n", __func__);
	#endif
	return 0;
}
#endif

static int wk2xxx_spi_irq_parse_dt(struct device *dev,int *irq_gpio)
{

	enum of_gpio_flags irq_flags;
    int irq; 
	#ifdef _DEBUG_WK_FUNCTION
        printk(KERN_ALERT "%s!!--in--\n", __func__);
	#endif
	*irq_gpio = of_get_named_gpio_flags(dev->of_node, "irq_gpio", 0,&irq_flags);
    if (!gpio_is_valid(*irq_gpio)){
		printk(KERN_ERR"invalid wk2xxx_irq_gpio: %d\n", *irq_gpio);
		return -1;
    }
   
    irq = gpio_to_irq(*irq_gpio);

    if(irq){
		if (gpio_request(*irq_gpio , "irq_gpio")){
            printk(KERN_ERR"gpio_request failed!! irq_gpio: %d!\n", irq);
		    gpio_free(*irq_gpio);
		    return  IRQ_NONE;
        }
    }else{
        printk(KERN_ERR"gpio_to_irq failed! irq: %d !\n", irq);
        return -ENODEV;
    }

	printk(KERN_ERR"wk2xxx_irq_gpio: %d, irq: %d", *irq_gpio, irq);
	#ifdef _DEBUG_WK_FUNCTION
        printk(KERN_ALERT "%s!!--exit--\n", __func__);
	#endif
	return irq;
}



static int wk2xxx_probe(struct spi_device *spi)
{
    const struct sched_param sched_param = { .sched_priority = MAX_RT_PRIO / 2 };
    //const struct sched_param sched_param = { .sched_priority = 100 / 2 };
    uint8_t i;
    int ret, irq;
    uint8_t dat[1];
	static struct wk2xxx_port *s;
    #ifdef _DEBUG_WK_FUNCTION
        printk(KERN_ALERT "%s!!--in--\n", __func__);
    #endif
    
	/* Setup SPI bus */
	spi->bits_per_word	= 8;
	/* only supports mode 0 on WK2124 */
	spi->mode		= spi->mode ? : SPI_MODE_0;
	spi->max_speed_hz	= spi->max_speed_hz ? : 10000000;
	ret = spi_setup(spi);
	if (ret)
		return ret;
    /* Alloc port structure */
	s = devm_kzalloc(&spi->dev, sizeof(*s) +sizeof(struct wk2xxx_one) * NR_PORTS,GFP_KERNEL);
	if (!s) {
	    printk(KERN_ALERT "wk2xxx_probe(devm_kzalloc) fail.\n");
		return -ENOMEM;
	}
	s->spi_wk = spi;
    s->devtype=&wk2124_devtype;
    dev_set_drvdata(&spi->dev, s);
    #ifdef WK_RSTGPIO_FUNCTION
     //Obtain the GPIO number of RST signal
    ret=wk2xxx_spi_rstgpio_parse_dt(&spi->dev,&s->rst_gpio_num);
    if(ret!=0){
        printk(KERN_ALERT "wk2xxx_probe(rst_gpio_num)  rst_gpio_num= 0x%d\n",s->rst_gpio_num);
        ret=s->rst_gpio_num;
        goto out_gpio;

    }
	/*reset wk2xxx*/
    mdelay(10);
    gpio_set_value(s->rst_gpio_num, 0); 	
	mdelay(10);
    gpio_set_value(s->rst_gpio_num, 1); 
    mdelay(10);
    #endif

    #ifdef WK_CSGPIO_FUNCTION 
    //Obtain the GPIO number of CS signal
    ret=wk2xxx_spi_csgpio_parse_dt(&spi->dev,&cs_gpio_num);
    if(ret!=0){
        printk(KERN_ALERT "wk2xxx_probe(cs_gpio)  cs_gpio_num = 0x%d\n",cs_gpio_num);
        ret=cs_gpio_num;
        goto out_gpio;

    }
    #endif

    //Obtain the IRQ signal GPIO number and interrupt number
    irq = wk2xxx_spi_irq_parse_dt(&spi->dev,&s->irq_gpio_num);
	if(irq<0){
        printk(KERN_ALERT "wk2xxx_probe(irq_gpio)  irq = 0x%d\n",irq);
        ret=irq;
        goto out_gpio;
	}
    s->irq_gpio = irq;

    /**********************test spi **************************************/

	do{
	    wk2xxx_read_global_reg(spi,WK2XXX_GENA_REG,dat);
        wk2xxx_read_global_reg(spi,WK2XXX_GENA_REG,dat);
		printk(KERN_ERR "wk2xxx_probe(0x30)  GENA = 0x%X\n",dat[0]);//GENA=0X30
		wk2xxx_write_global_reg(spi,WK2XXX_GENA_REG,0xf5);
		wk2xxx_read_global_reg(spi,WK2XXX_GENA_REG,dat);
		printk(KERN_ERR "wk2xxx_probe(0x35)  GENA = 0x%X\n",dat[0]);//GENA=0X35
		wk2xxx_write_global_reg(spi,WK2XXX_GENA_REG,0xff);
		wk2xxx_read_global_reg(spi,WK2XXX_GENA_REG,dat);
		printk(KERN_ERR "wk2xxx_probe(0x3f)  GENA = 0x%X\n",dat[0]);//GENA=0X3f
        wk2xxx_write_global_reg(spi,WK2XXX_GENA_REG,0xf0);
	}while(0);
    /*Get interrupt number*/
    wk2xxx_write_global_reg(spi,WK2XXX_GENA_REG,0x0);
    wk2xxx_read_global_reg(spi,WK2XXX_GENA_REG,dat);
    if((dat[0]&0xf0)!=0x30){ 
        printk(KERN_ALERT "wk2xxx_probe(0x30)  GENA = 0x%X\n",dat[0]);
        printk(KERN_ALERT "The spi failed to read the register.!!!!\n");
        ret=-1;
        goto out_gpio;
    }
    /*Init kthread_worker  and kthread_work */
    
#ifdef WK_WORK_KTHREAD
    kthread_init_worker(&(s->kworker));
	kthread_init_work(&s->irq_work, wk2xxx_ist);
#else
    init_kthread_worker(&(s->kworker));
	init_kthread_work(&s->irq_work, wk2xxx_ist);
#endif
	s->kworker_task = kthread_run(kthread_worker_fn, &s->kworker,
				      "wk2xxx");
	if (IS_ERR(s->kworker_task)) {
		ret = PTR_ERR(s->kworker_task);
		goto out_clk;
	}
	sched_setscheduler(s->kworker_task, SCHED_FIFO, &sched_param);

    /**/
    mutex_lock(&wk2xxxs_lock);
    if(!uart_driver_registered){
        uart_driver_registered = 1;
        ret = uart_register_driver(&wk2xxx_uart_driver);
        if (ret){
            printk(KERN_ERR "Couldn't register Wk2xxx uart driver\n");
            mutex_unlock(&wk2xxxs_lock);
            goto out_clk;
        }
    }

    printk(KERN_ALERT "wk2xxx_serial_init.\n");
    for(i =0;i<NR_PORTS;i++){
        s->p[i].line          = i;
        s->p[i].port.dev	= &spi->dev;
		s->p[i].port.line     = i;
		s->p[i].port.ops      = &wk2xxx_pops;
		s->p[i].port.uartclk  = WK_CRASTAL_CLK;
		s->p[i].port.fifosize = 256;
		s->p[i].port.iobase   = i+1;
		//s->p[i].port.irq      = irq;
		s->p[i].port.iotype   = SERIAL_IO_PORT;
        s->p[i].port.flags    = UPF_BOOT_AUTOCONF;
		//s->p[i].port.flags    = ASYNC_BOOT_AUTOCONF;
        //s->p[i].port.iotype   = UPIO_PORT;
		//s->p[i].port.flags    = UPF_FIXED_TYPE | UPF_LOW_LATENCY;
#ifdef WK_WORK_KTHREAD
        kthread_init_work(&s->p[i].start_tx_work,wk2xxx_start_tx_proc);
	    kthread_init_work(&s->p[i].stop_rx_work, wk2xxx_stop_rx_proc);
#else
        init_kthread_work(&s->p[i].start_tx_work, wk2xxx_start_tx_proc);
	    init_kthread_work(&s->p[i].stop_rx_work, wk2xxx_stop_rx_proc);
#endif
        /* Register uart port */
		ret = uart_add_one_port(&wk2xxx_uart_driver, &s->p[i].port);
       	if(ret<0){
            printk(KERN_ALERT "uart_add_one_port failed for line i:= %d with error %d\n",i,ret);
		    mutex_unlock(&wk2xxxs_lock);
            goto out_port;
        }

        printk(KERN_ALERT "uart_add_one_port：%ld. status= 0x%d\n",s->p[i].port.iobase,ret);
    }
 
    mutex_unlock(&wk2xxxs_lock);

   /* Setup interrupt */
	ret = devm_request_irq(&spi->dev, irq, wk2xxx_irq,IRQF_TRIGGER_FALLING, dev_name(&spi->dev), s);
  
	if (!ret){
        printk(KERN_ALERT "devm_request_irq success. ret=%d.\n",ret);
        return 0;
    }
   
out_port:
	for (i=0; i<NR_PORTS; i++) {
        printk(KERN_ALERT "uart_remove_one_port：%ld. status= 0x%d\n",s->p[i].port.iobase,ret);
		uart_remove_one_port(&wk2xxx_uart_driver, &s->p[i].port);
	}
out_clk:
    kthread_stop(s->kworker_task); 
out_gpio:
    if(s->irq_gpio_num>0){
        printk(KERN_ALERT "gpio_free(s->irq_gpio_num)= 0x%d,ret=0x%d\n",s->irq_gpio_num,ret);
        gpio_free(s->irq_gpio_num);
        s->irq_gpio_num=0;
    }
    if(s->rst_gpio_num>0){
        printk(KERN_ALERT "gpio_free(s->rst_gpio_num)= 0x%d,ret=0x%d\n",s->rst_gpio_num,ret);
        gpio_free(s->rst_gpio_num); 
        s->rst_gpio_num=0; 
    }
    #ifdef WK_CSGPIO_FUNCTION 
    if(cs_gpio_num>0){
        printk(KERN_ALERT "gpio_free(cs_gpio_num)= 0x%d,ret=0x%d\n",cs_gpio_num,ret);
        gpio_free(cs_gpio_num);  
        cs_gpio_num=0;
    }  
    #endif
	return ret;
}


static int wk2xxx_remove(struct spi_device *spi)
{

    int i;
    struct wk2xxx_port *s = dev_get_drvdata(&spi->dev);
	#ifdef _DEBUG_WK_FUNCTION
        printk(KERN_ALERT "%s!!--in--\n", __func__);
	#endif

    mutex_lock(&wk2xxxs_lock);
    for(i =0;i<NR_PORTS;i++){
        uart_remove_one_port(&wk2xxx_uart_driver, &s->p[i].port);
        printk(KERN_ALERT "%s!--uart_remove_one_port：%d.\n", __func__,i);
    }
#ifdef WK_WORK_KTHREAD
    kthread_flush_worker(&s->kworker);
#else
    flush_kthread_worker(&s->kworker);
#endif
	kthread_stop(s->kworker_task);
    /*
    if (s->irq_gpio){    
        free_irq(s->irq_gpio, s);
        printk(KERN_ALERT "%s!--,free_irq(s->irq_gpio, s);\n", __func__);
    }
    */
    if(s->irq_gpio_num>0){
        gpio_free(s->irq_gpio_num);
        printk(KERN_ALERT "%s!--,gpio_free(s->irq_gpio_num);\n", __func__);
    }
    if(s->rst_gpio_num>0){
        gpio_free(s->rst_gpio_num);
        printk(KERN_ALERT "%s!--,gpio_free(s->rst_gpio_num);\n", __func__);  
    }

    #ifdef WK_CSGPIO_FUNCTION 
    if(cs_gpio_num>0){
        gpio_free(cs_gpio_num); 
        printk(KERN_ALERT "%s!--,gpio_free(cs_gpio_num); ;\n", __func__); 
    } 
    #endif

    printk( KERN_ERR"removing wk2xxx_uart_driver\n");
    uart_unregister_driver(&wk2xxx_uart_driver);
    mutex_unlock(&wk2xxxs_lock);
    devm_kfree(&spi->dev,s);
    #ifdef _DEBUG_WK_FUNCTION
        printk(KERN_ALERT "%s!!--exit--\n", __func__);
	#endif
    return 0;
}

static const struct of_device_id wkmic_spi_dt_match[] = {
        //{ .compatible = "wkmic,wk2124_spi",  },
        { .compatible = "wkmic,wk2xxx_spi", },
		{ },
};

MODULE_DEVICE_TABLE(of, wkmic_spi_dt_match);

static struct spi_driver wk2xxx_driver = {
        .driver = {
                .name           = "wk2xxxspi1",
                .bus            = &spi_bus_type,
                .owner          = THIS_MODULE,
		        .of_match_table = of_match_ptr(wkmic_spi_dt_match),
        },

        .probe          = wk2xxx_probe,
        .remove         = wk2xxx_remove,
};


static int __init wk2xxx_init(void)
{

    int ret;
    printk(KERN_ALERT"%s: " DRIVER_DESC "\n",__func__);
	printk(KERN_ALERT "%s: " VERSION_DESC "\n",__func__);
    ret = spi_register_driver(&wk2xxx_driver);
    if(ret<0){
        printk(KERN_ALERT "%s,failed to init wk2xxx spi;ret= :%d\n",__func__,ret);
    }
    return ret;
}



static void __exit wk2xxx_exit(void)
{

    printk(KERN_ALERT "%s!!--in--\n", __func__);
    return spi_unregister_driver(&wk2xxx_driver);
}


module_init(wk2xxx_init);
module_exit(wk2xxx_exit);

MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL");






