/*
    ChibiOS/RT - Copyright (C) 2006-2013 Giovanni Di Sirio

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

        http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "ch.h"
#include "hal.h"
#include "test.h"

#include "chprintf.h"
#include "shell.h"

#include "lwipthread.h"
#include "web/web.h"

#include "ff.h"

#include "jsmn/jsmn.h"

/*Needed by json parsing mechanism */
static int jsoneq(const char *json, jsmntok_t *tok, const char *s) {
    if (tok->type == JSMN_STRING && (int) strlen(s) == tok->end - tok->start &&
            strncmp(json + tok->start, s, tok->end - tok->start) == 0) {
        return 0;
    }
    return -1;
}

/*===========================================================================*/
/* ADC related stuff.                                                        */
/*===========================================================================*/

#define ADC_GRP1_NUM_CHANNELS   2
#define ADC_GRP1_BUF_DEPTH      5000
#define DEBUG_LOG               0
#define numReadingsEMG   220
#define numReadingsCurr  220
#define initOffset    0
#define maxAdc        4095
#define initK         1.0
#define initKi        0.5
#define initKd        0.0
#define initSetPoint  150
static const int iMax = 2700;
static const int iMin = 0;
static unsigned long readings[numReadingsEMG];              // the readings from the analog input
static int ind = 0;                                      // the index of the current reading
static unsigned long totalEMG = 0;                          // the running total
static unsigned average = 0;                        // the average
static char rbuf[256];
static char tmpRead[10];
static adcsample_t samples[ADC_GRP1_NUM_CHANNELS * ADC_GRP1_BUF_DEPTH];
static Mutex mtx;
static jsmn_parser p;
static jsmntok_t t[20]; /* We expect no more than 20 tokens */
static int r;
static int tmpRange = numReadingsEMG;
static int range = numReadingsEMG;
static int tmpOffset = initOffset;
static int offset = initOffset;
static int shouldUpdate = 0;
static int testPid = 0;
static int tmpTestPid = 0;
static float k = initK;
static float tmpK = initK;
static float ki = initKi;
static float tmpKi = initKi;
static float kd= initKd;
static float tmpKd = initKd;
static unsigned setPoint = initSetPoint;
static unsigned tmpSetPoint = initSetPoint;
static unsigned pidOut = 0;
static int pidError = 0;

//low pass start
static float filterVal;       // this determines smoothness  - .0001 is max  1 is off (no smoothing)
static float smoothedVal;     // this holds the last loop value just use a unique variable for every different sensor that needs smoothin

int smooth(int data, float filterVal, int smoothedVal){


  if (filterVal > 1){      // check to make sure param's are within range
    filterVal = .99;
  }
  else if (filterVal <= 0){
    filterVal = 0;
  }

  smoothedVal = (data * (1 - filterVal)) + (smoothedVal  *  filterVal);

  return (int)smoothedVal;
}
//low pass end

static void adcerrorcallback(ADCDriver *adcp, adcerror_t err) {

  (void)adcp;
  (void)err;
}

/*
 * ADC conversion group.
 * Mode:        Linear buffer, 1 samples of 2 channel, SW triggered.
 * Channels:    IN5, IN4.
 */
static const ADCConversionGroup adcgrpcfg = {
                                              FALSE,
                                              ADC_GRP1_NUM_CHANNELS,
                                              NULL,
                                              adcerrorcallback,
                                              0,                        /* CR1 */
                                              ADC_CR2_SWSTART,          /* CR2 */
                                              ADC_SMPR2_SMP_AN5(ADC_SAMPLE_3) | ADC_SMPR2_SMP_AN4(ADC_SAMPLE_3),
                                              0,                        /* SMPR2 */
                                              ADC_SQR1_NUM_CH(ADC_GRP1_NUM_CHANNELS),
                                              0,                        /* SQR2 */
                                              ADC_SQR3_SQ2_N(ADC_CHANNEL_IN4) | ADC_SQR3_SQ1_N(ADC_CHANNEL_IN5)
};



/*===========================================================================*/
/* USB related stuff.                                                        */
/*===========================================================================*/

/*
 * Endpoints to be used for USBD1.
 */
