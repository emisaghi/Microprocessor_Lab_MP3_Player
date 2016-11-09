/********************************************************************************************

Amir Hossein Jamali, Ehsan Misaghi, Homa Ale davoud, Mohammad Soltani

*********************************************************************************************/

/* Includes ------------------------------------------------------------------*/
#include "ff.h"
#include "lpc17xx_uart.h"
#include "SPI_MSD_Driver.h"
#include "vs1003.h"
#include "mp3.h"
#include "adc.h"
#include <stdio.h>
#include <string.h>
#include "sys.h"
#include "lcd.h"
#include "exti.h"

#ifdef __GNUC__
  /* With GCC/RAISONANCE, small printf (option LD Linker->Libraries->Small printf
     set to 'Yes') calls __io_putchar() */
  #define PUTCHAR_PROTOTYPE int __io_putchar(int ch)
#else
  #define PUTCHAR_PROTOTYPE int fputc(int ch, FILE *f)
#endif /* __GNUC__ */

/* Private variables ---------------------------------------------------------*/
FATFS fs;         /* Work area (file system object) for logical drive */
FIL fsrc;         /* file objects */   
FRESULT res;
UINT br, bw,bt;         // File R/W count
uint8_t flag=0;
uint8_t buffer[1024]; // file copy buffer
uint32_t datasize=0;
uint16_t AD_Old_value;	
char path[512]="0:";
char RPATH[10][50];
unsigned char LCDPATH[10][14];
int FileCount=0;
int q=0,ss=12,ff=1, ii=1; 
int number=0;
const char *MusicExtend1[4]={"MP3","WMA","WAV","MID"};
//unsigned char test[10][14]={"AAgdfgnbhmjkj", "B44B", "CC", "DD","FF", "GG", "TT","cc"};
/* Private function prototypes -----------------------------------------------*/
int SD_TotalSize(void);
void USART_Configuration(void);
FRESULT scan_files (char* path);
void Play(int MusicNum);

int main(void)
{
	  uint16_t a,i;          
		SystemInit ();
		lcd_Initializtion();
		lcd_clear(BLACK);	
    Vs1003_Init();
		USART_Configuration();
    USER_ADC_Init();
		printf("****************************************************************\r\n");
		printf("*                                                              *\r\n");
		printf("*                Hello and welcome!  ^_^                       *\r\n");
		printf("*                                                              *\r\n");
		printf("****************************************************************\r\n");
    f_mount(0, &fs);
		scan_files(path);
		SD_TotalSize();
		Mp3Reset();
		VsRamTest( );
    Vs1003SoftReset();
		printf("*              Testing the player using sine test.             *\r\n");
		VsSineTest();
		//Display_Str(0,0,YELLOW,test);
		showlist (LCDPATH);
		draw_pointer();
		LPC_PINCON->PINSEL4 = 1<<20 | 1<<22 | 1<<24;;
		LCD_DrawRectangle(5, 5, 250, 250);
		LCD_DrawRectangle(6, 6, 249, 249);
		LCD_DrawRectangle(7, 7, 248, 248);
		LCD_DrawRectangle(10, 10, 245,245);
		LCD_DrawRectangle(11, 11, 244, 244);
		LCD_DrawRectangle(12, 12, 243, 243);
		//write_pic(redcolor,greencolor,bluecolor,0,0);
		NVIC_EnableIRQ(EINT1_IRQn);
		NVIC_EnableIRQ(EINT2_IRQn);
		NVIC_EnableIRQ(EINT0_IRQn);
		LPC_SC->EXTMODE |=1<<1 | 1<<2 | 1;  // H trigger
		LPC_SC-> EXTPOLAR =0X00;
		LPC_TIM0->PR=999;
		LPC_TIM0->TC=0;
		LPC_TIM0->CTCR=0;
		LPC_TIM0->MR0=24999;
		LPC_TIM0->MCR=3;
		NVIC_EnableIRQ(1);      //timer 0 interrupt enable	    
		while(1)
		{			
			while(number==0);
			Play(number-1);
			number=0;
			f_close(&fsrc);
		}
    f_mount(0, NULL);      
}

