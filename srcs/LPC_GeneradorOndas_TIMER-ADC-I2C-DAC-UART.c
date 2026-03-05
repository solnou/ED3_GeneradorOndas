#include "lpc17xx_timer.h"    /* Timer0 */
#include "lpc17xx_gpio.h"    /* GPIO */
#include "lpc17xx_pinsel.h"    /* Pin Configuration */
#include "lpc17xx_adc.h"    /* ADC */
#include "debug_frmwrk.h"   /* Debug Framework */
#include "lpc17xx_i2c.h"    /* I2C */
#include "lpc17xx_dac.h"    /* DAC */
#include "lpc17xx_gpdma.h"   /* DMA */
#include "lpc17xx_libcfg_default.h" /* */

/* DAC Definitions */
#define DMA_SIZE         60
#define NUM_SAMPLES      60 // Total number of samples for the full sine wave
#define CLOCK_DAC_MHZ    25 // DAC clock: 25 MHz (CCLK divided by 4)
#define DMA_CHANNEL_ZERO 0

/* LCD I2C Definitions */
#define LCD_ADDR 0x27 // Dirección I2C de la pantalla, si no anda probar 0x4E y 0x7E
#define I2CDEV LPC_I2C0
#define I2CDEV_CLK 100000
#define BACKLIGHT 0x08
#define COMMAND 0 // LCD interpreta datos enviados como comandos de control (mover el cursor, borrar pantalla, etc.)
#define DATA 1 // LCD interpreta datos enviados como caracteres a mostrar

/* Pin Definitions */
#define FIRST_LED ((uint32_t)(1 << 12)) /* P2.12 connected to LED */
#define SECOND_LED ((uint32_t)(1 << 8)) /* P2.8 connected to LED */
#define THIRD_LED ((uint32_t)(1 << 5)) /* P2.5 connected to LED */

/* GPIO Direction Definitions */
#define INPUT  0
#define OUTPUT 1

/* ADC frquency definitions */
#define ADC_FREQ 100000 /* 100 kHz */

/* Boolean Values */
#define TRUE  1
#define FALSE 0

/* Define frequency variables */
volatile uint16_t adc_read_value = 0;
volatile uint32_t frecuencia = 100;

/* Uart definitions*/
volatile uint32_t received_data[NUM_SAMPLES];
volatile uint8_t data_index = 0;

/* Waveform buffer for DAC output */
volatile uint32_t dac_waveform[NUM_SAMPLES]; 

/* DMA configuration structure */
GPDMA_Channel_CFG_Type GPDMACfg;
GPDMA_LLI_Type DMA_LLI_Struct; // DMA linked list item for continuous transfer


/*-------------------------PRIVATE FUNCTIONS------------------------------*/
void configure_leds(void);
void configure_adc(void);
void configure_timer(void);
void start_timer(void);
void configure_I2C(void);
void configure_dma(uint32_t* table);
void configure_UART(void);
void configure_dac(void);
void LCD_Write(uint8_t data, uint8_t mode);
void LCD_Cmd(uint8_t cmd);
void LCD_Char(char data);
void LCD_Init(void);
void LCD_String(char *str);
void LCD_SetCursor(uint8_t row, uint8_t col);
void delay(uint32_t count);
void displayProgressBar(uint8_t progress);
void copyArray(uint32_t *source, uint32_t *destination, uint32_t size);


/*-------------------------MAIN FUNCTION------------------------------*/
int main(void) {
    // Initialize the system clock (default: 100 MHz)
    SystemInit();

    // Inicializa el UART0
    configure_UART();

    // Configure DAC, and DMA for waveform output
    configure_dac();
    configure_dma(dac_waveform);

    // Inicializa el I2C y el LCD
    configure_I2C();
	LCD_Init();

    // Muestra "Frecuencia:" en la primera fila del LCD
	LCD_String("Frecuencia:"); 

    // Configure the GPIO ports, ADC, and Timer0
	configure_leds();
	configure_adc();
	configure_timer();
	start_timer();

    // Enable DMA channel 0 to start the waveform generation
    GPDMA_ChannelCmd(DMA_CHANNEL_ZERO, ENABLE);

    while (1) {
    	// do nothing
    }

	return 0;
}

/**
 * @brief Configure the pin for DAC output (P0.26) and the timeout for DMA operation.
 */
