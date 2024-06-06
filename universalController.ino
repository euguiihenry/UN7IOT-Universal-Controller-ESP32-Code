/* Includes Made Before Definitions:
================================================================================*/
  #include <Arduino.h>
  #include "PinDefinitionsAndMore.h"
  #include <IRremote.hpp>
  #include <WiFiManager.h>
  #include <iostream>

/* Defines:
================================================================================*/
  /* Env Defines:
  ============================================================================*/
    #include "env.h"

  /* Universal Controller Defines:
  ============================================================================*/
    #define RAW_BUFFER_LENGTH  750
    #define MARK_EXCESS_MICROS  20 // 20 is recommended for the cheap VS1838 modules

  /* Board Defines:
  ============================================================================*/
    #define pinLED 27

  /* IR Pin Definitions:
  ============================================================================*/
    #define IR_RECEIVE_PIN 15 // Replace with the actual pin connected to the IR receiver
    #define IR_SEND_PIN 14    // Replace with the actual pin connected to the IR sender

/* Include Made After the Declarations:
================================================================================*/
  #include <Blynk.h>
  #include <BlynkSimpleEsp32.h>

/* Structs & Variables:
================================================================================*/
  /* Structs:
  ============================================================================*/
    struct storedIRDataStruct {
        IRData receivedIRData;  // extensions for sendRaw
        uint8_t rawCode[RAW_BUFFER_LENGTH]; // The durations if raw
        uint16_t rawCodeLength; // The length of the code
    } sStoredIRData[10];

  /* Global Variables:
  ============================================================================*/
    int DELAY_BETWEEN_REPEAT = 50;
    int DEFAULT_NUMBER_OF_REPEATS_TO_SEND = 1;
    int estadoAnt = 0; // VARIÁVEL PARA CONTROLAR A TROCA DE MODO DE OPERAÇÃO
    int commandPosition = (-1);
    String setMode = "Any";
    String statusModeSet = "Any";
    int estadoAtual = 0; // Current state for recording or sending

/* Prototypes:
================================================================================*/
  void checkIR();
  void storeCode(IRData *aIRReceivedData, int index);
  void sendCode(storedIRDataStruct *aIRDataToSend);
  void storeCodeOnline();
  void wifiConnection();
  void blynkConnection();
  void gettingStatusMode();
  void disableReader();
  void disableSender();
  void resettingCommandPosition();

/* Setup Function:
================================================================================*/
  void setup() {
    /* Setting Led Builtin Feedback:
    ==========================================================================*/
      pinMode(LED_BUILTIN, OUTPUT);

    /* Setting Serial Monitor Frequency:
    ==========================================================================*/
      Serial.begin(115200);

    /* Calling Essential Methods for First Load:
    ==========================================================================*/
      wifiConnection();
      blynkConnection();

      Serial.println(IR_SEND_PIN);    // Shows in which pin is sender set to:
      Serial.println(IR_RECEIVE_PIN); // Shows in which pin is receiver set to:

      IrReceiver.begin(IR_RECEIVE_PIN, ENABLE_LED_FEEDBACK);
      IrSender.begin();

    /* Printing Instructions for Serial Monitor Use:
    ==========================================================================*/
      Serial.println(F("Para gravar digite '#' seguido da posicao entre 0 e 9"));
      Serial.println(F("Para enviar digite o numero entre 0 e 9 de um codigo ja gravado"));
  }

/* Loop Function:
================================================================================*/
  void loop() {
      Blynk.run();
      checkIR();
  }

/* Check IR Status Function:
================================================================================*/
  void checkIR() {
    // INICIA GRAVAÇÃO DE COMANDO
    if (estadoAtual != estadoAnt && estadoAtual == 1) {
        IrReceiver.start();
    }

    // ENVIA COMANDO
    if (estadoAtual == 2 && commandPosition >= 0 && commandPosition < 10) {
        IrReceiver.stop();

        Serial.println(F("Enviando..."));
        sendCode(&sStoredIRData[commandPosition]);

        commandPosition = -1; // commandPosition < 0 PARA NÃO REPETIR O ENVIO
    }

    // GRAVA COMANDO
    if (estadoAtual == 1 && IrReceiver.available() && commandPosition >= 0 && commandPosition < 10) {
        storeCode(IrReceiver.read(), commandPosition);
        IrReceiver.resume(); // resume receiver
    }

    estadoAnt = estadoAtual;
  }