void Play(int MusicNum)
{
	uint16_t count;
	uint16_t AD_value;
	unsigned int datasize=0;
	int end=0;
	int i=0,ii=0;
	FileCount=0;
	printf ("Reading file %s\r\n",LCDPATH[MusicNum]);
	bw = f_open(&fsrc,RPATH[MusicNum],FA_OPEN_EXISTING | FA_READ);
	while(end==0)
	{
		if(number==0){
			printf("Stopped. \r\n");
			return;
		}
		if(count++==20)
		{
			count=0;
			AD_value=USER_ADC_Get();
      if(abs(AD_value-AD_Old_value)>30) 
				{
          AD_Old_value = AD_value;
					AD_value=(255-(AD_value*255)/4096);
					AD_value<<=8;
          AD_value+=(255-(AD_Old_value*255)/4096);  
					Vs1003_CMD_Write(SPI_VOL,AD_value);
				}
	 	}
		if(datasize<fsrc.fsize)
		{
			bw = f_read(&fsrc,buffer,512,&br);
			datasize+=512;
			for(i=0;i<16;i++)
			{	 
				while(!(LPC_GPIO0->FIOPIN&MP3_DREQ));
				Vs1003_DATA_Write(buffer+i*32);
			}
		 SPI_Set_Speed(GetHeadInfo());
			ii++;
		}
		else
		{	
			end=1;
			printf("End of file.\r\n");
		}
	}
}

/*******************************************************************************
* Function Name  : USART_Configuration
* Description    : Configure USART1 
* Input          : None
* Output         : None
* Return         : None
* Attention		 : None
*******************************************************************************/
void USART_Configuration(void)
{ 
    uint32_t  Fdiv;
	PINSEL_CFG_Type PinCfg;
	/*
	 * Initialize UART2 pin connect
	 */
	PinCfg.Funcnum = 1;
	PinCfg.OpenDrain = 0;
	PinCfg.Pinmode = 0;
	PinCfg.Portnum = 0;
	PinCfg.Pinnum = 10;
	PINSEL_ConfigPin(&PinCfg);
	PinCfg.Pinnum = 11;
	PINSEL_ConfigPin(&PinCfg);

	/* Initialize UART Configuration parameter structure to default state:
	 * Baudrate = 115200bps
	 * 8 data bit
	 * 1 Stop bit
	 * None parity
	 */
	LPC_SC->PCONP = LPC_SC->PCONP|(1<<24);	  /* UART2 Power bit */

    LPC_UART2->LCR = 0x83;		              /* 8 bits, no Parity, 1 Stop bit */

    #define FOSC    12000000                  /* 振荡器频率 */

    #define FCCLK   (FOSC  * 8)               /* 主时钟频率<=100Mhz FOSC的整数倍 */

    #define FPCLK   (FCCLK / 4)               /* 外设时钟频率 FCCLK的1/2 1/4 */

	Fdiv = ( 25000000 / 16 ) / 115200 ;	      /*baud rate */

    LPC_UART2->DLM = Fdiv / 256;							
    LPC_UART2->DLL = Fdiv % 256;
    LPC_UART2->LCR = 0x03;		              /* DLAB = 0 */
    LPC_UART2->FCR = 0x07;		              /* Enable and reset TX and RX FIFO. */
}

/*******************************************************************************
* Function Name  : scan_files
* Description    : 搜索文件目录下所有文件
* Input          : - path: 根目录
* Outut         : None
* Return         : FRESULT
* Attention		 : 不支持长文件名
*******************************************************************************/
FRESULT scan_files (char* path)
{
    FILINFO fno;
    DIR dir;
    int i,ii;
    char *fn;
#if _USE_LFN
    static char lfn[_MAX_LFN * (_DF1S ? 2 : 1) + 1];
    fno.lfname = lfn;
    fno.lfsize = sizeof(lfn);
#endif

    res = f_opendir(&dir, path);
    if (res == FR_OK) {
        i = strlen(path);
        for (;;) {
            res = f_readdir(&dir, &fno);
            if (res != FR_OK || fno.fname[0] == 0) break;
            if (fno.fname[0] == '.') continue;
#if _USE_LFN
            fn = *fno.lfname ? fno.lfname : fno.fname;
#else
            fn = fno.fname;
#endif
            if (fno.fattrib & AM_DIR) {
									
              sprintf(&path[i], "/%s", fn);  
				res = scan_files(path);
                if (res != FR_OK) break;
                path[i] = 0;
            } else {
							ii=strlen(fn);
							if(strcasecmp(&fn[ii-3],MusicExtend1[0])==0||
			   strcasecmp(&fn[ii-3],MusicExtend1[1])==0||
			   strcasecmp(&fn[ii-3],MusicExtend1[2])==0||
			   strcasecmp(&fn[ii-3],MusicExtend1[3])==0
			  )
				{
					
              sprintf(RPATH[FileCount], "%s%s", path,fn);
							sprintf(LCDPATH[FileCount], "%s",fn);
							FileCount++;
					
				}			
                printf("%s/%s \r\n", path, fn);
            }
        }
    }
    return res;
}