void configure_dac(void)
{
    PINSEL_CFG_Type PinCfg;

    // Configure pin P0.26 as DAC output
    PinCfg.Funcnum = PINSEL_FUNC_2;           // DAC function
    PinCfg.OpenDrain = PINSEL_PINMODE_NORMAL; // Disable open drain
    PinCfg.Pinmode = PINSEL_PINMODE_PULLUP;   // No pull-up or pull-down
    PinCfg.Pinnum = PINSEL_PIN_26;            // Pin number 26
    PinCfg.Portnum = PINSEL_PORT_0;           // Port number 0
    PINSEL_ConfigPin(&PinCfg);

    DAC_CONVERTER_CFG_Type DAC_Struct;
    uint32_t update_interval;

    // Configure DAC settings
    DAC_Struct.CNT_ENA = SET; // Enable DAC counter mode (timeout mode)
    DAC_Struct.DMA_ENA = SET; // Enable DAC DMA mode
    DAC_Init(LPC_DAC);        // Initialize the DAC

    // Calculate sample update interval for the desired waveform frequency
    update_interval = (CLOCK_DAC_MHZ * 1000000) / (frecuencia * NUM_SAMPLES);
    DAC_SetDMATimeOut(LPC_DAC, update_interval); // Set the DAC timeout between samples

    // Apply the DAC configuration
    DAC_ConfigDAConverterControl(LPC_DAC, &DAC_Struct);
}

/**
 * @brief Configure the DMA to transfer the waveform to the DAC.
 */
void configure_dma(uint32_t* table)
{
    // Set up the DMA linked list for continuous waveform transfer
    DMA_LLI_Struct.SrcAddr = (uint32_t)table;              // Source: DAC waveform table
    DMA_LLI_Struct.DstAddr = (uint32_t) & (LPC_DAC->DACR); // Destination: DAC register
    DMA_LLI_Struct.NextLLI = (uint32_t)&DMA_LLI_Struct;    // Point to itself for continuous transfer
    DMA_LLI_Struct.Control = DMA_SIZE | (2 << 18)          // Source width: 32-bit
                             | (2 << 21)                   // Destination width: 32-bit
                             | (1 << 26);                  // Increment source address

    // Initialize the DMA module
    GPDMA_Init();

    // Configure DMA channel for memory-to-peripheral transfer
    GPDMACfg.ChannelNum = DMA_CHANNEL_ZERO;         // Use channel 0
    GPDMACfg.SrcMemAddr = (uint32_t)table;          // Source: DAC waveform table
    GPDMACfg.DstMemAddr = 0;                        // No memory destination (peripheral)
    GPDMACfg.TransferSize = DMA_SIZE;               // Transfer size: 60 samples
    GPDMACfg.TransferWidth = 0;                     // Not used
    GPDMACfg.TransferType = GPDMA_TRANSFERTYPE_M2P; // Memory-to-Peripheral transfer
    GPDMACfg.SrcConn = 0;                           // Source is memory
    GPDMACfg.DstConn = GPDMA_CONN_DAC;              // Destination: DAC connection
    GPDMACfg.DMALLI = (uint32_t)&DMA_LLI_Struct;    // Linked list for continuous transfer

    // Apply DMA configuration
    GPDMA_Setup(&GPDMACfg);
}

/**
 * @brief Initialize the GPIO pins for the LEDs.
 */
void configure_leds(void)
{
    PINSEL_CFG_Type pin_cfg_struct; /* Create a variable to store the configuration of the pin */

    /* Configure P2.12, P2.8, and P2.5 as GPIO for LED outputs */
    pin_cfg_struct.Portnum = 2;           /* Port number is 2 */
    pin_cfg_struct.Pinnum = 12;            /* Pin number 12 */
    pin_cfg_struct.Funcnum = 0;           /* Function number is 0 (GPIO) */
    pin_cfg_struct.Pinmode = PINSEL_PINMODE_PULLUP;   /* Pin mode is pull-up */
    pin_cfg_struct.OpenDrain = PINSEL_PINMODE_NORMAL; /* Normal mode */
    PINSEL_ConfigPin(&pin_cfg_struct);

    /* Configure the second LED pin */
    pin_cfg_struct.Pinnum = 8;
    PINSEL_ConfigPin(&pin_cfg_struct);

    /* Configure the third LED pin */
    pin_cfg_struct.Pinnum = 5;
    PINSEL_ConfigPin(&pin_cfg_struct);

    /* Set the LED pins as output */
    GPIO_SetDir(2, FIRST_LED | SECOND_LED | THIRD_LED, OUTPUT);

    // Prende y apaga todos los LEDs
    GPIO_SetValue(2, (1 << 12) | (1 << 8) | (1 << 5));
    delay(10000); // Espera 1 segundo
    GPIO_ClearValue(2, (1 << 12) | (1 << 8) | (1 << 5));

}

