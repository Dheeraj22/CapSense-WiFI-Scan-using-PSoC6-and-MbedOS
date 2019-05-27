/* WiFi Example
 * Copyright (c) 2016 ARM Limited
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "mbed.h"
#include "cy_pdl.h"
#include "cycfg_capsense.h"

/***************************************************************************
* Global constants
***************************************************************************/
#define SLIDER_NUM_TOUCH                        (1u)    /* Number of touches on the slider */
#define LED_OFF                                 (1u) 
#define LED_ON                                  (0u)
#define CAPSENSE_SCAN_PERIOD                    (20u)   /* milliseconds */

/***************************************
* Function Prototypes
**************************************/
void RunCapSenseScan(void);
void InitTunerCommunication(void);
void ProcessTouchStatus(void);
void EZI2C_InterruptHandler(void);
void CapSense_InterruptHandler(void);
void CapSenseEndOfScanCallback(cy_stc_active_scan_sns_t * ptrActiveScan);
int connect_to_network(void);

WiFiInterface *wifi;

/*******************************************************************************
* Interrupt configuration
*******************************************************************************/
const cy_stc_sysint_t CapSense_ISR_cfg =
{
    .intrSrc = CapSense_IRQ,
    .intrPriority = 7u
};

const cy_stc_sysint_t EZI2C_ISR_cfg = {
    .intrSrc = CSD_COMM_IRQ,
    .intrPriority = 3u
};

/*******************************************************************************
* Global variables
*******************************************************************************/
DigitalOut ledRed(LED_RED);
Semaphore capsense_sem;
EventQueue queue;
cy_stc_scb_ezi2c_context_t EZI2C_context;
uint32_t prevBtn0Status = 0u; 
uint32_t prevBtn1Status = 0u;
uint32_t prevSliderPos = 0u;
bool launch_flag = true;

const char *sec2str(nsapi_security_t sec)
{
    switch (sec) {
        case NSAPI_SECURITY_NONE:
            return "None";
        case NSAPI_SECURITY_WEP:
            return "WEP";
        case NSAPI_SECURITY_WPA:
            return "WPA";
        case NSAPI_SECURITY_WPA2:
            return "WPA2";
        case NSAPI_SECURITY_WPA_WPA2:
            return "WPA/WPA2";
        case NSAPI_SECURITY_UNKNOWN:
        default:
            return "Unknown";
    }
}

int scan_demo(WiFiInterface *wifi)
{
    WiFiAccessPoint *ap;

    printf("Scan:\n");

    int count = wifi->scan(NULL,0);

    if (count <= 0) {
        printf("scan() failed with return value: %d\n", count);
        return 0;
    }

    /* Limit number of network arbitrary to 15 */
    count = count < 15 ? count : 15;

    ap = new WiFiAccessPoint[count];
    count = wifi->scan(ap, count);

    if (count <= 0) {
        printf("scan() failed with return value: %d\n", count);
        return 0;
    }

    for (int i = 0; i < count; i++) {
        printf("Network: %s secured: %s BSSID: %hhX:%hhX:%hhX:%hhx:%hhx:%hhx RSSI: %hhd Ch: %hhd\n", ap[i].get_ssid(),
               sec2str(ap[i].get_security()), ap[i].get_bssid()[0], ap[i].get_bssid()[1], ap[i].get_bssid()[2],
               ap[i].get_bssid()[3], ap[i].get_bssid()[4], ap[i].get_bssid()[5], ap[i].get_rssi(), ap[i].get_channel());
    }
    printf("%d networks available.\n", count);

    delete[] ap;
    return count;
}

/*****************************************************************************
* Function Name: RunCapSenseScan()
******************************************************************************
* Summary:
*   This function starts the scan, and processes the touch status. It is
* periodically called by an event dispatcher. 
*
*****************************************************************************/
void RunCapSenseScan(void)
{
    Cy_CapSense_ScanAllWidgets(&cy_capsense_context);
    capsense_sem.wait();          
    Cy_CapSense_ProcessAllWidgets(&cy_capsense_context);
    Cy_CapSense_RunTuner(&cy_capsense_context);
    ProcessTouchStatus();     
}