#define USBD1_DATA_REQUEST_EP           1
#define USBD1_DATA_AVAILABLE_EP         1
#define USBD1_INTERRUPT_REQUEST_EP      2

/*
 * Serial over USB Driver structure.
 */
static SerialUSBDriver SDU1;

/*
 * USB Device Descriptor.
 */
static const uint8_t vcom_device_descriptor_data[18] = {
  USB_DESC_DEVICE       (0x0110,        /* bcdUSB (1.1).                    */
                         0x02,          /* bDeviceClass (CDC).              */
                         0x00,          /* bDeviceSubClass.                 */
                         0x00,          /* bDeviceProtocol.                 */
                         0x40,          /* bMaxPacketSize.                  */
                         0x0483,        /* idVendor (ST).                   */
                         0x5740,        /* idProduct.                       */
                         0x0200,        /* bcdDevice.                       */
                         1,             /* iManufacturer.                   */
                         2,             /* iProduct.                        */
                         3,             /* iSerialNumber.                   */
                         1)             /* bNumConfigurations.              */
};

/*
 * Device Descriptor wrapper.
 */
static const USBDescriptor vcom_device_descriptor = {
  sizeof vcom_device_descriptor_data,
  vcom_device_descriptor_data
};

/* Configuration Descriptor tree for a CDC.*/
static const uint8_t vcom_configuration_descriptor_data[67] = {
  /* Configuration Descriptor.*/
  USB_DESC_CONFIGURATION(67,            /* wTotalLength.                    */
                         0x02,          /* bNumInterfaces.                  */
                         0x01,          /* bConfigurationValue.             */
                         0,             /* iConfiguration.                  */
                         0xC0,          /* bmAttributes (self powered).     */
                         50),           /* bMaxPower (100mA).               */
  /* Interface Descriptor.*/
  USB_DESC_INTERFACE    (0x00,          /* bInterfaceNumber.                */
                         0x00,          /* bAlternateSetting.               */
                         0x01,          /* bNumEndpoints.                   */
                         0x02,          /* bInterfaceClass (Communications
                                           Interface Class, CDC section
                                           4.2).                            */
                         0x02,          /* bInterfaceSubClass (Abstract
                                         Control Model, CDC section 4.3).   */
                         0x01,          /* bInterfaceProtocol (AT commands,
                                           CDC section 4.4).                */
                         0),            /* iInterface.                      */
  /* Header Functional Descriptor (CDC section 5.2.3).*/
  USB_DESC_BYTE         (5),            /* bLength.                         */
  USB_DESC_BYTE         (0x24),         /* bDescriptorType (CS_INTERFACE).  */
  USB_DESC_BYTE         (0x00),         /* bDescriptorSubtype (Header
                                           Functional Descriptor.           */
  USB_DESC_BCD          (0x0110),       /* bcdCDC.                          */
  /* Call Management Functional Descriptor. */
  USB_DESC_BYTE         (5),            /* bFunctionLength.                 */
  USB_DESC_BYTE         (0x24),         /* bDescriptorType (CS_INTERFACE).  */
  USB_DESC_BYTE         (0x01),         /* bDescriptorSubtype (Call Management
                                           Functional Descriptor).          */
  USB_DESC_BYTE         (0x00),         /* bmCapabilities (D0+D1).          */
  USB_DESC_BYTE         (0x01),         /* bDataInterface.                  */
  /* ACM Functional Descriptor.*/
  USB_DESC_BYTE         (4),            /* bFunctionLength.                 */
  USB_DESC_BYTE         (0x24),         /* bDescriptorType (CS_INTERFACE).  */
  USB_DESC_BYTE         (0x02),         /* bDescriptorSubtype (Abstract
                                           Control Management Descriptor).  */
  USB_DESC_BYTE         (0x02),         /* bmCapabilities.                  */
  /* Union Functional Descriptor.*/
  USB_DESC_BYTE         (5),            /* bFunctionLength.                 */
  USB_DESC_BYTE         (0x24),         /* bDescriptorType (CS_INTERFACE).  */
  USB_DESC_BYTE         (0x06),         /* bDescriptorSubtype (Union
                                           Functional Descriptor).          */
  USB_DESC_BYTE         (0x00),         /* bMasterInterface (Communication
                                           Class Interface).                */
  USB_DESC_BYTE         (0x01),         /* bSlaveInterface0 (Data Class
                                           Interface).                      */
  /* Endpoint 2 Descriptor.*/
  USB_DESC_ENDPOINT     (USBD1_INTERRUPT_REQUEST_EP|0x80,
                         0x03,          /* bmAttributes (Interrupt).        */
                         0x0008,        /* wMaxPacketSize.                  */
                         0xFF),         /* bInterval.                       */
  /* Interface Descriptor.*/
  USB_DESC_INTERFACE    (0x01,          /* bInterfaceNumber.                */
                         0x00,          /* bAlternateSetting.               */
                         0x02,          /* bNumEndpoints.                   */
                         0x0A,          /* bInterfaceClass (Data Class
                                           Interface, CDC section 4.5).     */
                         0x00,          /* bInterfaceSubClass (CDC section
                                           4.6).                            */
                         0x00,          /* bInterfaceProtocol (CDC section
                                           4.7).                            */
                         0x00),         /* iInterface.                      */
  /* Endpoint 3 Descriptor.*/
  USB_DESC_ENDPOINT     (USBD1_DATA_AVAILABLE_EP,       /* bEndpointAddress.*/
                         0x02,          /* bmAttributes (Bulk).             */
                         0x0040,        /* wMaxPacketSize.                  */
                         0x00),         /* bInterval.                       */
  /* Endpoint 1 Descriptor.*/
  USB_DESC_ENDPOINT     (USBD1_DATA_REQUEST_EP|0x80,    /* bEndpointAddress.*/
                         0x02,          /* bmAttributes (Bulk).             */
                         0x0040,        /* wMaxPacketSize.                  */
                         0x00)          /* bInterval.                       */
};

