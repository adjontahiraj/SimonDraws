
/*
 * ECE 153b final project
 * by Adjon Tahiraj
 *    Ryan Phan
 *    Michelle Nguyen
 *  Our project is a memory based game with multiple rounds where
 *  the user needs to memorize the pattern shown in the small period
 *  of time and input that pattern into the LED board using the built
 *  in joystick and confirm using Button SW6 which uses GPIO.
 */

#include <stdlib.h>
#include <string.h>
#include <board.h>


/*****************************************************************************
 * Private types/enumerations/variables
 ****************************************************************************/
#define DEFAULT_I2C          I2C0

#define I2C_EEPROM_BUS       DEFAULT_I2C
#define I2C_IOX_BUS          DEFAULT_I2C

#define SPEED_100KHZ         100000
#define SPEED_400KHZ         400000

#define TIMER0_IRQ_HANDLER                TIMER0_IRQHandler  // TIMER0 interrupt IRQ function name
#define TIMER0_INTERRUPT_NVIC_NAME        TIMER0_IRQn        // TIMER0 interrupt NVIC interrupt name


#define GPIO_LED_PIN     10                // GPIO pin number mapped to LED toggle
#define GPIO_LED_PORT    GPIOINT_PORT2    // GPIO port number mapped to LED toggle

static int mode_poll;   /* Poll/Interrupt mode flag */
static I2C_ID_T i2cDev = DEFAULT_I2C; /* Currently active I2C device */

/* EEPROM SLAVE data */
#define I2C_SLAVE_EEPROM_SIZE       64
#define I2C_SLAVE_EEPROM_ADDR       0x5A
#define I2C_SLAVE_TEMP_ADDR         0x70

/* Xfer structure for slave operations */
static I2C_XFER_T temp_xfer;
//static I2C_XFER_T iox_xfer;

static uint8_t i2Cbuffer[2][256];

uint8_t picBuffer[17] = {0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000,
    0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000};

uint8_t redBuffer[17] = {0b00000000, 0b00000000,
    0b00000000, 0b00000000,
    0b00000100, 0b00000000,
    0b00001100, 0b00000000,
    0b00000100, 0b00000000,
    0b01110100, 0b00000000,
    0b00000100, 0b00000000,
    0b00001110, 0b00000000,
    0b00000000};
int joyDebounce = 1;
int x = 1;
int y = 2;





/*****************************************************************************
 * Private functions
 ****************************************************************************/



/* State machine handler for I2C0 and I2C1 */
static void i2c_state_handling(I2C_ID_T id)
{
    if (Chip_I2C_IsMasterActive(id)) {
        Chip_I2C_MasterStateHandler(id);
    } else {
        Chip_I2C_SlaveStateHandler(id);
    }
}

/* Set I2C mode to polling/interrupt */
static void i2c_set_mode(I2C_ID_T id, int polling)
{
    if(!polling) {
        mode_poll &= ~(1 << id);
        Chip_I2C_SetMasterEventHandler(id, Chip_I2C_EventHandler);
        NVIC_EnableIRQ(id == I2C0 ? I2C0_IRQn : I2C1_IRQn);
    } else {
        mode_poll |= 1 << id;
        NVIC_DisableIRQ(id == I2C0 ? I2C0_IRQn : I2C1_IRQn);
        Chip_I2C_SetMasterEventHandler(id, Chip_I2C_EventHandlerPolling);
    }
}

void TIMER0_IRQHandler(void)
{
    if (Chip_TIMER_MatchPending(LPC_TIMER0, 0))
    {
        Chip_TIMER_ClearMatch(LPC_TIMER0,0);  // Clear TIMER0 interrupt
        joyDebounce = 1;
        Chip_TIMER_Reset(LPC_TIMER0);
        Chip_TIMER_Disable(LPC_TIMER0);
    }
}


/* Initialize the I2C bus */
static void i2c_app_init(I2C_ID_T id, int speed)
{
    Board_I2C_Init(id);
    
    /* Initialize I2C */
    Chip_I2C_Init(id);
    Chip_I2C_SetClockRate(id, speed);
    
    /* Set default mode to interrupt */
    i2c_set_mode(id, 0);
}

static void i2c_write_setup(I2C_XFER_T *xfer, uint8_t addr, int numBytes)
{
    xfer->slaveAddr = addr;
    xfer->rxBuff = 0;
    xfer->txBuff = 0;
    xfer->rxSz = 0;
    xfer->txSz = numBytes;
    xfer->txBuff = i2Cbuffer[0];
}