/*******************************************************************************
* Function Name: InitTunerCommunication
********************************************************************************
*
* Summary:
*   This function performs the following functions:
*       - Initializes SCB block for operation in EZI2C mode.
*       - Connects EZI2C HW to the SDA and SCL pins.
*       - Sets communication data buffer to CapSense data structure.
*
*******************************************************************************/
void InitTunerCommunication(void)
{
    Cy_SCB_EZI2C_Init(CSD_COMM_HW, &CSD_COMM_config, &EZI2C_context);

    /* Initialize and enable EZI2C interrupts */
    Cy_SysInt_Init(&EZI2C_ISR_cfg, &EZI2C_InterruptHandler);
    NVIC_EnableIRQ(EZI2C_ISR_cfg.intrSrc);

    /* Set up communication data buffer to CapSense data structure to be exposed
     * to I2C master at primary slave address request.
     */
    Cy_SCB_EZI2C_SetBuffer1(CSD_COMM_HW, (uint8 *)&cy_capsense_tuner,
        sizeof(cy_capsense_tuner), sizeof(cy_capsense_tuner), &EZI2C_context);

    /* Enable EZI2C block */
    Cy_SCB_EZI2C_Enable(CSD_COMM_HW);
}


/*******************************************************************************
* Function Name: ProcessTouchStatus
********************************************************************************
*
* Summary:
*   Controls the LED status according to the status of CapSense widgets and
*   prints the status to serial terminal.
*
*******************************************************************************/
void ProcessTouchStatus(void)
{
    uint32_t currSliderPos;    
    uint32_t currBtn0Status = Cy_CapSense_IsSensorActive(CY_CAPSENSE_BUTTON0_WDGT_ID, CY_CAPSENSE_BUTTON0_SNS0_ID, &cy_capsense_context);        
    uint32_t currBtn1Status = Cy_CapSense_IsSensorActive(CY_CAPSENSE_BUTTON1_WDGT_ID, CY_CAPSENSE_BUTTON1_SNS0_ID, &cy_capsense_context);       
    cy_stc_capsense_touch_t *sldrTouch = Cy_CapSense_GetTouchInfo(CY_CAPSENSE_LINEARSLIDER0_WDGT_ID, &cy_capsense_context);

    if(currBtn0Status != prevBtn0Status)
    {
        printf("Button_0 status: %lu\r\n", currBtn0Status);
        prevBtn0Status = currBtn0Status;

        if(launch_flag == true){
            connect_to_network();
            launch_flag = false;
        }
    }
    
    if(currBtn1Status != prevBtn1Status)
    {
        printf("Button_1 status: %lu\r\n", currBtn1Status);
        prevBtn1Status = currBtn1Status;

        if(launch_flag == false){
            wifi->disconnect();
            printf("WiFi Disconnected. Connect Again!\n");
            launch_flag = true;
        }
    } 

    if (sldrTouch->numPosition == SLIDER_NUM_TOUCH)
    {       
        currSliderPos = sldrTouch->ptrPosition->x;

        if(currSliderPos != prevSliderPos)
        {
            printf("Slider position: %lu\r\n", currSliderPos);
            prevSliderPos = currSliderPos;
        }
    }

    ledRed = (currBtn0Status || currBtn1Status || (sldrTouch->numPosition == SLIDER_NUM_TOUCH)) ? LED_ON : LED_OFF;
}

/*******************************************************************************
* Function Name: EZI2C_InterruptHandler
********************************************************************************
* Summary:
*   Wrapper function for handling interrupts from EZI2C block. 
*
*******************************************************************************/
void EZI2C_InterruptHandler(void)
{
    Cy_SCB_EZI2C_Interrupt(CSD_COMM_HW, &EZI2C_context);
}