/**
 * @brief Configure the ADC to sample the potenciometer at 100 kHz.
 */
void configure_adc(void)
{
	PINSEL_CFG_Type pin_cfg_struct; /* Create a variable to store the configuration of the pin */

	/* Configure 0.25 as AD0.2*/
	pin_cfg_struct.Portnum = 0;           /* Port number is 0 */
	pin_cfg_struct.Pinnum = 25;            /* Pin number 25 */
	pin_cfg_struct.Funcnum = 1;           /* Function number is 1 ADO.2) */
	pin_cfg_struct.Pinmode = PINSEL_PINMODE_PULLUP;   /* Pin mode is pull-up */
	pin_cfg_struct.OpenDrain = PINSEL_PINMODE_NORMAL; /* Normal mode */

	/* Configure the AD0.3 pin */
	PINSEL_ConfigPin(&pin_cfg_struct);

    ADC_Init(LPC_ADC, ADC_FREQ); /* Initialize the ADC peripheral with a 100 kHz sampling frequency */
    ADC_BurstCmd(LPC_ADC, DISABLE);
    ADC_ChannelCmd(LPC_ADC, (uint8_t) 2, ENABLE); /* Enable ADC channel 7 */
    ADC_IntConfig(LPC_ADC, (uint8_t) 2, ENABLE); /* Enable interrupt for ADC channel 7 */

    NVIC_SetPriority(ADC_IRQn, 3);
    NVIC_EnableIRQ(ADC_IRQn); /* Enable the ADC interrupt */
}

/**
 * @brief Configure Timer0 to trigger an interrupt every 100 ms
 */
void configure_timer(void)
{
    TIM_TIMERCFG_Type timer_cfg_struct; /* Create a variable to store the configuration of the timer */

    timer_cfg_struct.PrescaleOption = TIM_PRESCALE_USVAL; /* Prescaler is in microseconds */
    timer_cfg_struct.PrescaleValue = (uint32_t) 100; /* Prescaler value is 100, giving a time resolution of ~1.01 µs */

    TIM_Init(LPC_TIM0, TIM_TIMER_MODE, &timer_cfg_struct); /* Initialize Timer0 */

    TIM_MATCHCFG_Type match_cfg_struct; /* Create a variable to store the configuration of the match */

    match_cfg_struct.MatchChannel = 0; /* Match channel 0 */
    match_cfg_struct.IntOnMatch = ENABLE; /* Enable interrupt on match */
    match_cfg_struct.StopOnMatch = DISABLE; /* Do not stop the timer on match */
    match_cfg_struct.ResetOnMatch = ENABLE; /* Reset the timer on match */
    match_cfg_struct.ExtMatchOutputType = TIM_EXTMATCH_NOTHING; /* No external match output */
    match_cfg_struct.MatchValue = (uint32_t)(1000); /* Match value set for 100 ms */

    NVIC_SetPriority(TIMER0_IRQn, 2);
    TIM_ConfigMatch(LPC_TIM0, &match_cfg_struct); /* Configure the match */
}

/**
 * @brief Start Timer0.
 */
void start_timer(void)
{
    TIM_Cmd(LPC_TIM0, ENABLE); /* Enable the timer */
    NVIC_EnableIRQ(TIMER0_IRQn); /* Enable the Timer0 interrupt */
}

/**
 * @brief Configure I2C0 for communication with the LCD.
 */