void delay_ms (uint32_t ms)
{
    uint32_t delay;
    volatile uint32_t i;
    for (delay = ms; delay >0 ; delay--)
    {
        for (i=3500; i >0;i--){};
    }
}


/*
 * handles the pattern confirmation when done drawing
 */


/*****************************************************************************
 * Public functions
 ****************************************************************************/
//Function used to count down 3,2,1 before the game begins
void countDown(int countDown) {
    if(countDown == 3) {
        picBuffer[1] = 0b00000000;
        picBuffer[3] = 0b00111100;
        picBuffer[5] = 0b00000100;
        picBuffer[7] = 0b00011100;
        picBuffer[9] = 0b00011100;
        picBuffer[11] = 0b00000100;
        picBuffer[13] = 0b00111100;
        picBuffer[15] = 0b00000000;
    }else if(countDown ==2) {
        picBuffer[1] = 0b00000000;
        picBuffer[3] = 0b00111100;
        picBuffer[5] = 0b00000100;
        picBuffer[7] = 0b00000100;
        picBuffer[9] = 0b00001000;
        picBuffer[11] = 0b00010000;
        picBuffer[13] = 0b00111100;
        picBuffer[15] = 0b00000000;
    }else{
        picBuffer[1] = 0b00000000;
        picBuffer[3] = 0b00001000;
        picBuffer[5] = 0b00011000;
        picBuffer[7] = 0b00001000;
        picBuffer[9] = 0b00001000;
        picBuffer[11] = 0b00001000;
        picBuffer[13] = 0b00011100;
        picBuffer[15] = 0b00000000;
    }
    int i;
    for(i = 1; i<=15; i++) {
        i2Cbuffer[0][i] = picBuffer[i];
    }
}

//Function used to display the images that the user needs to memorize for each round
void initPic(int round) {

    if(round == 1) {
        //X- image
        picBuffer[1] = 0b00000000;
        picBuffer[3] = 0b01000010;
        picBuffer[5] = 0b00100100;
        picBuffer[7] = 0b00011000;
        picBuffer[9] = 0b00011000;
        picBuffer[11] = 0b00100100;
        picBuffer[13] = 0b01000010;
        picBuffer[15] = 0b00000000;
    }else if(round == 2) {
        //Smiley Face Img
        picBuffer[1] = 0b00111100;
        picBuffer[3] = 0b01000010;
        picBuffer[5] = 0b10100101;
        picBuffer[7] = 0b10000001;
        picBuffer[9] = 0b10100101;
        picBuffer[11] = 0b10011001;
        picBuffer[13] = 0b01000010;
        picBuffer[15] = 0b00111100;
    }else if(round == 3) {
        //Final Round Hard Pattern
        picBuffer[1] = 0b10010001;
        picBuffer[3] = 0b01010010;
        picBuffer[5] = 0b00111100;
        picBuffer[7] = 0b10111010;
        picBuffer[9] = 0b01011101;
        picBuffer[11] = 0b00111100;
        picBuffer[13] = 0b01001010;
        picBuffer[15] = 0b10001001;
    }
    int i;
    for(i = 1; i<=15; i++) {
        i2Cbuffer[0][i] = picBuffer[i];
    }
}

//Function used to put the initial dot at the top left after the user has seen the pattern
//and before the user can start moving the dot to fill in the pattern
void putDot(void) {
    //Put the blinker at the initial starting position
    i2Cbuffer[0][0] =     0b00000000;
    i2Cbuffer[0][1] =     0b00000000;//Row 1 - Green
    i2Cbuffer[0][2] =     0b10000000;//Row 1 - Red
    i2Cbuffer[0][3] =     0b00000000;//Row 2
    i2Cbuffer[0][4] =     0b00000000;
    i2Cbuffer[0][5] =     0b00000000;//Row 3
    i2Cbuffer[0][6] =     0b00000000;
    i2Cbuffer[0][7] =     0b00000000;//Row 4
    i2Cbuffer[0][8] =     0b00000000;
    i2Cbuffer[0][9] =     0b00000000;//Row 5
    i2Cbuffer[0][10] =  0b00000000;
    i2Cbuffer[0][11] =  0b00000000;//Row 6
    i2Cbuffer[0][12] =  0b00000000;
    i2Cbuffer[0][13] =  0b00000000;//Row 7
    i2Cbuffer[0][14] =  0b00000000;
    i2Cbuffer[0][15] =  0b00000000;//Row 8
    i2Cbuffer[0][16] =  0b00000000;
    Chip_I2C_MasterSend(i2cDev, temp_xfer.slaveAddr, temp_xfer.txBuff, 17);
}