/*****************************************************************************
* Function Name: CapSense_InterruptHandler()
******************************************************************************
* Summary:
*  Wrapper function for handling interrupts from CSD block.
*
*****************************************************************************/
void CapSense_InterruptHandler(void)
{
    Cy_CapSense_InterruptHandler(CapSense_HW, &cy_capsense_context);
}

/*****************************************************************************
* Function Name: CapSenseEndOfScanCallback()
******************************************************************************
* Summary:
*  This function releases a semaphore to indicate end of a CapSense scan.
*
* Parameters:
*  cy_stc_active_scan_sns_t* : pointer to active sensor details.
*
*****************************************************************************/
void CapSenseEndOfScanCallback(cy_stc_active_scan_sns_t * ptrActiveScan)
{
    capsense_sem.release();
}

int connect_to_network(void){

    wifi = WiFiInterface::get_default_instance();
    if (!wifi) {
        printf("ERROR: No WiFiInterface found.\n");
        return -1;
    }

    int count = scan_demo(wifi);
    if (count == 0) {
        printf("No WIFI APs found - can't continue further.\n");
        return -1;
    }

    printf("\nConnecting to %s...\n", MBED_CONF_APP_WIFI_SSID);
    int ret = wifi->connect(MBED_CONF_APP_WIFI_SSID, MBED_CONF_APP_WIFI_PASSWORD, NSAPI_SECURITY_WPA_WPA2);
    if (ret != 0) {
        printf("\nConnection error: %d\n", ret);
        return -1;
    }

    printf("Success\n\n");
    printf("MAC: %s\n", wifi->get_mac_address());
    printf("IP: %s\n", wifi->get_ip_address());
    printf("Netmask: %s\n", wifi->get_netmask());
    printf("Gateway: %s\n", wifi->get_gateway());
    printf("RSSI: %d\n\n", wifi->get_rssi());

}

int main()
{
    printf("WiFi + CapSense example\n");

#ifdef MBED_MAJOR_VERSION
    printf("Mbed OS version %d.%d.%d\n\n", MBED_MAJOR_VERSION, MBED_MINOR_VERSION, MBED_PATCH_VERSION);
#endif

    InitTunerCommunication();

    /* Initialize the CSD HW block to the default state. */
    cy_status status = Cy_CapSense_Init(&cy_capsense_context);
    if(CY_RET_SUCCESS != status)
    {
        printf("CapSense initialization failed. Status code: %lu\r\n", status);
        wait(osWaitForever);
    }

    /* Initialize CapSense interrupt */
    Cy_SysInt_Init(&CapSense_ISR_cfg, &CapSense_InterruptHandler);
    NVIC_ClearPendingIRQ(CapSense_ISR_cfg.intrSrc);
    NVIC_EnableIRQ(CapSense_ISR_cfg.intrSrc);

    /* Initialize the CapSense firmware modules. */
    Cy_CapSense_Enable(&cy_capsense_context);
    Cy_CapSense_RegisterCallback(CY_CAPSENSE_END_OF_SCAN_E, CapSenseEndOfScanCallback, &cy_capsense_context);
    
    /* Create a thread to run CapSense scan periodically using an event queue
     * dispatcher.
     */
    Thread thread(osPriorityNormal, OS_STACK_SIZE, NULL, "CapSense Scan Thread");
    thread.start(callback(&queue, &EventQueue::dispatch_forever));
    queue.call_every(CAPSENSE_SCAN_PERIOD, RunCapSenseScan);

    /* Initiate scan immediately since the first call of RunCapSenseScan()
     * happens CAPSENSE_SCAN_PERIOD after the event queue dispatcher has
     * started. 
     */
    Cy_CapSense_ScanAllWidgets(&cy_capsense_context); 

    wifi = WiFiInterface::get_default_instance();
    if (!wifi) {
        printf("ERROR: No WiFiInterface found.\n");
        return -1;
    }

    printf("\nDone\n\r");
    
    printf("Application has started. Touch any CapSense button or slider.\r\n");
    wait(osWaitForever);
}