/*
 * Configuration Descriptor wrapper.
 */
static const USBDescriptor vcom_configuration_descriptor = {
  sizeof vcom_configuration_descriptor_data,
  vcom_configuration_descriptor_data
};

/*
 * U.S. English language identifier.
 */
static const uint8_t vcom_string0[] = {
  USB_DESC_BYTE(4),                     /* bLength.                         */
  USB_DESC_BYTE(USB_DESCRIPTOR_STRING), /* bDescriptorType.                 */
  USB_DESC_WORD(0x0409)                 /* wLANGID (U.S. English).          */
};

/*
 * Vendor string.
 */
static const uint8_t vcom_string1[] = {
  USB_DESC_BYTE(38),                    /* bLength.                         */
  USB_DESC_BYTE(USB_DESCRIPTOR_STRING), /* bDescriptorType.                 */
  'S', 0, 'T', 0, 'M', 0, 'i', 0, 'c', 0, 'r', 0, 'o', 0, 'e', 0,
  'l', 0, 'e', 0, 'c', 0, 't', 0, 'r', 0, 'o', 0, 'n', 0, 'i', 0,
  'c', 0, 's', 0
};

/*
 * Device Description string.
 */
static const uint8_t vcom_string2[] = {
  USB_DESC_BYTE(56),                    /* bLength.                         */
  USB_DESC_BYTE(USB_DESCRIPTOR_STRING), /* bDescriptorType.                 */
  'C', 0, 'h', 0, 'i', 0, 'b', 0, 'i', 0, 'O', 0, 'S', 0, '/', 0,
  'R', 0, 'T', 0, ' ', 0, 'V', 0, 'i', 0, 'r', 0, 't', 0, 'u', 0,
  'a', 0, 'l', 0, ' ', 0, 'C', 0, 'O', 0, 'M', 0, ' ', 0, 'P', 0,
  'o', 0, 'r', 0, 't', 0
};

/*
 * Serial Number string.
 */
static const uint8_t vcom_string3[] = {
  USB_DESC_BYTE(8),                     /* bLength.                         */
  USB_DESC_BYTE(USB_DESCRIPTOR_STRING), /* bDescriptorType.                 */
  '0' + CH_KERNEL_MAJOR, 0,
  '0' + CH_KERNEL_MINOR, 0,
  '0' + CH_KERNEL_PATCH, 0
};

/*
 * Strings wrappers array.
 */