//Function used to display a "W" after the user has passed all 3 rounds
void youWin(void){
    picBuffer[1] = 0b00000000;
    picBuffer[3] = 0b01000010;
    picBuffer[5] = 0b01000010;
    picBuffer[7] = 0b01011010;
    picBuffer[9] = 0b01011010;
    picBuffer[11] = 0b01011010;
    picBuffer[13] = 0b00111100;
    picBuffer[15] = 0b00000000;
    
    int i;
    for(i = 1; i<=15; i++) {
        i2Cbuffer[0][i] = picBuffer[i];
    }
}

//Function used to display an "L" after the user has failed to complete the 3 rounds
//within the 3 lives that they have
void youLose(void){
    picBuffer[1] = 0b00000000;
    picBuffer[3] = 0b00000000;
    picBuffer[5] = 0b01111100;
    picBuffer[7] = 0b00000100;
    picBuffer[9] = 0b00000100;
    picBuffer[11] = 0b00000100;
    picBuffer[13] = 0b00000000;
    picBuffer[15] = 0b00000000;
    
    int i;
    for(i = 1; i<=15; i++) {
        i2Cbuffer[0][i] = picBuffer[i];
    }
}

//Function used to check if the user's drawn image matches the given image for that round
int confirmButton(int round)
{
    if(round == 1) {
        //X- image
        picBuffer[1] = 0b00000000;
        picBuffer[3] = 0b01000010;
        picBuffer[5] = 0b00100100;
        picBuffer[7] = 0b00011000;
        picBuffer[9] = 0b00011000;
        picBuffer[11] = 0b00100100;
        picBuffer[13] = 0b01000010;
        picBuffer[15] = 0b00000000;
    }else if(round == 2) {
        //Smiley Face Img
        picBuffer[1] = 0b00111100;
        picBuffer[3] = 0b01000010;
        picBuffer[5] = 0b10100101;
        picBuffer[7] = 0b10000001;
        picBuffer[9] = 0b10100101;
        picBuffer[11] = 0b10011001;
        picBuffer[13] = 0b01000010;
        picBuffer[15] = 0b00111100;
    }else if(round == 3) {
        //Final Round Hard Pattern
        picBuffer[1] = 0b10010001;
        picBuffer[3] = 0b01010010;
        picBuffer[5] = 0b00111100;
        picBuffer[7] = 0b10111010;
        picBuffer[9] = 0b01011101;
        picBuffer[11] = 0b00111100;
        picBuffer[13] = 0b01001010;
        picBuffer[15] = 0b10001001;
    }
    int z = 1;
    int c = 1;
    for( z; z <= 15; z+=2) {
        if(i2Cbuffer[0][z] != picBuffer[z]) {
            c = 0;
            break;
        }
    }
    return c;
}

//Function used to update the position of the dot as the user moves it around with the joystick
void drawPos(int status)
{
    switch (status)
    {
        case 0b10000://click
            i2Cbuffer[0][y-1] = (i2Cbuffer[0][y-1] ^ i2Cbuffer[0][y]);
            break;
        case 0b0001: //1 right
            if (y<16){
                i2Cbuffer[0][y+2] = i2Cbuffer[0][y];
                i2Cbuffer[0][y] = 0b00000000;
                y=y+2;
                //x = x+1;
                //posBuffer[y] = posBuffer[y] >> 1;
            }
            break;
        case 0b0010: //2 left
            if (y>2){
                i2Cbuffer[0][y-2] = i2Cbuffer[0][y];
                i2Cbuffer[0][y] = 0b00000000;
                y=y-2;
                //x = x+1;
                //posBuffer[y] = posBuffer[y] << 1;
            }
            break;
        case 0b0100: //4 up
            if (x>1){
                x = x - 1;
                i2Cbuffer[0][y] = i2Cbuffer[0][y] << 1;
                //posBuffer[y-2] = posBuffer[y];
                //posBuffer[y] = 0b00000000;
                //y=y-2;
            }
            break;
        case 0b1000: //8 down
            if (x<8){
                x = x+1;
                i2Cbuffer[0][y] = i2Cbuffer[0][y] >> 1;
                //posBuffer[y+2] = posBuffer[y];
                //posBuffer[y] = 0b00000000;
                //y=y+2;
            }
            break;
    }
    
    Chip_I2C_MasterSend(i2cDev, temp_xfer.slaveAddr, temp_xfer.txBuff, 17);
}