void configure_I2C(void) {
	/* Config SDA0 y SCL0 */
	PINSEL_CFG_Type PinCfg;
	PinCfg.OpenDrain = 0;
	PinCfg.Pinmode = 0;
	PinCfg.Funcnum = 1;
	PinCfg.Pinnum = 27;
	PinCfg.Portnum = 0;
	PINSEL_ConfigPin(&PinCfg);//SDA0
	PinCfg.Pinnum = 28;
	PINSEL_ConfigPin(&PinCfg);//SCL0

	/* Initialize I2C peripheral*/
    I2C_Init(I2CDEV, I2CDEV_CLK); // Inicializa I2C con velocidad de 100kHz
    I2C_Cmd(I2CDEV, ENABLE); // Habilita I2C
}

/**
 * @brief Write data to the LCD.
 */
void LCD_Write(uint8_t data, uint8_t mode) {
    uint8_t data_u, data_l;
    uint8_t data_t[4]; // Arreglo para guardar los datos a enviar

    data_u = (data & 0xf0); // Extrae los 4 bits más significativos y los coloca en la parte alta del byte
    data_l = ((data << 4) & 0xf0); // Extrae los 4 bits menos significativos y los coloca en la parte baja del byte
    data_t[0] = data_u | mode | BACKLIGHT | 0x04; // Se envia el dato con el enable bit en alto para que el LCD capture el dato
    data_t[1] = data_u | mode | BACKLIGHT; // Sin el enable bit en alto para que el LCD procese el dato
    data_t[2] = data_l | mode | BACKLIGHT | 0x04;
    data_t[3] = data_l | mode | BACKLIGHT;

    I2C_M_SETUP_Type i2cData; // Estructura para configurar la transferencia I2C
    i2cData.sl_addr7bit = LCD_ADDR; // Dirección del LCD
    i2cData.tx_data = data_t; // Datos a enviar
    i2cData.tx_length = 4; // Longitud de los datos
    i2cData.rx_data = NULL; // No se espera recibir datos
    i2cData.rx_length = 0; // No se espera recibir datos
    i2cData.retransmissions_max = 3; // Número máximo de retransmisiones en caso de error
    I2C_MasterTransferData(I2CDEV, &i2cData, I2C_TRANSFER_POLLING); // Transfiere los datos por I2C
}

/**
 * @brief Send a command to the LCD.
 */
void LCD_Cmd(uint8_t cmd) {
    LCD_Write(cmd, COMMAND); // Escribe el comando en el LCD
}

/**
 * @brief Send a character to the LCD.
 */
void LCD_Char(char data) {
    LCD_Write(data, DATA); // Escribe el carácter en el LCD
}

/**
 * @brief Initialize the LCD.
 */
void LCD_Init(void) {
    LCD_Cmd(0x33); // 0011 0011 Se envia 2 veces para asegurar que el LCD está en modo de 8 bits
    LCD_Cmd(0x32); 
    LCD_Cmd(0x06);   
    LCD_Cmd(0x0C);  
    LCD_Cmd(0x28);
    LCD_Cmd(0x01);
    delay(5);
    LCD_SetCursor(0, 0); // Mueve el cursor a la primera fila y primera columna del LCD
}

/**
 * @brief Display a string on the LCD.
 */
void LCD_String(char *str) {
    while (*str) {
        LCD_Char(*str++); // Va escribiendo los caracteres uno por uno
    }
}

/**
 * @brief Set the cursor position on the LCD.
 */
void LCD_SetCursor(uint8_t row, uint8_t col) {
    uint8_t addr = (row == 0) ? 0x80 + col : 0xC0 + col; // Calcula la dirección de la memoria del LCD basada en la fila y columna
    LCD_Cmd(addr); // Mueve el cursor a la posición indicada
}

/**
 * @brief Delay function.
 */
void delay(uint32_t count) {
    uint32_t i;
    for (i = 0; i < count * 1000; i++);
}

/**
 * @brief Display a progress bar on the LCD.
 */
void displayProgressBar(uint8_t progress) {
    char bar[17];
    uint8_t i;
    for (i = 0; i < 16; i++) {
        if (i < progress) {
            bar[i] = 0xFF; // Carácter lleno
        } else {
            bar[i] = ' '; // Espacio vacío
        }
    }
    bar[16] = '\0';
    LCD_SetCursor(1, 0); // mueve el cursor a la segunda fila y primera columna del LCD
    LCD_String(bar);
}

/**
 * @brief Configure the UART0 peripheral for communication.
 */