static const USBDescriptor vcom_strings[] = {
  {sizeof vcom_string0, vcom_string0},
  {sizeof vcom_string1, vcom_string1},
  {sizeof vcom_string2, vcom_string2},
  {sizeof vcom_string3, vcom_string3}
};

/*
 * Handles the GET_DESCRIPTOR callback. All required descriptors must be
 * handled here.
 */
static const USBDescriptor *get_descriptor(USBDriver *usbp,
                                           uint8_t dtype,
                                           uint8_t dindex,
                                           uint16_t lang) {

  (void)usbp;
  (void)lang;
  switch (dtype) {
  case USB_DESCRIPTOR_DEVICE:
    return &vcom_device_descriptor;
  case USB_DESCRIPTOR_CONFIGURATION:
    return &vcom_configuration_descriptor;
  case USB_DESCRIPTOR_STRING:
    if (dindex < 4)
      return &vcom_strings[dindex];
  }
  return NULL;
}

/**
 * @brief   IN EP1 state.
 */
static USBInEndpointState ep1instate;

/**
 * @brief   OUT EP1 state.
 */
static USBOutEndpointState ep1outstate;

/**
 * @brief   EP1 initialization structure (both IN and OUT).
 */
static const USBEndpointConfig ep1config = {
                                            USB_EP_MODE_TYPE_BULK,
                                            NULL,
                                            sduDataTransmitted,
                                            sduDataReceived,
                                            0x0040,
                                            0x0040,
                                            &ep1instate,
                                            &ep1outstate,
                                            2,
                                            NULL
};

/**
 * @brief   IN EP2 state.
 */
static USBInEndpointState ep2instate;

/**
 * @brief   EP2 initialization structure (IN only).
 */
static const USBEndpointConfig ep2config = {
                                            USB_EP_MODE_TYPE_INTR,
                                            NULL,
                                            sduInterruptTransmitted,
                                            NULL,
                                            0x0010,
                                            0x0000,
                                            &ep2instate,
                                            NULL,
                                            1,
                                            NULL
};

/*
 * Handles the USB driver global events.
 */
static void usb_event(USBDriver *usbp, usbevent_t event) {

  switch (event) {
  case USB_EVENT_RESET:
    return;
  case USB_EVENT_ADDRESS:
    return;
  case USB_EVENT_CONFIGURED:
    chSysLockFromIsr();

    /* Enables the endpoints specified into the configuration.
       Note, this callback is invoked from an ISR so I-Class functions
       must be used.*/
    usbInitEndpointI(usbp, USBD1_DATA_REQUEST_EP, &ep1config);
    usbInitEndpointI(usbp, USBD1_INTERRUPT_REQUEST_EP, &ep2config);

    /* Resetting the state of the CDC subsystem.*/
    sduConfigureHookI(&SDU1);

    chSysUnlockFromIsr();
    return;
  case USB_EVENT_SUSPEND:
    return;
  case USB_EVENT_WAKEUP:
    return;
  case USB_EVENT_STALLED:
    return;
  }
  return;
}

/*
 * USB driver configuration.
 */
static const USBConfig usbcfg = {
                                 usb_event,
                                 get_descriptor,
                                 sduRequestsHook,
                                 NULL
};

/*
 * Serial over USB driver configuration.
 */
static const SerialUSBConfig serusbcfg = {
                                          &USBD1,
                                          USBD1_DATA_REQUEST_EP,
                                          USBD1_DATA_AVAILABLE_EP,
                                          USBD1_INTERRUPT_REQUEST_EP
};

/* PWM related stuff */

static PWMConfig pwmcfg = {
  40000000,                                    /* PWM clock frequency.   */
  4000,                                      /* Initial PWM period.     */
  NULL,
  {
   {PWM_OUTPUT_DISABLED, NULL},
   {PWM_OUTPUT_ACTIVE_HIGH, NULL},
   {PWM_OUTPUT_DISABLED, NULL},
   {PWM_OUTPUT_DISABLED, NULL}
  },
  0,
  0
};