//Function used to empty the buffer
void emptyBuff(void) {
    int z = 0;
    for(z; z<17; z++) {
        i2Cbuffer[0][z] = 0b00000000;
    }
}

//Function used to display a "-1" when the user fails to guess the pattern correctly
void redBuff(void) {
    int z = 2;
    for(z; z<=16; z+=2) {
        i2Cbuffer[0][z] = redBuffer[z];
    }
}

//Function used to display a green output to the entire buffer if the user guesses the pattern correcty
void greenBuff(void) {
    int z = 1;
    for(z; z<=15; z+=2) {
        i2Cbuffer[0][z] = 0b11111111;
    }
}

/**
 * @brief    SysTick Interrupt Handler
 * @return    Nothing
 * @note    Systick interrupt handler updates the button status
 */
void SysTick_Handler(void)
{
}

/**
 * @brief    I2C Interrupt Handler
 * @return    None
 */
void I2C1_IRQHandler(void)
{
    i2c_state_handling(I2C1);
}

/**
 * @brief    I2C0 Interrupt handler
 * @return    None
 */
void I2C0_IRQHandler(void)
{
    i2c_state_handling(I2C0);
}

/**
 * @brief    Main program body
 * @return    int
 */
int main(void)
{
    int PrescaleValue = 120000;
    
    
    Chip_TIMER_Init(LPC_TIMER0);                       // Initialize TIMER0
    Chip_TIMER_PrescaleSet(LPC_TIMER0,PrescaleValue);  // Set Prescale value
    Chip_TIMER_SetMatch(LPC_TIMER0,0,100);               // Set match value
    Chip_TIMER_MatchEnableInt(LPC_TIMER0, 0);           // Configure to trigger interrupt on match
    NVIC_ClearPendingIRQ(TIMER0_INTERRUPT_NVIC_NAME);
    NVIC_EnableIRQ(TIMER0_INTERRUPT_NVIC_NAME);
    
    //Initializing
    Board_Init();
    SystemCoreClockUpdate();
    Board_Joystick_Init();
    i2c_app_init(I2C0, SPEED_100KHZ);
    i2c_set_mode(I2C0, 0);
    i2cDev = I2C0;
    
    
    //begging setup to 0x70 and turning on internal system oscillator for the led by sending 0x21
    //and the rest of the starting commands that are found in the datasheet
    i2c_write_setup(&temp_xfer, (I2C_SLAVE_TEMP_ADDR), 1);
    i2Cbuffer[0][0] = 0x21;
    Chip_I2C_MasterSend(i2cDev, temp_xfer.slaveAddr, temp_xfer.txBuff, 1);
    i2Cbuffer[0][0] = (0x80 | 0x01);
    Chip_I2C_MasterSend(i2cDev, temp_xfer.slaveAddr, temp_xfer.txBuff, 1);
    uint8_t b = 15;
    i2Cbuffer[0][0] = (0xE0 | b);
    Chip_I2C_MasterSend(i2cDev, temp_xfer.slaveAddr, temp_xfer.txBuff, 1);
    
    
    //Initialize buffer to all off
    i2Cbuffer[0][0] =     0b00000000;
    i2Cbuffer[0][1] =     0b00000000;//Row 1-Green
    i2Cbuffer[0][2] =     0b00000000;//Row 1-Red
    i2Cbuffer[0][3] =     0b00000000;//Row 2
    i2Cbuffer[0][4] =     0b00000000;
    i2Cbuffer[0][5] =     0b00000000;//Row 3
    i2Cbuffer[0][6] =     0b00000000;
    i2Cbuffer[0][7] =     0b00000000;//Row 4
    i2Cbuffer[0][8] =     0b00000000;
    i2Cbuffer[0][9] =     0b00000000;//Row 5
    i2Cbuffer[0][10] =  0b00000000;
    i2Cbuffer[0][11] =  0b00000000;//Row 6
    i2Cbuffer[0][12] =  0b00000000;
    i2Cbuffer[0][13] =  0b00000000;//Row 7
    i2Cbuffer[0][14] =  0b00000000;
    i2Cbuffer[0][15] =  0b00000000;//Row 8
    i2Cbuffer[0][16] =  0b00000000;
    Chip_I2C_MasterSend(i2cDev, temp_xfer.slaveAddr, temp_xfer.txBuff, 17);
    
    //This part of the code counts down from 3,2,1 using countDown function and delays
    countDown(3);
    Chip_I2C_MasterSend(i2cDev, temp_xfer.slaveAddr, temp_xfer.txBuff, 17);
    delay_ms(2000);
    countDown(2);
    Chip_I2C_MasterSend(i2cDev, temp_xfer.slaveAddr, temp_xfer.txBuff, 17);
    delay_ms(2000);
    countDown(1);
    Chip_I2C_MasterSend(i2cDev, temp_xfer.slaveAddr, temp_xfer.txBuff, 17);
    delay_ms(4000);
    
    //Outputting the initial picture for round 1 to start the round
    initPic(1);
    Chip_I2C_MasterSend(i2cDev, temp_xfer.slaveAddr, temp_xfer.txBuff, 17);
    delay_ms(6000);
    
    //Putting the dot on the initial position
    putDot();
    
    //Variables used in the main function
    bool buttonPress = 0;
    int win = 0;
    int lives = 3;
    int round = 1;
    while(lives > 0 && round <=3)
    {
        //Getting the input from the joystick
        int status = Joystick_GetStatus();
        
        //Making sure to debounce the joystick input and update the position on
        //the LED board using the function drawPos
        if (status && joyDebounce) {
            joyDebounce = 0;
            printf("TEST\r\n");
            drawPos(status);
            Chip_TIMER_Enable(LPC_TIMER0);
        }
        
        //checking to see if we have pressed the confirm button
        buttonPress = Chip_GPIO_GetPinState(LPC_GPIO, 2, 10);
        if(buttonPress == 0) {
            //checking if the user guessed the image corrctly
            win = confirmButton(round);
            if (win == 1) {
                //Displaying a flashing green if the user guessed correctly
                while( win < 4) {
                    emptyBuff();
                    Chip_I2C_MasterSend(i2cDev, temp_xfer.slaveAddr, temp_xfer.txBuff, 17);
                    delay_ms(1500);
                    greenBuff();
                    Chip_I2C_MasterSend(i2cDev, temp_xfer.slaveAddr, temp_xfer.txBuff, 17);
                    delay_ms(1500);
                    win += 1;
                }
                //updating to next round
                round+=1;
                if(round<=3){
                    //if game is not over we show the picture for next round and update positions
                    initPic(round);
                    Chip_I2C_MasterSend(i2cDev, temp_xfer.slaveAddr, temp_xfer.txBuff, 17);
                    delay_ms(6000);
                    emptyBuff();
                    Chip_I2C_MasterSend(i2cDev, temp_xfer.slaveAddr, temp_xfer.txBuff, 17);
                    putDot();
                    x = 1;
                    y = 2;
                }else{
                    //if passed round 3, game is over and the LED displays the "W" using youWin function
                    youWin();
                    Chip_I2C_MasterSend(i2cDev, temp_xfer.slaveAddr, temp_xfer.txBuff, 17);
                    delay_ms(8000);
                    break;
                }
            }else{
                //if the user guessed incorrectly we display a flashin "-1"
                while(win < 3) {
                    emptyBuff();
                    Chip_I2C_MasterSend(i2cDev, temp_xfer.slaveAddr, temp_xfer.txBuff, 17);
                    delay_ms(1500);
                    redBuff();
                    Chip_I2C_MasterSend(i2cDev, temp_xfer.slaveAddr, temp_xfer.txBuff, 17);
                    delay_ms(1500);
                    win+= 1;
                }
                //decrement the lives of the user
                lives -=1;
                //if user has no lives left we display a "L" and the game is over
                if(lives <1) {
                    youLose();
                    Chip_I2C_MasterSend(i2cDev, temp_xfer.slaveAddr, temp_xfer.txBuff, 17);
                    delay_ms(8000);
                    break;
                }else{
                    //if the user still has lives we show them the picture for the same round so
                    //they can try to guess again before being able to move on to the next round
                    countDown(lives);
                    Chip_I2C_MasterSend(i2cDev, temp_xfer.slaveAddr, temp_xfer.txBuff, 17);
                    delay_ms(6000);
                    initPic(round);
                    Chip_I2C_MasterSend(i2cDev, temp_xfer.slaveAddr, temp_xfer.txBuff, 17);
                    delay_ms(6000);
                    emptyBuff();
                    Chip_I2C_MasterSend(i2cDev, temp_xfer.slaveAddr, temp_xfer.txBuff, 17);
                    putDot();
                    x = 1;
                    y = 2;
                }
            }
        }
    }
    Chip_I2C_DeInit(I2C0);
    
    return 0;
}