/* Store Code Function:
================================================================================*/
  void storeCode(IRData *aIRReceivedData, int index) {
      if (aIRReceivedData->flags & IRDATA_FLAGS_IS_REPEAT) {
          Serial.println(F("Ignore repeat"));
          return;
      }
      if (aIRReceivedData->flags & IRDATA_FLAGS_IS_AUTO_REPEAT) {
          Serial.println(F("Ignore autorepeat"));
          return;
      }
      if (aIRReceivedData->flags & IRDATA_FLAGS_PARITY_FAILED) {
          Serial.println(F("Ignore parity error"));
          return;
      }
      /*
      * Copy decoded data
      */
      sStoredIRData[index].receivedIRData = *aIRReceivedData;

      if (sStoredIRData[index].receivedIRData.protocol == UNKNOWN) {
          Serial.print(F("Received unknown code and store "));
          Serial.print(IrReceiver.decodedIRData.rawDataPtr->rawlen - 1);
          Serial.println(F(" timing entries as raw "));
          IrReceiver.printIRResultRawFormatted(&Serial, true); // Output the results in RAW format
          sStoredIRData[index].rawCodeLength = IrReceiver.decodedIRData.rawDataPtr->rawlen - 1;
          /*
          * Store the current raw data in a dedicated array for later usage
          */
          IrReceiver.compensateAndStoreIRResultInArray(sStoredIRData[index].rawCode);
      } else {
          IrReceiver.printIRResultShort(&Serial);
          IrReceiver.printIRSendUsage(&Serial);
          sStoredIRData[index].receivedIRData.flags = 0; // clear flags -esp. repeat- for later sending
          Serial.println();
      }
  }

/* Send Code Function:
================================================================================*/
  void sendCode(storedIRDataStruct *aIRDataToSend) {
      if (aIRDataToSend->receivedIRData.protocol == UNKNOWN /* i.e. raw */) {
          // Assume 38 KHz
          IrSender.sendRaw(aIRDataToSend->rawCode, aIRDataToSend->rawCodeLength, 38);

          Serial.print(F("Sent raw "));
          Serial.print(aIRDataToSend->rawCodeLength);
          Serial.println(F(" marks or spaces"));
      } else {

          /*
          * Use the write function, which does the switch for different protocols
          */
          IrSender.write(&aIRDataToSend->receivedIRData, DEFAULT_NUMBER_OF_REPEATS_TO_SEND);

          Serial.print(F("Sent: "));
          printIRResultShort(&Serial, &aIRDataToSend->receivedIRData, false);
      }
  }

/* Wi-Fi Connection:
================================================================================*/
  void wifiConnection() {
    WiFiManager wm;
    bool res;
    res = wm.autoConnect("esp32_reset","bettergoodluck");

    if(!res) {
          Serial.println("Failed to connect");
          ESP.restart();
    } 
    else {
        //if you get here you have connected to the WiFi    
        Serial.println("connected...yeey :)");
    }
  }

/* Blynk Functions:
================================================================================*/
  /* Blynk Connection:
  ============================================================================*/
    void blynkConnection() {
        Blynk.begin(BLYNK_AUTH_TOKEN, WIFI_SSID, WIFI_PASS);
    }

  /* Reading Value to Set Reader Mode On:
  ============================================================================*/
    BLYNK_WRITE(V0) {
        int v0_value = param.asInt();

        if (v0_value == 1 && setMode != "Sender") {
            setMode = "Reader";
            statusModeSet = "Recording";
            estadoAtual = 1;
            Serial.println("Reader mode enabled via Blynk");
        } else {
            disableReader();
            resettingCommandPosition();
        }
        gettingStatusMode();
    }

  /* Reading Value to Set Sender Mode On:
  ============================================================================*/
    BLYNK_WRITE(V1) {
        int v1_value = param.asInt();

        if (v1_value == 1 && setMode != "Reader") {
            setMode = "Sender";
            statusModeSet = "Sender Mode";
            estadoAtual = 2;
            Serial.println("Sender mode enabled via Blynk");
        } else {
            disableSender();
            resettingCommandPosition();
        }
        gettingStatusMode();
    }

  /* Getting Mode Status to Show on App:
  ============================================================================*/
    void gettingStatusMode() {
        Serial.println(statusModeSet);
        Blynk.virtualWrite(V2, statusModeSet);
    }

  /* Disabling Reader Switcher:
  ============================================================================*/
    void disableReader() {
        Serial.println("Disabling Reader Mode Switcher");
        Blynk.virtualWrite(V0, 0);
        setMode = "Any";
        statusModeSet = "Any";
        estadoAtual = 0;
        IrReceiver.stop();
    }

  /* Disabling Sender Switcher:
  ============================================================================*/
    void disableSender() {
        Serial.println("Disabling Sender Mode Switcher");
        Blynk.virtualWrite(V1, 0);
        setMode = "Any";
        statusModeSet = "Any";
        estadoAtual = 0;
    }

  /* Getting Command Position:
  ============================================================================*/
    BLYNK_WRITE(V3) {
        commandPosition = param.asInt();
        Serial.println(commandPosition);
    }

  /* Setting Command Position:
  ============================================================================*/
    void resettingCommandPosition() {
        Serial.println("Setting Command Position to -1 in Blynk Cloud!");
        commandPosition = -1;
        Blynk.virtualWrite(V3, commandPosition);
    }