static int computePID(unsigned setpoint, adcsample_t currentFeedback) {
  int error = setpoint - currentFeedback;
  pidError = error;
  static float iTerm = 0.0;
  float dTerm = 0.0;
  static int prevError = 0;
  float pTerm = k * (float) error;
  int out = 0;
  iTerm += ki * (float) error;
  //chprintf(&SDU1, "setpoint = %u\n", setpoint);
  /*Anti Wind-up*/
  if (iTerm > iMax) {
    iTerm = iMax;
  } else if (iTerm < iMin) {
    iTerm = iMin;
  }

  dTerm = kd * (error - prevError);
  prevError = error;
  //chprintf(&SDU1, "error = %d, pterm = %f, iterm = %f, dterm = %f\n", error, pTerm, iTerm, dTerm);
  out = (int) (pTerm + iTerm + dTerm);
  //chprintf(&SDU1, "first out %d\n", out);
  if(out > iMax) {
    out = iMax;
  } else if(out < iMin) {
    out = 0;
  }
  //chprintf(&SDU1, "total out %d\n", out);
  return out;
}


/*===========================================================================*/
/* Main and generic code.                                                    */
/*===========================================================================*/

/*
 * Reader Thread, times are in milliseconds.
 */
static WORKING_AREA(waThread1, 512);
static msg_t Thread1(void *arg) {
  int i;
  (void)arg;
  chRegSetThreadName("Reader Thread");
  while (TRUE) {
    chnReadTimeout(&SDU1, rbuf, 256, TIME_IMMEDIATE);
    if(rbuf[0] != (char) 0) {
      /* JSON parser initialization */
      jsmn_init(&p);
      r = jsmn_parse(&p, rbuf, strlen(rbuf), t, sizeof(t)/sizeof(t[0]));
      /* Loop over all keys of the root object */
      for (i = 1; i < r; i++) {
        if (jsoneq(rbuf, &t[i], "aRange") == 0) {
          int tmp;
          strncpy(tmpRead, rbuf + t[i+1].start, t[i+1].end - t[i+1].start);
          tmpRead[9] = '\0';
          tmp =  atoi(tmpRead);
          if(tmp <= numReadingsEMG) {
            chMtxLock(&mtx);
            tmpRange = atoi(tmpRead);
            shouldUpdate = 1;
            chMtxUnlock();
          }
#if DEBUG_LOG
          chprintf(&SDU1, "DEBUG: update range value %d\n\r", tmpRange);
#endif
          memset(tmpRead, 0, 10 * sizeof(char));
          i++;
        } else if(jsoneq(rbuf, &t[i], "aOffset") == 0) {
          int tmp;
          strncpy(tmpRead, rbuf + t[i+1].start, t[i+1].end - t[i+1].start);
          tmpRead[9] = '\0';
          tmp =  atoi(tmpRead);
          if(tmp <= maxAdc) {
            chMtxLock(&mtx);
            tmpOffset = atoi(tmpRead);
            shouldUpdate = 1;
            chMtxUnlock();
          }
#if DEBUG_LOG
          chprintf(&SDU1, "DEBUG: update offset value %d\n\r", tmpOffset);
#endif
          memset(tmpRead, 0, 10 * sizeof(char));
          i++;
        } else if(jsoneq(rbuf, &t[i], "k") == 0) {
          int tmp;
          strncpy(tmpRead, rbuf + t[i+1].start, t[i+1].end - t[i+1].start);
          tmpRead[9] = '\0';
          tmp =  atof(tmpRead);
          if(tmp <= maxAdc) {
            chMtxLock(&mtx);
            tmpK = atoi(tmpRead);
            shouldUpdate = 1;
            chMtxUnlock();
          }
#if DEBUG_LOG
          chprintf(&SDU1, "DEBUG: update K value %d\n\r", tmpK);
#endif
          memset(tmpRead, 0, 10 * sizeof(char));
          i++;
        } else if(jsoneq(rbuf, &t[i], "ki") == 0) {
          int tmp;
          strncpy(tmpRead, rbuf + t[i+1].start, t[i+1].end - t[i+1].start);
          tmpRead[9] = '\0';
          tmp =  atof(tmpRead);
          if(tmp <= maxAdc) {
            chMtxLock(&mtx);
            tmpKi = atoi(tmpRead);
            shouldUpdate = 1;
            chMtxUnlock();
          }
#if DEBUG_LOG
          chprintf(&SDU1, "DEBUG: update Ki value %d\n\r", tmpK);
#endif
          memset(tmpRead, 0, 10 * sizeof(char));
          i++;
        } else if(jsoneq(rbuf, &t[i], "kd") == 0) {
          int tmp;
          strncpy(tmpRead, rbuf + t[i+1].start, t[i+1].end - t[i+1].start);
          tmpRead[9] = '\0';
          tmp =  atof(tmpRead);
          if(tmp <= maxAdc) {
            chMtxLock(&mtx);
            tmpKd = atoi(tmpRead);
            shouldUpdate = 1;
            chMtxUnlock();
          }
#if DEBUG_LOG
          chprintf(&SDU1, "DEBUG: update Kd value %d\n\r", tmpK);
#endif
          memset(tmpRead, 0, 10 * sizeof(char));
          i++;
        } else if(jsoneq(rbuf, &t[i], "setPoint") == 0) {
          int tmp;
          strncpy(tmpRead, rbuf + t[i+1].start, t[i+1].end - t[i+1].start);
          tmpRead[9] = '\0';
          tmp =  atoi(tmpRead);
          if(tmp <= maxAdc) {
            chMtxLock(&mtx);
            tmpSetPoint = atoi(tmpRead);
            shouldUpdate = 1;
            chMtxUnlock();
          }
#if DEBUG_LOG
          chprintf(&SDU1, "DEBUG: update set point value %d\n\r", tmpK);
#endif
          memset(tmpRead, 0, 10 * sizeof(char));
          i++;
        } else if(jsoneq(rbuf, &t[i], "testPid") == 0) {
          int tmp;
          strncpy(tmpRead, rbuf + t[i+1].start, t[i+1].end - t[i+1].start);
          tmpRead[9] = '\0';
          tmp =  atoi(tmpRead);
          if(tmp == 1) {
            chMtxLock(&mtx);
            tmpTestPid = 1;
            chMtxUnlock();
          } else {
            chMtxLock(&mtx);
            tmpTestPid = 0;
            chMtxUnlock();
          }
#if DEBUG_LOG
          chprintf(&SDU1, "DEBUG: update testPid value %d\n\r", tmpK);
#endif
          memset(tmpRead, 0, 10 * sizeof(char));
          i++;
        }
      }
      memset(rbuf, 0, 256 * sizeof(char));
    }
    chThdSleepMilliseconds(100);
  }
  return 0;
}