/*******************************************************************************
* Function Name  : SD_TotalSize
* Description    : 文件空间占用情况
* Input          : None
* Output         : None
* Return         : 返回1成功 返回0失败
* Attention		 : None
*******************************************************************************/
int SD_TotalSize(void)
{
    FATFS *fs;
    DWORD fre_clust;        

    res = f_getfree("0:", &fre_clust, &fs);  /* 必须是根目录，选择磁盘0 */
    if ( res==FR_OK ) 
    {
	  /* Print free space in unit of MB (assuming 512 bytes/sector) */
      printf("\r\n%d MB total drive space.\r\n"
           "%d MB available.\r\n",
           ( (fs->n_fatent - 2) * fs->csize ) / 2 /1024 , (fre_clust * fs->csize) / 2 /1024 );
		
	  return ENABLE;
	}
	else 
	  return DISABLE;   
}	 

/**
  * @brief  Retargets the C library printf function to the USART.
  * @param  None
  * @retval None
  */
PUTCHAR_PROTOTYPE
{
	/* wait for current transmission complete - THR must be empty */
	while (UART_CheckBusy(LPC_UART2) == SET);
	
	UART_SendByte(LPC_UART2, ch);
	
	return ch;
}

#ifdef  DEBUG
/*******************************************************************************
* @brief		Reports the name of the source file and the source line number
* 				where the CHECK_PARAM error has occurred.
* @param[in]	file Pointer to the source file name
* @param[in]    line assert_param error line source number
* @return		None
*******************************************************************************/
void check_failed(uint8_t *file, uint32_t line)
{
	/* User can add his own implementation to report the file name and line number,
	 ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */

	/* Infinite loop */
	while(1);
}
#endif

/*********************************************************************************************************
      END FILE
*********************************************************************************************************/
//Event Handlers
void EINT1_IRQHandler(void)
{
	if (q<9)
	{
	down(q);
	q=q+1;
	}
	LPC_SC->EXTINT |=1<<1;      //clear flag
}

void EINT2_IRQHandler(void)
{ 
	if (q>0)
	{
	up(q);
	q=q-1;
	}
	LPC_SC->EXTINT |=1<<2;      //clear flag
}

 void EINT0_IRQHandler(void)
{
	int i;
LPC_TIM0->TCR=0;  
	number=play();
	
	Display_Str(300, 15,BLACK , "%%%%%%%%%%%%%%");
	Display_Str(300, 15, CYAN , LCDPATH[q]);
	LPC_TIM0->TCR=1;   
	for(i=0;i<14;i++)
{
	Display_Str(300, 15,BLACK , "%%%%%%%%%%%%%%");
	Display_Str(300, 15, CYAN , LCDPATH[q]);

}	

			
		
		
	LPC_SC->EXTINT |=1; 
}
void TIMER0_IRQHandler()
{
	int i=0;

//	Display_Str( (300+8*ii), 15,BLACK , "%");
	
//		Display_char( (300+8*ii), 5, CYAN , a[q][ii]);
//		Display_Str( (300+8*ii), 5,BLACK , "%");
	
//		    Display_char( (300+8*(ii)), 15, RED , a[q][ii]);
	    //  Display_char( (300+8*(ii-1)), 15, CYAN , a[q][ii-1]);

	ii++;
	if(ii == 14)
	{
		ii=1; 
	}
	if(ii==1)  Display_char( (300+8*(14)), 15, CYAN , LCDPATH[q][14]); 
	LPC_TIM0->IR=1;

	
}


//