void configure_UART(void) {
    // Configura los pines P0.2 como TXD0 y P0.3 como RXD0
    PINSEL_CFG_Type PinCfg;
    PinCfg.Funcnum = 1;
    PinCfg.OpenDrain = 0;
    PinCfg.Pinmode = 0;
    PinCfg.Portnum = 0;
    PinCfg.Pinnum = 2;
    PINSEL_ConfigPin(&PinCfg);
    PinCfg.Pinnum = 3;
    PINSEL_ConfigPin(&PinCfg);

    // Configura la velocidad de transmisión (baudrate)
    UART_CFG_Type UARTConfigStruct;
    UART_ConfigStructInit(&UARTConfigStruct);
    UARTConfigStruct.Baud_rate = 9600;
    UART_Init(LPC_UART0, &UARTConfigStruct);
    UART_TxCmd(LPC_UART0, ENABLE);

    // Habilita la interrupción por recepción de datos
    UART_IntConfig(LPC_UART0, UART_INTCFG_RBR, ENABLE);

    // Configura la prioridad de la interrupción y habilítala
    NVIC_SetPriority(UART0_IRQn, 1);
    NVIC_EnableIRQ(UART0_IRQn);

    return;
}


// ----------------- Interrupt Handler Functions -----------------

/**
 * @brief Initializa a conversion on the ADC.
 */
void TIMER0_IRQHandler()
{
    TIM_ClearIntPending(LPC_TIM0, TIM_MR0_INT); /* Clear the interrupt flag */
    ADC_StartCmd(LPC_ADC, ADC_START_NOW); /* Start the ADC conversion */

    return;
}

/**
 * @brief Calculate the frequency of the ADC value and turn on the appropriate LEDs and I2C.
 */
void ADC_IRQHandler()
{
    uint8_t frecuencia_i2c = 0;

	adc_read_value = ADC_ChannelGetData(LPC_ADC, (uint8_t)2); // Get data from ADC

    frecuencia = ((adc_read_value * 900) / 4096) + 100; /* Escale ADC value to frequency */

    adc_read_value = adc_read_value / 256; /* Escala el valor de ADC a 0-16 (Poner en 252 para ver barra completa) */
    frecuencia_i2c = (uint8_t)(adc_read_value);
    displayProgressBar(frecuencia_i2c); /* Muestra la barra de progreso */

    uint32_t update_interval;
    update_interval = (CLOCK_DAC_MHZ * 1000000) / (frecuencia * NUM_SAMPLES);
    DAC_SetDMATimeOut(LPC_DAC, update_interval); // Set the DAC timeout between samples

    return;
}

void UART0_IRQHandler(void) {
    // Verifica si la interrupción es por recepción de datos
    if (UART_GetIntId(LPC_UART0) == UART_IIR_INTID_RDA) {
        // Lee el dato recibido
        uint8_t data = UART_ReceiveByte(LPC_UART0);

        // Si el índice es mayor o igual al tamaño de la muestra, reinicia el índice (ingresa una nueva onda)
        if (data_index == NUM_SAMPLES) data_index = 0;

        // Convierte el dato a un entero de 32 bits para guardarlo en el array para el dac
        uint32_t dac_data = (uint32_t)data;
        dac_data = dac_data * 256; // Escala el dato para que esté en el rango de 0 a 1023

        received_data[data_index] = dac_data;
        data_index = data_index + 1;

        //Enciende los LEDs para indicar que se esta recibiendo un dato
        GPIO_SetValue(2, (1 << 12) | (1 << 8) | (1 << 5));

        // Apaga los LEDs si se ha alcanzado el tamaño de las muestras
        if (data_index == NUM_SAMPLES){
        	//GPDMA_ChannelCmd(DMA_CHANNEL_ZERO, DISABLE);
        	GPIO_ClearValue(2, (1 << 12) | (1 << 8) | (1 << 5));
            copyArray(received_data, dac_waveform, NUM_SAMPLES);
            //GPDMA_ChannelCmd(DMA_CHANNEL_ZERO, ENABLE); // Activa la transferencia de la onda
        }
    }

    return;
}

void copyArray(uint32_t *source, uint32_t *destination, uint32_t size) {
    for (uint32_t i = 0; i < size; i++) {
        destination[i] = source[i];
    }
}