/*
 * Engine Thread, times are in milliseconds. It is started with higher priority
 */
static WORKING_AREA(waThread2, 256);
static msg_t Thread2(void *arg) {

  (void)arg;
  chRegSetThreadName("Engine Thread");
  while (TRUE) {
    if ((SDU1.config->usbp->state == USB_ACTIVE)){
      int temp = 0;
      // subtract the last reading:
      //totalEMG= totalEMG - readings[ind];
      // read from the sensor:
      adcConvert(&ADCD3, &adcgrpcfg, samples, ADC_GRP1_BUF_DEPTH);

      temp = samples[0] - 1850;

      temp = temp >= 0 ? temp : -(temp);

      //readings[ind] = temp;
      // add the reading to the total:
      //totalEMG= totalEMG + readings[ind];
      // advance to the next position in the array:
      //ind = ind + 1;

      // if we're at the end of the array...
     //if (ind >= range)
        // ...wrap around to the beginning:
       // ind = 0;

      // calculate the average:
      //average = totalEMG / range;
      //chprintf(&SDU1, "{\"averageRange\" : 0, \"filteredOut\" : %u, \"notFilteredOut\" : %d, \"PIDOut\" : %u, \"PIDError\" : 0, \"DCCurrent\" : %u}\n\r", average, temp, pidOut, samples[1]);
      //chprintf(&SDU1, "{\"}%u\n\r", average);
      //compute low pass
      average = smooth(temp, 0.99, average);
      if((int)(average - offset) >= 0) {
        average -= offset;
      } else {
        average = 0;
      }

      int k;
      unsigned long max = 0;
      max = samples[1];

      for(k = 1; k < 10000; k += 2) {
      if(samples[k] > max) {
        max = samples[k];
      }
      }

      //chprintf(&SDU1, "%u\n\r", max);
      if(chMtxTryLock(&mtx) == TRUE) {
        if(shouldUpdate) {
          totalEMG = 0;
          average = 0;
          range = tmpRange;
          offset = tmpOffset;
          k = tmpK;
          ki = tmpKi;
          kd = tmpKd;
          setPoint = tmpSetPoint;
          testPid = tmpTestPid;
          memset(readings, 0, numReadingsEMG * sizeof(long));
          shouldUpdate = 0;
        }

        chMtxUnlock();
      }
#if DEBUG_LOG
      chprintf(&SDU1, "Range: %d\n\r", range);
#endif
#if 0
      if(average < 0) {
        average = 0;
      } else {
        average -= 500;
      }
#endif
      if(1) {
        pidOut = computePID(setPoint, max);
        pwmEnableChannel(&PWMD3, 1, pidOut);
      }
      chprintf(&SDU1, "{\"averageRange\" : 0, \"filteredOut\" : %u, \"notFilteredOut\" : %d, \"PIDOut\" : %u, \"PIDError\" : %d, \"DCCurrent\" : %u}\n\r", 3 * average, temp, pidOut, pidError, max/*samples[1]*/);
    }
    chEvtWaitOneTimeout(ALL_EVENTS, MS2ST(10));
  }
  return 0;
}

/*
 * Application entry point.
 */
int main(void) {
  /*
   * System initializations.
   * - HAL initialization, this also initializes the configured device drivers
   *   and performs the board-specific initializations.
   * - Kernel initialization, the main() function becomes a thread and the
   *   RTOS is active.
   */
  halInit();
  chSysInit();

  /*
   * Initializes a serial-over-USB CDC driver.
   */
  sduObjectInit(&SDU1);
  sduStart(&SDU1, &serusbcfg);

  /*
   * Activates the USB driver and then the USB bus pull-up on D+.
   * Note, a delay is inserted in order to not have to disconnect the cable
   * after a reset.
   */
  usbDisconnectBus(serusbcfg.usbp);
  chThdSleepMilliseconds(1500);
  usbStart(serusbcfg.usbp, &usbcfg);
  usbConnectBus(serusbcfg.usbp);

  /*
   * Activates the serial driver 6 and SDC driver 1 using default
   * configuration.
   */
  sdStart(&SD6, NULL);
  sdcStart(&SDCD1, NULL);

  /* ADC inputs */
  palSetPadMode(GPIOF, 7, PAL_MODE_INPUT_ANALOG);
  palSetPadMode(GPIOF, 6, PAL_MODE_INPUT_ANALOG);

  /*
   * Activates the ADC1 driver and the temperature sensor.
   */
  adcStart(&ADCD3, NULL);
  adcSTM32EnableTSVREFE();

  /*
   * Linear conversion.
   */
  adcConvert(&ADCD3, &adcgrpcfg, samples, ADC_GRP1_BUF_DEPTH);
  chThdSleepMilliseconds(1000);

  /* PWM configuration, DC dirs, breaks */
  palSetPadMode(GPIOB, 5, PAL_MODE_ALTERNATE(2));  //PWM out
  palSetPadMode(GPIOA, 5, PAL_MODE_OUTPUT_PUSHPULL);  //dir
  palSetPad(GPIOA, 5);
  palSetPadMode(GPIOG, 12, PAL_MODE_OUTPUT_PUSHPULL); //break
  palClearPad(GPIOG, 12);
  pwmStart(&PWMD3, &pwmcfg);


  /* Mutex initialization */
  chMtxInit(&mtx);

  /*
   * Creates threads.
   */
  chThdCreateStatic(waThread1, sizeof(waThread1), NORMALPRIO, Thread1, NULL); //Read Thread

  chThdCreateStatic(waThread2, sizeof(waThread2), HIGHPRIO, Thread2, NULL);   //Engine Thread

  /*
   * Normal main() thread activity just blinking led.
   */
  while (TRUE) {
    palTogglePad(GPIOC, GPIOC_LED);
    chThdSleepMilliseconds(500);
  }
}
